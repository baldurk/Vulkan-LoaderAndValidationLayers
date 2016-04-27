/* Copyright (c) 2015-2016 The Khronos Group Inc.
 * Copyright (c) 2015-2016 Valve Corporation
 * Copyright (c) 2015-2016 LunarG, Inc.
 * Copyright (C) 2015-2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Tobin Ehlis <tobine@google.com>
 */

#include "descriptor_sets.h"
#include "vk_enum_string_helper.h"
#include <sstream>

cvdescriptorset::DescriptorSetLayout::DescriptorSetLayout()
    : layout_(VK_NULL_HANDLE), flags_(0), binding_count_(0), descriptor_count_(0), dynamic_descriptor_count_(0) {}
// Construct DescriptorSetLayout instance from given create info
cvdescriptorset::DescriptorSetLayout::DescriptorSetLayout(debug_report_data *report_data, const VkDescriptorSetLayoutCreateInfo *p_create_info,
                                         const VkDescriptorSetLayout layout)
    : layout_(layout), flags_(p_create_info->flags), binding_count_(p_create_info->bindingCount), descriptor_count_(0),
      dynamic_descriptor_count_(0) {
    uint32_t global_index = 0;
    for (uint32_t i = 0; i < binding_count_; ++i) {
        descriptor_count_ += p_create_info->pBindings[i].descriptorCount;
        if (!binding_to_index_map_.emplace(p_create_info->pBindings[i].binding, i).second) {
            log_msg(report_data, VK_DEBUG_REPORT_ERROR_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT,
                    reinterpret_cast<uint64_t &>(layout_), __LINE__, DRAWSTATE_INVALID_LAYOUT, "DS",
                    "duplicated binding number in "
                    "VkDescriptorSetLayoutBinding");
        }
        binding_to_global_start_index_map_[p_create_info->pBindings[i].binding] = global_index;
        global_index += p_create_info->pBindings[i].descriptorCount ? p_create_info->pBindings[i].descriptorCount - 1 : 0;
        binding_to_global_end_index_map_[p_create_info->pBindings[i].binding] = global_index;
        global_index++;
        bindings_.push_back(new safe_VkDescriptorSetLayoutBinding(&p_create_info->pBindings[i]));
        // In cases where we should ignore pImmutableSamplers make sure it's NULL
        if ((p_create_info->pBindings[i].pImmutableSamplers) &&
            ((p_create_info->pBindings[i].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER) &&
             (p_create_info->pBindings[i].descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))) {
            bindings_.back()->pImmutableSamplers = nullptr;
        }
        if (p_create_info->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            p_create_info->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
            dynamic_descriptor_count_++;
        }
    }
}
cvdescriptorset::DescriptorSetLayout::~DescriptorSetLayout() {
    for (auto binding : bindings_)
        delete binding;
}
VkDescriptorSetLayoutBinding const *cvdescriptorset::DescriptorSetLayout::GetDescriptorSetLayoutBindingPtrFromBinding(const uint32_t binding) {
    if (!binding_to_index_map_.count(binding))
        return nullptr;
    return bindings_[binding_to_index_map_[binding]]->ptr();
}
VkDescriptorSetLayoutBinding const *cvdescriptorset::DescriptorSetLayout::GetDescriptorSetLayoutBindingPtrFromIndex(const uint32_t index) {
    if (index >= bindings_.size())
        return nullptr;
    return bindings_[index]->ptr();
}
// Return descriptorCount for given binding, 0 if index is unavailable
uint32_t DescriptorSetLayout::GetDescriptorCountFromBinding(const uint32_t binding) {
    if (!binding_to_index_map_.count(binding))
        return 0;
    return bindings_[binding_to_index_map_[binding]]->descriptorCount;
}
// Return descriptorCount for given index, 0 if index is unavailable
uint32_t DescriptorSetLayout::GetDescriptorCountFromIndex(const uint32_t index) {
    if (index >= bindings_.size())
        return 0;
    return bindings_[index]->descriptorCount;
}
// For the given binding, return descriptorType
VkDescriptorType DescriptorSetLayout::GetTypeFromBinding(const uint32_t binding) {
    assert(binding_to_index_map_.count(binding));
    return bindings_[binding_to_index_map_[binding]]->descriptorType;
}
// For the given index, return descriptorType
VkDescriptorType DescriptorSetLayout::GetTypeFromIndex(const uint32_t index) {
    assert(index < bindings_.size());
    return bindings_[index]->descriptorType;
}
// For the given global index, return descriptorType
//  Currently just counting up through bindings_, may improve this in future
VkDescriptorType DescriptorSetLayout::GetTypeFromGlobalIndex(const uint32_t index) {
    auto global_offset = 0;
    for (auto binding : bindings_) {
        global_offset += binding->descriptorCount;
        if (index < global_offset)
            return binding->descriptorType;
    }
    assert(0); // requested global index is out of bounds
}
// For the given binding, return stageFlags
VkShaderStageFlags DescriptorSetLayout::GetStageFlagsFromBinding(const uint32_t binding) {
    assert(binding_to_index_map_.count(binding));
    return bindings_[binding_to_index_map_[binding]]->stageFlags;
}
// For the given binding, return start index
uint32_t DescriptorSetLayout::GetGlobalStartIndexFromBinding(const uint32_t binding) {
    assert(binding_to_global_start_index_map_.count(binding));
    return binding_to_global_start_index_map_[binding];
}
// For the given binding, return end index
uint32_t DescriptorSetLayout::GetGlobalEndIndexFromBinding(const uint32_t binding) {
    assert(binding_to_global_end_index_map_.count(binding));
    return binding_to_global_end_index_map_[binding];
}
//
VkSampler const *DescriptorSetLayout::GetImmutableSamplerPtrFromBinding(const uint32_t binding) {
    assert(binding_to_index_map_.count(binding));
    return bindings_[binding_to_index_map_[binding]]->pImmutableSamplers;
}
//
VkSampler const *cvdescriptorset::DescriptorSetLayout::GetImmutableSamplerPtrFromIndex(const uint32_t index) {
    assert(index < bindings_.size());
    return bindings_[index]->pImmutableSamplers;
}
// If our layout is compatible with rh_sd_layout, return true,
//  else return false and fill in error_msg will description of what causes incompatibility
bool DescriptorSetLayout::IsCompatible(DescriptorSetLayout *rh_ds_layout, string *error_msg) {
    // Trivial case
    if (layout_ == rh_ds_layout->GetDescriptorSetLayout())
        return true;
    if (descriptor_count_ != rh_ds_layout->descriptor_count_) {
        stringstream error_str;
        error_str << "DescriptorSetLayout " << layout_ << " has " << descriptor_count_ << " descriptors, but DescriptorSetLayout "
                  << rh_ds_layout->GetDescriptorSetLayout() << " has " << rh_ds_layout->descriptor_count_ << " descriptors.";
        *error_msg = error_str.str();
        return false; // trivial fail case
    }
    // Descriptor counts match so need to go through bindings one-by-one
    //  and verify that type and stageFlags match
    for (auto binding : bindings_) {
        // TODO : Do we also need to check immutable samplers?
        // VkDescriptorSetLayoutBinding *rh_binding;
        // rh_ds_layout->FillDescriptorSetLayoutBindingStructFromBinding(binding->binding, rh_binding);
        if (binding->descriptorCount != rh_ds_layout->GetTotalDescriptorCount()) {
            stringstream error_str;
            error_str << "Binding " << binding->binding << " for DescriptorSetLayout " << layout_ << " has a descriptorCount of "
                      << binding->descriptorCount << " but binding " << binding->binding << " for DescriptorSetLayout "
                      << rh_ds_layout->GetDescriptorSetLayout() << " has a descriptorCount of "
                      << rh_ds_layout->GetTotalDescriptorCount();
            *error_msg = error_str.str();
            return false;
        } else if (binding->descriptorType != rh_ds_layout->GetTypeFromBinding(binding->binding)) {
            stringstream error_str;
            error_str << "Binding " << binding->binding << " for DescriptorSetLayout " << layout_ << " is type '"
                      << string_VkDescriptorType(binding->descriptorType) << "' but binding " << binding->binding
                      << " for DescriptorSetLayout " << rh_ds_layout->GetDescriptorSetLayout() << " is type '"
                      << string_VkDescriptorType(rh_ds_layout->GetTypeFromBinding(binding->binding)) << "'";
            *error_msg = error_str.str();
            return false;
        } else if (binding->stageFlags != rh_ds_layout->GetStageFlagsFromBinding(binding->binding)) {
            stringstream error_str;
            error_str << "Binding " << binding->binding << " for DescriptorSetLayout " << layout_ << " has stageFlags "
                      << binding->stageFlags << " but binding " << binding->binding << " for DescriptorSetLayout "
                      << rh_ds_layout->GetDescriptorSetLayout() << " has stageFlags "
                      << rh_ds_layout->GetStageFlagsFromBinding(binding->binding);
            *error_msg = error_str.str();
            return false;
        }
    }
    return true;
}

