// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_COLOR_MANAGER_CHROMEOS_H_
#define ASH_DISPLAY_DISPLAY_COLOR_MANAGER_CHROMEOS_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"

namespace base {
class SequencedWorkerPool;
}

namespace ui {
struct GammaRampRGBEntry;
}  // namespace ui

namespace ash {

// An object that observes changes in display configuration applies any color
// calibration where needed.
class ASH_EXPORT DisplayColorManager
    : public ui::DisplayConfigurator::Observer,
      public base::SupportsWeakPtr<DisplayColorManager> {
 public:
  DisplayColorManager(ui::DisplayConfigurator* configurator,
                      base::SequencedWorkerPool* blocking_pool);
  ~DisplayColorManager() override;

  // ui::DisplayConfigurator::Observer
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) override;
  void OnDisplayModeChangeFailed(
      const ui::DisplayConfigurator::DisplayStateList& displays,
      ui::MultipleDisplayState failed_new_state) override {}

  struct ColorCalibrationData {
    ColorCalibrationData();
    ~ColorCalibrationData();

    std::vector<ui::GammaRampRGBEntry> lut;
  };

 private:
  void ApplyDisplayColorCalibration(int64_t display_id, int64_t product_id);
  void LoadCalibrationForDisplay(const ui::DisplaySnapshot* display);
  void UpdateCalibrationData(
      int64_t display_id,
      int64_t product_id,
      scoped_ptr<DisplayColorManager::ColorCalibrationData> data,
      bool success);

  ui::DisplayConfigurator* configurator_;
  std::map<int64_t, ColorCalibrationData*> calibration_map_;
  base::SequencedWorkerPool* blocking_pool_;

  DISALLOW_COPY_AND_ASSIGN(DisplayColorManager);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_COLOR_MANAGER_CHROMEOS_H_
