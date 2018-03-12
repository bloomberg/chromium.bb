// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
#define ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_

#include "ash/system/unified/unified_slider_view.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {

class UnifiedBrightnessSliderController;

// View of a slider that can change display brightness. It observes current
// brightness level from PowerManagerClient.
class UnifiedBrightnessView : public UnifiedSliderView,
                              public chromeos::PowerManagerClient::Observer {
 public:
  explicit UnifiedBrightnessView(UnifiedBrightnessSliderController* controller);
  ~UnifiedBrightnessView() override;

 private:
  void HandleInitialBrightness(base::Optional<double> percent);

  // chromeos::PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

  base::WeakPtrFactory<UnifiedBrightnessView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UnifiedBrightnessView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_UNIFIED_BRIGHTNESS_VIEW_H_
