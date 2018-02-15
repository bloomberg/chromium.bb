// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TOAST_H_
#define CHROME_BROWSER_VR_ELEMENTS_TOAST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class LinearLayout;
class Rect;
class VectorIcon;

// Toast may have a vector icon or a text or both as the foreground and rounded
// rect background.
class Toast : public UiElement {
 public:
  Toast();
  ~Toast() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  void AddIcon(const gfx::VectorIcon& icon, int width_pixels, float icon_size);
  void AddText(const base::string16& text,
               float font_height_dmm,
               TextLayoutMode text_layout_mode);
  void SetMargin(float margin_in_dmms);

  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);

 private:
  void OnSetDrawPhase() override;
  void OnSetName() override;
  void OnSetSize(const gfx::SizeF& size) override;
  void OnSetCornerRadii(const CornerRadii& radii) override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                cc::KeyframeModel* keyframe_model) override;

  LinearLayout* container_ = nullptr;
  Rect* background_ = nullptr;
  VectorIcon* icon_ = nullptr;
  Text* text_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Toast);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TOAST_H_
