// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chromeos/chromeos_paths.h"
#include "third_party/qcms/src/qcms.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

bool ParseFile(const base::FilePath& path,
               DisplayColorManager::ColorCalibrationData* data) {
  if (!base::PathExists(path))  // No icc file for this display; not an error.
    return false;
  qcms_profile* display_profile = qcms_profile_from_path(path.value().c_str());

  if (!display_profile) {
    LOG(WARNING) << "Unable to load ICC file: " << path.value();
    return false;
  }

  size_t vcgt_channel_length =
      qcms_profile_get_vcgt_channel_length(display_profile);
  if (!vcgt_channel_length) {
    LOG(WARNING) << "No vcgt table in ICC file: " << path.value();
    qcms_profile_release(display_profile);
    return false;
  }

  std::vector<uint16_t> vcgt_data;
  vcgt_data.resize(vcgt_channel_length * 3);
  if (!qcms_profile_get_vcgt_rgb_channels(display_profile, &vcgt_data[0])) {
    LOG(WARNING) << "Unable to get vcgt data";
    qcms_profile_release(display_profile);
    return false;
  }

  data->lut.resize(vcgt_channel_length);
  for (size_t i = 0; i < vcgt_channel_length; ++i) {
    data->lut[i].r = vcgt_data[i];
    data->lut[i].g = vcgt_data[vcgt_channel_length + i];
    data->lut[i].b = vcgt_data[(vcgt_channel_length * 2) + i];
  }
  qcms_profile_release(display_profile);
  return true;
}

base::FilePath PathForDisplaySnapshot(const ui::DisplaySnapshot* snapshot) {
  base::FilePath path;
  CHECK(
      PathService::Get(chromeos::DIR_DEVICE_COLOR_CALIBRATION_PROFILES, &path));
  path = path.Append(
      base::StringPrintf("%08" PRIx64 ".icc", snapshot->product_id()));
  return path;
}

}  // namespace

DisplayColorManager::DisplayColorManager(
    ui::DisplayConfigurator* configurator,
    base::SequencedWorkerPool* blocking_pool)
    : configurator_(configurator), blocking_pool_(blocking_pool) {
  configurator_->AddObserver(this);
}

DisplayColorManager::~DisplayColorManager() {
  configurator_->RemoveObserver(this);
  STLDeleteValues(&calibration_map_);
}

void DisplayColorManager::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& display_states) {
  for (const ui::DisplaySnapshot* state : display_states) {
    if (calibration_map_[state->product_id()]) {
      ApplyDisplayColorCalibration(state->display_id(), state->product_id());
    } else {
      if (state->product_id() != ui::DisplaySnapshot::kInvalidProductID)
        LoadCalibrationForDisplay(state);
    }
  }
}

void DisplayColorManager::ApplyDisplayColorCalibration(int64_t display_id,
                                                       int64_t product_id) {
  if (calibration_map_.find(product_id) != calibration_map_.end()) {
    ColorCalibrationData* ramp = calibration_map_[product_id];
    if (!configurator_->SetGammaRamp(display_id, ramp->lut))
      LOG(WARNING) << "Error applying gamma ramp";
  }
}

void DisplayColorManager::LoadCalibrationForDisplay(
    const ui::DisplaySnapshot* display) {
  if (display->display_id() == gfx::Display::kInvalidDisplayID) {
    LOG(WARNING) << "Trying to load calibration data for invalid display id";
    return;
  }

  base::FilePath path = PathForDisplaySnapshot(display);
  VLOG(1) << "Loading ICC file " << path.value()
          << " for display id: " << display->display_id()
          << " with product id: " << display->product_id();

  scoped_ptr<ColorCalibrationData> data(new ColorCalibrationData());
  base::Callback<bool(void)> request(
      base::Bind(&ParseFile, path, base::Unretained(data.get())));
  base::PostTaskAndReplyWithResult(
      blocking_pool_, FROM_HERE, request,
      base::Bind(&DisplayColorManager::UpdateCalibrationData, AsWeakPtr(),
                 display->display_id(), display->product_id(),
                 base::Passed(data.Pass())));
}

void DisplayColorManager::UpdateCalibrationData(
    int64_t display_id,
    int64_t product_id,
    scoped_ptr<ColorCalibrationData> data,
    bool success) {
  DCHECK_EQ(base::MessageLoop::current()->type(), base::MessageLoop::TYPE_UI);
  if (success) {
    // The map takes over ownership of the underlying memory.
    calibration_map_[product_id] = data.release();
    ApplyDisplayColorCalibration(display_id, product_id);
  }
}

DisplayColorManager::ColorCalibrationData::ColorCalibrationData() {}

DisplayColorManager::ColorCalibrationData::~ColorCalibrationData() {}

}  // namespace ash