bool cvdescriptorset::DescriptorSetLayout::IsNextBindingConsistent(const uint32_t binding) {
    if (!binding_to_index_map_.count(binding+1))
        return false;
    auto type = bindings_[binding_to_index_map_[binding]].descriptorType;
    auto stage_flags = bindings_[binding_to_index_map_[binding]].stageFlags;
    auto immut_samp = bindings_[binding_to_index_map_[binding]].pImmutableSamplers ? true : false;
    if ((type != bindings_[binding_to_index_map_[binding+1]].descriptorType) ||
            (stage_flags != bindings_[binding_to_index_map_[binding+1]].stageFlags) ||
            (immut_samp != (bindings_[binding_to_index_map_[binding+1]].pImmutableSamplers ? true : false))) {
        return false;
    }
    return true;
}

cvdescriptorset::DescriptorSet::DescriptorSet() : some_update(false), full_update(false), set_(VK_NULL_HANDLE), descriptor_count_(0), p_layout_(nullptr) {}

cvdescriptorset::DescriptorSet::DescriptorSet(const VkDescriptorSet* set, const DescriptorSetLayout* layout) : some_update(false), full_update(false), set_(set), p_layout_(layout) {
    // Descriptors won't have data until the time they're updated so init to nullptrs for now
    descriptors_.resize(p_layout_->GetTotalDescriptorCount());
    // Foreach binding, create default descriptors of given type
    auto offset = 0; // Track offset into global descriptor array
    for (uint32_t i=0; i<p_layout_->GetBindingCount(); ++i) {
        switch (p_layout_->GetTypeFromIndex(i)) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                auto immut = p_layout_->GetImmutableSamplerPtrFromIndex(i);
                for (uint32_t di=0; di<p_layout_->GetDescriptorCountFromIndex(i); ++di) {
                    if (immut)
                        descriptors_.push_back(std::unique_ptr<Descriptor*>(new SamplerDescriptor(immut+di)));
                    else
                        descriptors_.push_back(std::unique_ptr<Descriptor*>(new SamplerDescriptor()));
                }
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                auto immut = p_layout_->GetImmutableSamplerPtrFromIndex(i);
                for (uint32_t di=0; di<p_layout_->GetDescriptorCountFromIndex(i); ++di) {
                    if (immut)
                        descriptors_.push_back(std::unique_ptr<Descriptor*>(new ImageSamplerDescriptor(immut+di)));
                    else
                        descriptors_.push_back(std::unique_ptr<Descriptor*>(new ImageSamplerDescriptor()));
                }
                break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                for (uint32_t di=0; di<p_layout_->GetDescriptorCountFromIndex(i); ++di)
                    descriptors_.push_back(std::unique_ptr<Descriptor*>(new ImageDescriptor()));
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                for (uint32_t di=0; di<p_layout_->GetDescriptorCountFromIndex(i); ++di)
                    descriptors_.push_back(std::unique_ptr<Descriptor*>(new TexelDescriptor()));
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                for (uint32_t di=0; di<p_layout_->GetDescriptorCountFromIndex(i); ++di)
                    descriptors_.push_back(std::unique_ptr<Descriptor*>(new BufferDescriptor()));
                break;
            default:
                break;
        }
    }
}
// Perform write update in given update struct
//  If an error occurs, return false and fill in details in error_msg string
bool cvdescriptorset::DescriptorSet::WriteUpdate(debug_report_data *report_data, const VkWriteDescriptorSet *update, std::string *error_msg) {
    // Verify dst binding exists
    if (!p_layout_->HasBinding(update->dstBinding)) {
        std::stringstream error_str;
        error_str << "DescriptorSet " << set_ << " does not have binding " << update->dstBinding << ".";
        *error_msg = error_str.str();
        return false;
    } else {
        // We know that binding is valid, do update on each descriptor
        auto start_index = p_layout_->GetGlobalStartIndexFromBinding(update->dstBinding) + update->dstArrayElement;
        // descriptor type, stage flags and immutable sampler must be consistent for all descriptors in a single update
        auto type = p_layout_->GetTypeFromBinding(update->dstBinding);
        auto flags = p_layout_->GetStageFlagsFromBinding(update->dstBinding);
        auto immut = p_layout_->GetImmutableSamplerPtrFromBinding(update->dstBinding) ? true : false;
        if (type != update->descriptorType) {
            // TODO : FLAG ERROR HERE DUE TO MIS_MATCHED TYPES
            return false;
        }
        if ((start_index + update->descriptorCount) > p_layout_->GetTotalDescriptorCount()) {
            // TODO : FLAG ERROR AS UPDATE OVERSTEPS BOUNDS OF LAYOUT
            return false;
        }
        // Verify consecutive bindings match (if needed)
        auto update_count = update->descriptorCount;
        auto current_binding = update->dstBinding;
        auto binding_remaining = p_layout_->GetDescriptorCountFromBinding(current_binding) - update->dstArrayElement;
        while (update_count > binding_remaining) { // While our updates overstep current binding
            // Verify next consecutive binding matches type, stage flags & immutable sampler use
            if (!p_layout_->IsNextBindingConsistent(current_binding++)) {
                // TODO : Report error
                return false;
            }
            // For sake of this check consider the bindings updated and advance to next binding
            update_count -= binding_remaining;
            binding_remaining = p_layout_->GetDescriptorCountFromBinding(current_binding);
        }
        // Update is within bounds and consistent so verify and record update contents
        for (auto di=0; di<update->descriptorCount; ++di) {
            descriptors_[start_index+di]->WriteUpdate(update, di);
        }
    }
}
// Copy update
bool cvdescriptorset::DescriptorSet::CopyUpdate(debug_report_data *report_data, const VkCopyDescriptorSet *update, const DescriptorSet* src_set, std::string *error) {
    if (!p_layout_->HasBinding(update->dstBinding)) {
        // ERROR
        return false;
    } else if (src_set->HasBinding(update->srcBinding)) {
        // ERROR
        return false;
    } else { // src & dst set bindings are valid
        // Check bounds of src & dst
        auto src_start_index = src_set->GetGlobalStartIndexFromBinding(update->srcBinding) + update->srcArrayElement;
        if ((src_start_index + update->descriptorCount) > src_set->GetTotalDescriptorCount()) {
            // SRC update out of bounds
        }
        auto dst_start_index = p_layout_->GetGlobalStartIndexFromBinding(update->dstBinding) + update->dstArrayElement;
        if ((dst_start_index + update->descriptorCount) > p_layout_->GetTotalDescriptorCount()) {
            // DST update out of bounds
        }
        // Verify consistency of src & dst bindings if update crosses binding boundaries
    }
}
// Return ptr to descriptor based on global index, or nullptr if descriptor out of bounds
Descriptor *cvdescriptorset::DescriptorSet::GetDescriptor(const uint32_t index) {
    if (index >= descriptors_.size())
        return nullptr;
    return descriptors_[index];
}
cvdescriptorset::SamplerDescriptor::SamplerDescriptor(const VkSampler* immut) : sampler(VK_NULL_HANDLE), immutable(false) {
    updated = false;
    if (immut) {
        sampler = immut;
        immutable = true;
        updated = true;
    }
}

