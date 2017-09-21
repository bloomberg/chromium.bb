// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_H_
#define CHROME_BROWSER_VR_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/exclusive_screen_toast_texture.h"
#include "chrome/browser/vr/elements/simple_textured_element.h"

namespace vr {

class ExclusiveScreenToast
    : public SimpleTexturedElement<ExclusiveScreenToastTexture> {
 public:
  explicit ExclusiveScreenToast(int preferred_width);
  ~ExclusiveScreenToast() override;

 private:
  void UpdateElementSize() override;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveScreenToast);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_H_
