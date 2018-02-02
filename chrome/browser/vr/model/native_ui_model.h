// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_NATIVE_UI_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_NATIVE_UI_MODEL_H_

#include "chrome/browser/vr/content_input_delegate.h"

namespace vr {
typedef ContentInputDelegate* ContentInputDelegatePtr;
struct NativeUiModel {
  bool hosted_ui_enabled = false;
  float size_ratio = 0;
  ContentInputDelegatePtr delegate = nullptr;
  unsigned int texture_id = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_NATIVE_UI_MODEL_H_
