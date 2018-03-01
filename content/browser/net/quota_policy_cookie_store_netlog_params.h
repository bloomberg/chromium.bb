// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_QUOTA_POLICY_COOKIE_STORE_NETLOG_PARAMS_H_
#define CONTENT_BROWSER_NET_QUOTA_POLICY_COOKIE_STORE_NETLOG_PARAMS_H_

#include <memory>

#include "base/values.h"
#include "content/common/content_export.h"
#include "net/log/net_log_capture_mode.h"

namespace content {

CONTENT_EXPORT std::unique_ptr<base::Value>
QuotaPolicyCookieStoreOriginFiltered(const std::string& origin,
                                     bool secure,
                                     net::NetLogCaptureMode capture_mode);

}  // namespace content

#endif  // CONTENT_BROWSER_NET_QUOTA_POLICY_COOKIE_STORE_NETLOG_PARAMS_H_