bool cvdescriptorset::SamplerDescriptor::WriteUpdate(const VkWriteDescriptorSet* update, const uint32_t index, std::string *error) {
    updated = true;
    if (!immutable) {
        sampler = update->pImageInfo[index].sampler;
        // TODO : How/where to verify that sampler is valid
    } else {
        // TODO : Warn here?
    }
    return true;
}

bool cvdescriptorset::SamplerDescriptor::CopyUpdate(const VkCopyDescriptorSet* update, const uint32_t index, const Descriptor* src, std::string *error) {
    updated = true;
    if (!immutable) {
        sampler = dynamic_cast<SamplerDescriptor*>(src)->sampler;
    } else {
        // TODO : Warn here?
    }
    return true;
}
cvdescriptorset::ImageSamplerDescriptor::ImageSamplerDescriptor(const VkSampler *immut) : sampler(VK_NULL_HANDLE), immutable(true), image_view(VK_NULL_HANDLE), image_layout(VK_IMAGE_LAYOUT_UNDEFINED) {
    updated = false;
    if (immut) {
        sampler = immut;
        immutable = true;
        updated = true;
    }
}
bool cvdescriptorset::ImageSamplerDescriptor::WriteUpdate(const VkWriteDescriptorSet* update, const uint32_t index, std::string *error) {
    updated = true;
    if (!immutable) {
        sampler = update->pImageInfo[index].sampler;
    } else {
        // TOOD : warn here?
    }
    image_view = update->pImageInfo[index].imageView;
    image_layout = update->pImageInfo[index].imageLayout;
    return true;
}

