// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/quota_policy_cookie_store_netlog_params.h"

namespace content {

std::unique_ptr<base::Value> QuotaPolicyCookieStoreOriginFiltered(
    const std::string& origin,
    bool secure,
    net::NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::Value> dict =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  dict->SetKey("origin", base::Value(origin));
  dict->SetKey("secure", base::Value(secure));
  return dict;
}

}  // namespace content
