// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/exit_warning_texture.h"
#include "chrome/browser/android/vr_shell/textures/insecure_content_permanent_texture.h"
#include "chrome/browser/android/vr_shell/textures/insecure_content_transient_texture.h"
#include "chrome/browser/android/vr_shell/textures/presentation_toast_texture.h"
#include "chrome/browser/android/vr_shell/textures/splash_screen_icon_texture.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace vr_shell {

template <class T>
class SimpleTexturedElement : public TexturedElement {
 public:
  // |preferred_width| is the element's desired width in meters. Constraints
  // implied by the texture being rendered may or may not allow it to be
  // rendered exactly at the preferred width.
  explicit SimpleTexturedElement(int maximum_width)
      : TexturedElement(maximum_width), texture_(base::MakeUnique<T>()) {}
  ~SimpleTexturedElement() override {}
  T* GetDerivedTexture() { return texture_.get(); }

 private:
  UiTexture* GetTexture() const override { return texture_.get(); }
  std::unique_ptr<T> texture_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTexturedElement);
};

typedef SimpleTexturedElement<ExitWarningTexture> ExitWarning;
typedef SimpleTexturedElement<InsecureContentPermanentTexture>
    PermanentSecurityWarning;
typedef SimpleTexturedElement<InsecureContentTransientTexture>
    TransientSecurityWarning;
typedef SimpleTexturedElement<PresentationToastTexture> PresentationToast;
typedef SimpleTexturedElement<SplashScreenIconTexture> SplashScreenIcon;

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_
