// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_user_data.h"

#include "base/memory/ptr_util.h"

namespace previews {

const void* const kPreviewsUserDataKey = &kPreviewsUserDataKey;

PreviewsUserData::PreviewsUserData(uint64_t page_id) : page_id_(page_id) {}

PreviewsUserData::~PreviewsUserData() {}

PreviewsUserData::PreviewsUserData(const PreviewsUserData& previews_user_data) =
    default;

std::unique_ptr<PreviewsUserData> PreviewsUserData::DeepCopy() const {
  // Raw new to avoid friending std::make_unique.
  return base::WrapUnique(new PreviewsUserData(*this));
}

PreviewsUserData* PreviewsUserData::GetData(
    const base::SupportsUserData& owner) {
  PreviewsUserData* data =
      static_cast<PreviewsUserData*>(owner.GetUserData(kPreviewsUserDataKey));
  return data;
}

PreviewsUserData* PreviewsUserData::Create(base::SupportsUserData* owner,
                                           uint64_t page_id) {
  if (!owner)
    return nullptr;
  PreviewsUserData* data = GetData(*owner);
  if (data)
    return data;
  data = new PreviewsUserData(page_id);
  owner->SetUserData(kPreviewsUserDataKey, base::WrapUnique(data));
  return data;
}

void PreviewsUserData::SetCommittedPreviewsType(
    previews::PreviewsType previews_type) {
  DCHECK(committed_previews_type_ == PreviewsType::NONE);
  committed_previews_type_ = previews_type;
}

}  // namespace previews
