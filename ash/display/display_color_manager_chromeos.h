// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_COLOR_MANAGER_CHROMEOS_H_
#define ASH_DISPLAY_DISPLAY_COLOR_MANAGER_CHROMEOS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/types/display_constants.h"

namespace base {
class SequencedWorkerPool;
}

namespace ui {
class DisplaySnapshot;
struct GammaRampRGBEntry;
}  // namespace ui

namespace ash {

// An object that observes changes in display configuration applies any color
// calibration where needed.
class ASH_EXPORT DisplayColorManager
    : public ui::DisplayConfigurator::Observer {
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

    std::vector<ui::GammaRampRGBEntry> degamma_lut;
    std::vector<ui::GammaRampRGBEntry> gamma_lut;
    std::vector<float> correction_matrix;
  };

 protected:
  virtual void FinishLoadCalibrationForDisplay(int64_t display_id,
                                               int64_t product_id,
                                               bool has_color_correction_matrix,
                                               ui::DisplayConnectionType type,
                                               const base::FilePath& path,
                                               bool file_downloaded);
  virtual void UpdateCalibrationData(
      int64_t display_id,
      int64_t product_id,
      std::unique_ptr<ColorCalibrationData> data);

 private:
  void ApplyDisplayColorCalibration(int64_t display_id, int64_t product_id);
  void LoadCalibrationForDisplay(const ui::DisplaySnapshot* display);

  ui::DisplayConfigurator* configurator_;
  std::map<int64_t, std::unique_ptr<ColorCalibrationData>> calibration_map_;
  base::ThreadChecker thread_checker_;
  base::SequencedWorkerPool* blocking_pool_;

  // Factory for callbacks.
  base::WeakPtrFactory<DisplayColorManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayColorManager);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_COLOR_MANAGER_CHROMEOS_H_
