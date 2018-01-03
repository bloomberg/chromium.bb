// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_navigation_data.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "net/url_request/url_request.h"

namespace {
const char* kDataReductionProxyDataKey = "data_reduction_proxy_data";
const char* kPreviewsUserDataKey = "preview_user_data";
const char* kPreviewsStateKey = "preview_state";
}  // namespace

const void* const kChromeNavigationDataUserDataKey =
    &kChromeNavigationDataUserDataKey;

ChromeNavigationData::ChromeNavigationData()
    : previews_state_(content::PreviewsTypes::PREVIEWS_UNSPECIFIED) {}

ChromeNavigationData::~ChromeNavigationData() {}

base::Value ChromeNavigationData::ToValue() {
  base::Value value(base::Value::Type::DICTIONARY);
  if (data_reduction_proxy_data_) {
    value.SetKey(kDataReductionProxyDataKey,
                 data_reduction_proxy_data_->ToValue());
  }

  if (previews_user_data_) {
    value.SetKey(kPreviewsUserDataKey, previews_user_data_->ToValue());
  }

  value.SetKey(kPreviewsStateKey, base::Value(previews_state_));

  return value;
}

ChromeNavigationData::ChromeNavigationData(const base::Value& value)
    : ChromeNavigationData() {
  if (value.is_none())
    return;

  const base::Value* data_reduction_proxy_data =
      value.FindKey(kDataReductionProxyDataKey);
  if (data_reduction_proxy_data) {
    data_reduction_proxy_data_ =
        std::make_unique<data_reduction_proxy::DataReductionProxyData>(
            *data_reduction_proxy_data);
  }

  const base::Value* previews_user_data = value.FindKey(kPreviewsUserDataKey);
  if (previews_user_data) {
    previews_user_data_ =
        std::make_unique<previews::PreviewsUserData>(*previews_user_data);
  }

  previews_state_ = value.FindKey(kPreviewsStateKey)->GetInt();
}

ChromeNavigationData* ChromeNavigationData::GetDataAndCreateIfNecessary(
    net::URLRequest* request) {
  if (!request)
    return nullptr;
  ChromeNavigationData* data = static_cast<ChromeNavigationData*>(
      request->GetUserData(kChromeNavigationDataUserDataKey));
  if (data)
    return data;
  data = new ChromeNavigationData();
  request->SetUserData(kChromeNavigationDataUserDataKey,
                       base::WrapUnique(data));
  return data;
}
