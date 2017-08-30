// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_LOADING_INDICATOR_H_
#define CHROME_BROWSER_VR_ELEMENTS_LOADING_INDICATOR_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"

namespace vr {

class LoadingIndicatorTexture;

class LoadingIndicator : public TexturedElement {
 public:
  explicit LoadingIndicator(int preferred_width);
  ~LoadingIndicator() override;

  void SetVisible(bool visible) override;
  void SetLoading(bool loading);
  void SetLoadProgress(float progress);

 private:
  UiTexture* GetTexture() const override;
  std::unique_ptr<LoadingIndicatorTexture> texture_;

  void UpdateOpacity();

  bool visible_ = false;
  bool loading_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoadingIndicator);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_LOADING_INDICATOR_H_
