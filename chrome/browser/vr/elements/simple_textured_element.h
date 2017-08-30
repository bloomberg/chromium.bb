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
#include "chrome/browser/vr/elements/insecure_content_permanent_texture.h"
#include "chrome/browser/vr/elements/insecure_content_transient_texture.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/transience_manager.h"
#include "chrome/browser/vr/elements/ui_texture.h"

namespace vr {

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

 protected:
  UiTexture* GetTexture() const override { return texture_.get(); }

 private:
  std::unique_ptr<T> texture_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTexturedElement);
};

template <class T>
class TransientSimpleTexturedElement : public SimpleTexturedElement<T> {
 public:
  TransientSimpleTexturedElement(int maximum_width,
                                 const base::TimeDelta& timeout)
      : SimpleTexturedElement<T>(maximum_width), transience_(this, timeout) {}

  ~TransientSimpleTexturedElement() override {}

  void SetVisible(bool visible) override { transience_.SetVisible(visible); }

  TransienceManager* transience() { return &transience_; }

 private:
  TransienceManager transience_;

  DISALLOW_COPY_AND_ASSIGN(TransientSimpleTexturedElement);
};

typedef SimpleTexturedElement<ExitWarningTexture> ExitWarning;
typedef SimpleTexturedElement<InsecureContentPermanentTexture>
    PermanentSecurityWarning;
typedef TransientSimpleTexturedElement<InsecureContentTransientTexture>
    TransientSecurityWarning;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SIMPLE_TEXTURED_ELEMENT_H_
