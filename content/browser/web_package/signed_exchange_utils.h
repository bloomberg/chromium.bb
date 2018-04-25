// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_

#include <string>

namespace content {

class SignedExchangeDevToolsProxy;

namespace signed_exchange_utils {

// Utility method to call SignedExchangeDevToolsProxy::ReportErrorMessage() and
// TRACE_EVENT_END() to report the error to both DevTools and about:tracing. If
// |devtools_proxy| is nullptr, it just calls TRACE_EVENT_END().
void ReportErrorAndEndTraceEvent(SignedExchangeDevToolsProxy* devtools_proxy,
                                 const char* trace_event_name,
                                 const std::string& error_message);

}  // namespace  signed_exchange_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
