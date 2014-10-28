// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_CRASH_HANDLER_H_
#define ANDROID_WEBVIEW_COMMON_AW_CRASH_HANDLER_H_

#include <string>

namespace android_webview {
namespace crash_handler {

void RegisterCrashHandler(const std::string& version);

}  // namespace crash_handler
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_CRASH_HANDLER_H_
