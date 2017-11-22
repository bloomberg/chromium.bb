// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/exclusive_screen_toast_texture.h"
#include "chrome/browser/vr/elements/exit_warning_texture.h"
#include "chrome/browser/vr/elements/system_indicator_texture.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"

namespace vr {

template <typename T, typename R>
class SimpleTexturedElement : public TexturedElement {
 public:
  explicit SimpleTexturedElement(int maximum_width)
      : TexturedElement(maximum_width, R()), texture_(base::MakeUnique<T>()) {}
  ~SimpleTexturedElement() override {}
  T* GetDerivedTexture() { return texture_.get(); }

 protected:
  UiTexture* GetTexture() const override { return texture_.get(); }

 private:
  std::unique_ptr<T> texture_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTexturedElement);
};

typedef SimpleTexturedElement<ExitWarningTexture,
                              TexturedElement::ResizeVertically>
    ExitWarning;
typedef SimpleTexturedElement<ExclusiveScreenToastTexture,
                              TexturedElement::ResizeHorizontally>
    ExclusiveScreenToast;
typedef SimpleTexturedElement<SystemIndicatorTexture,
                              TexturedElement::ResizeHorizontally>
    SystemIndicator;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_
