// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/display/display_color_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/qcms/src/qcms.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

using content::BrowserThread;

namespace ash {

namespace {

bool ParseFile(const base::FilePath& path,
               DisplayColorManager::ColorCalibrationData* data) {
  qcms_profile* display_profile = qcms_profile_from_path(path.value().c_str());

  if (!display_profile) {
    LOG(WARNING) << "Unable to load ICC file: " << path.value();
    return false;
  }

  size_t vcgt_channel_length =
      qcms_profile_get_vcgt_channel_length(display_profile);
  if (!vcgt_channel_length) {
    LOG(WARNING) << "No vcgt table in ICC file: " << path.value();
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

}  // namespace

DisplayColorManager::DisplayColorManager(ui::DisplayConfigurator* configurator)
    : configurator_(configurator) {
  configurator_->AddObserver(this);
  LoadInternalFromCommandLine();
}

DisplayColorManager::~DisplayColorManager() {
  configurator_->RemoveObserver(this);

  for (auto it : calibration_map_) {
    delete it.second;
    calibration_map_.erase(it.first);
  }
}

void DisplayColorManager::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& display_states) {
  for (const ui::DisplaySnapshot* state : display_states)
    ApplyDisplayColorCalibration(state->display_id());
}

void DisplayColorManager::ApplyDisplayColorCalibration(uint64_t display_id) {
  if (calibration_map_.find(display_id) != calibration_map_.end()) {
    ColorCalibrationData* ramp = calibration_map_[display_id];
    if (!configurator_->SetGammaRamp(display_id, ramp->lut))
      LOG(WARNING) << "Error applying gamma ramp";
  }
}

void DisplayColorManager::LoadInternalFromCommandLine() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          chromeos::switches::kInternalDisplayColorProfileFile)) {
    const base::FilePath& path = command_line->GetSwitchValuePath(
        chromeos::switches::kInternalDisplayColorProfileFile);
    VLOG(1) << "Loading ICC file : " << path.value()
            << "for internal display id: " << gfx::Display::InternalDisplayId();
    LoadCalibrationForDisplay(gfx::Display::InternalDisplayId(), path);
  }
}

void DisplayColorManager::LoadCalibrationForDisplay(
    int64_t display_id,
    const base::FilePath& path) {
  if (display_id == gfx::Display::kInvalidDisplayID) {
    LOG(WARNING) << "Trying to load calibration data for invalid display id";
    return;
  }

  scoped_ptr<ColorCalibrationData> data(new ColorCalibrationData());
  base::Callback<bool(void)> request(
      base::Bind(&ParseFile, path, base::Unretained(data.get())));
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(), FROM_HERE, request,
      base::Bind(&DisplayColorManager::UpdateCalibrationData, AsWeakPtr(),
                 display_id, base::Passed(data.Pass())));
}

void DisplayColorManager::UpdateCalibrationData(
    uint64_t display_id,
    scoped_ptr<ColorCalibrationData> data,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (success) {
    // The map takes over ownership of the underlying memory.
    calibration_map_[display_id] = data.release();
  }
}

DisplayColorManager::ColorCalibrationData::ColorCalibrationData() {
}

DisplayColorManager::ColorCalibrationData::~ColorCalibrationData() {
}

}  // namespace ash
