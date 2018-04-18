// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_COLOR_MANAGER_H_
#define ASH_DISPLAY_DISPLAY_COLOR_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_constants.h"

namespace base {
class SequencedTaskRunner;
}

namespace display {
class DisplaySnapshot;
class Screen;
struct GammaRampRGBEntry;
}  // namespace display

namespace ash {

// An object that observes changes in display configuration applies any color
// calibration where needed.
class ASH_EXPORT DisplayColorManager
    : public display::DisplayConfigurator::Observer,
      public display::DisplayObserver {
 public:
  DisplayColorManager(display::DisplayConfigurator* configurator,
                      display::Screen* screen_to_observe);
  ~DisplayColorManager() override;

  // Sets the given |color_matrix| on the display hardware of |display_id|,
  // combining the given matrix with any available color calibration matrix for
  // this display. This doesn't affect gamma or degamma values.
  // Returns true if the hardware supports this operation and the matrix was
  // successfully sent to the GPU.
  bool SetDisplayColorMatrix(int64_t display_id,
                             const SkMatrix44& color_matrix);

  // display::DisplayConfigurator::Observer
  void OnDisplayModeChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;
  void OnDisplayModeChangeFailed(
      const display::DisplayConfigurator::DisplayStateList& displays,
      display::MultipleDisplayState failed_new_state) override {}

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& old_display) override;

  struct ColorCalibrationData {
    ColorCalibrationData();
    ~ColorCalibrationData();

    std::vector<display::GammaRampRGBEntry> degamma_lut;
    std::vector<display::GammaRampRGBEntry> gamma_lut;
    std::vector<float> correction_matrix;
  };

 protected:
  virtual void FinishLoadCalibrationForDisplay(
      int64_t display_id,
      int64_t product_code,
      bool has_color_correction_matrix,
      display::DisplayConnectionType type,
      const base::FilePath& path,
      bool file_downloaded);
  virtual void UpdateCalibrationData(
      int64_t display_id,
      int64_t product_code,
      std::unique_ptr<ColorCalibrationData> data);

 private:
  friend class DisplayColorManagerTest;

  void ApplyDisplayColorCalibration(int64_t display_id, int64_t product_code);
  void LoadCalibrationForDisplay(const display::DisplaySnapshot* display);

  display::DisplayConfigurator* configurator_;

  // This is a pre-allocated storage in order to avoid re-allocating the
  // matrix array every time when converting a skia matrix to a matrix array.
  std::vector<float> matrix_buffer_;

  // Contains a per display color transform matrix that can be post-multiplied
  // by any available color calibration matrix for the corresponding display.
  // The key is the display ID.
  base::flat_map<int64_t, SkMatrix44> displays_color_matrix_map_;

  // Maps a display's color calibration data by the display's product code as
  // the key.
  std::map<int64_t, std::unique_ptr<ColorCalibrationData>> calibration_map_;
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  // This is null in DisplayColorManagerTest.
  display::Screen* screen_to_observe_;

  // Factory for callbacks.
  base::WeakPtrFactory<DisplayColorManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayColorManager);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_COLOR_MANAGER_H_
