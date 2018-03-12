// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_

#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

namespace ash {

class UnifiedSliderListener : public views::ButtonListener,
                              public views::SliderListener {
 public:
  ~UnifiedSliderListener() override = default;
};

// Base view class of a slider row in UnifiedSystemTray. It has a button on the
// left side and a slider on the right side.
class UnifiedSliderView : public views::View {
 public:
  UnifiedSliderView(UnifiedSliderListener* listener,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id);
  ~UnifiedSliderView() override;

 protected:
  views::Slider* slider() { return slider_; }

 private:
  // Unowned. Owned by views hierarchy.
  views::Slider* const slider_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSliderView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SLIDER_VIEW_H_
