// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_

#include <string>

#include "base/callback_forward.h"

namespace content {
namespace signed_exchange_utils {

using LogCallback = base::RepeatingCallback<void(const std::string&)>;

// Runs |callback| with |error_message| if |callback| is not null. And calls
// TRACE_EVENT_END() with "disabled-by-default-loading" category, |name| and
// |error_meassage|.
void RunErrorMessageCallbackAndEndTraceEvent(const char* name,
                                             const LogCallback& callback,
                                             const std::string& error_message);

}  // namespace  signed_exchange_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