bool cvdescriptorset::ImageSamplerDescriptor::CopyUpdate(const Descriptor* src, std::string* error) {
    updated = true;
    if (!immutable) {
        sampler = dynamic_cast<ImageSamplerDescriptor*>(src)->sampler;
    }
    image_view = dynamic_cast<ImageSamplerDescriptor*>(src)->image_view;
    image_layout = dynamic_cast<ImageSamplerDescriptor*>(src)->image_layout;
    return true;
}

bool cvdescriptorset::ImageDescriptor::WriteUpdate(const VkWriteDescriptorSet* update, const uint32_t index, std::string *error) {
    updated = true;
    image_view = update->pImageInfo[index].imageView;
    image_layout = update->pImageInfo[index].imageLayout;
    return true;
}

bool cvdescriptorset::ImageDescriptor::CopyUpdate(const Descriptor* src, std::string* error) {
    updated = true;
    image_view = dynamic_cast<ImageDescriptor*>(src)->image_view;
    image_layout = dynamic_cast<ImageDescriptor*>(src)->image_layout;
    return true;
}

bool cvdescriptorset::BufferDescriptor::WriteUpdate(const VkWriteDescriptorSet* update, const uint32_t index, std::string *error) {
    updated = true;
    buffer = update->pBufferInfo[index].buffer;
    offset = update->pBufferInfo[index].offset;
    range = update->pBufferInfo[index].range;
    return true;
}

bool cvdescriptorset::BufferDescriptor::CopyUpdate(const Descriptor* src, std::string* error) {
    updated = true;
    buffer = dynamic_cast<BufferDescriptor*>(src)->buffer;
    offset = dynamic_cast<BufferDescriptor*>(src)->offset;
    range = dynamic_cast<BufferDescriptor*>(src)->range;
    return true;
}

bool cvdescriptorset::TexelDescriptor::WriteUpdate(const VkWriteDescriptorSet* update, const uint32_t index, std::string *error) {
    updated = true;
    buffer_view = update->pTexelBufferView[index];
    return true;
}

bool cvdescriptorset::TexelDescriptor::CopyUpdate(const Descriptor* src, std::string* error) {
    updated = true;
    buffer_view = dynamic_cast<TexelDescriptor*>(src)->buffer_view;
    return true;
}