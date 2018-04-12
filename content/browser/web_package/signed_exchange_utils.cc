// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "base/callback.h"
#include "base/trace_event/trace_event.h"

namespace content {
namespace signed_exchange_utils {

void RunErrorMessageCallbackAndEndTraceEvent(const char* name,
                                             const LogCallback& callback,
                                             const std::string& error_message) {
  if (callback)
    callback.Run(error_message);

  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"), name, "error",
                   error_message);
}

}  // namespace signed_exchange_utils
}  // namespace content
