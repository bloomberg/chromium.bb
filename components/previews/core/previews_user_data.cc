// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_user_data.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "net/url_request/url_request.h"

namespace previews {

namespace {
const void* const kPreviewsUserDataKey = &kPreviewsUserDataKey;
const char* kPageIdKey = "page_id";
const char* kCommittedPreviewsTypeKey = "committed_previews_type";
}  // namespace

PreviewsUserData::PreviewsUserData(uint64_t page_id)
    : page_id_(page_id), committed_previews_type_(PreviewsType::NONE) {}

PreviewsUserData::~PreviewsUserData() {}

base::Value PreviewsUserData::ToValue() {
  base::Value value(base::Value::Type::DICTIONARY);

  // Store |page_id_| as a string because base::Value can't store a 64 bits
  // integer.
  value.SetKey(kPageIdKey, base::Value(std::to_string(page_id_)));
  value.SetKey(kCommittedPreviewsTypeKey,
               base::Value((int)committed_previews_type_));

  return value;
}

PreviewsUserData::PreviewsUserData(const base::Value& value)
    : page_id_(std::stoll(value.FindKey(kPageIdKey)->GetString())),
      committed_previews_type_(
          (PreviewsType)value.FindKey(kCommittedPreviewsTypeKey)->GetInt()) {}

std::unique_ptr<PreviewsUserData> PreviewsUserData::DeepCopy() const {
  std::unique_ptr<PreviewsUserData> copy(new PreviewsUserData(page_id_));
  copy->SetCommittedPreviewsType(committed_previews_type_);
  return copy;
}

PreviewsUserData* PreviewsUserData::GetData(const net::URLRequest& request) {
  PreviewsUserData* data =
      static_cast<PreviewsUserData*>(request.GetUserData(kPreviewsUserDataKey));
  return data;
}

PreviewsUserData* PreviewsUserData::Create(net::URLRequest* request,
                                           uint64_t page_id) {
  if (!request)
    return nullptr;
  PreviewsUserData* data = GetData(*request);
  if (data)
    return data;
  data = new PreviewsUserData(page_id);
  request->SetUserData(kPreviewsUserDataKey, base::WrapUnique(data));
  return data;
}

void PreviewsUserData::SetCommittedPreviewsType(
    previews::PreviewsType previews_type) {
  DCHECK(committed_previews_type_ == PreviewsType::NONE);
  committed_previews_type_ = previews_type;
}

}  // namespace previews
