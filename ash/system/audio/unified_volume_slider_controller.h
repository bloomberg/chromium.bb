// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_

#include "ash/system/unified/unified_slider_view.h"

namespace ash {

// Controller of a slider that can change audio volume.
class UnifiedVolumeSliderController : public UnifiedSliderListener {
 public:
  UnifiedVolumeSliderController();
  ~UnifiedVolumeSliderController() override;

  // Instantiates UnifiedSliderView. The view will be onwed by views hierarchy.
  // The view should be always deleted after the controller is destructed.
  views::View* CreateView();

  // UnifiedSliderListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

 private:
  UnifiedSliderView* slider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedVolumeSliderController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_VOLUME_SLIDER_CONTROLLER_H_
