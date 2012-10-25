// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RESULT_CODES_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RESULT_CODES_H_

#include "content/public/common/result_codes.h"

namespace android_webview {

enum ResultCode {
  RESULT_CODE_START = content::RESULT_CODE_LAST_CODE,

  // A critical resource file is missing.
  RESULT_CODE_MISSING_DATA,
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RESULT_CODES_H_
