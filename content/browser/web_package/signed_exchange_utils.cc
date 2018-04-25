// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"

namespace content {
namespace signed_exchange_utils {

void ReportErrorAndEndTraceEvent(SignedExchangeDevToolsProxy* devtools_proxy,
                                 const char* trace_event_name,
                                 const std::string& error_message) {
  if (devtools_proxy)
    devtools_proxy->ReportErrorMessage(error_message);
  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"), trace_event_name,
                   "error", error_message);
}

}  // namespace signed_exchange_utils
}  // namespace content
