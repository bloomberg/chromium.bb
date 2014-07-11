// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
#define ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_

namespace switches {

// Disable the record whole document workaround which is used to support
// teleporting software draws.
extern const char kDisableRecordDocumentWorkaround[];

// Always use software auxiliary bitmap rendering path. This is temporary
// until Android L is released to AOSP.
extern const char kForceAuxiliaryBitmap[];

bool ForceAuxiliaryBitmap();

}  // namespace switches

#endif  // ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
