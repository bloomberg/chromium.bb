// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/presentation_toast.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/presentation_toast_texture.h"

namespace vr_shell {

PresentationToast::PresentationToast(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<PresentationToastTexture>()) {}

PresentationToast::~PresentationToast() = default;

UiTexture* PresentationToast::GetTexture() const {
  return texture_.get();
}

}  // namespace vr_shell
