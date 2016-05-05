// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/quirks/quirks_manager.h"
#include "third_party/qcms/src/qcms.h"
#include "ui/display/display.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

namespace ash {

namespace {

std::unique_ptr<DisplayColorManager::ColorCalibrationData> ParseDisplayProfile(
    const base::FilePath& path,
    bool has_color_correction_matrix) {
  VLOG(1) << "Trying ICC file " << path.value()
          << " has_color_correction_matrix: "
          << (has_color_correction_matrix ? "true" : "false");
  qcms_profile* display_profile = qcms_profile_from_path(path.value().c_str());
  if (!display_profile) {
    LOG(WARNING) << "Unable to load ICC file: " << path.value();
    return nullptr;
  }

  size_t vcgt_channel_length =
      qcms_profile_get_vcgt_channel_length(display_profile);

  if (!has_color_correction_matrix && !vcgt_channel_length) {
    LOG(WARNING) << "No vcgt table or color correction matrix in ICC file: "
                 << path.value();
    qcms_profile_release(display_profile);
    return nullptr;
  }

  std::unique_ptr<DisplayColorManager::ColorCalibrationData> data(
      new DisplayColorManager::ColorCalibrationData());
  if (vcgt_channel_length) {
    VLOG_IF(1, has_color_correction_matrix)
        << "Using VCGT data on CTM enabled platform.";

    std::vector<uint16_t> vcgt_data;
    vcgt_data.resize(vcgt_channel_length * 3);
    if (!qcms_profile_get_vcgt_rgb_channels(display_profile, &vcgt_data[0])) {
      LOG(WARNING) << "Unable to get vcgt data";
      qcms_profile_release(display_profile);
      return nullptr;
    }

    data->gamma_lut.resize(vcgt_channel_length);
    for (size_t i = 0; i < vcgt_channel_length; ++i) {
      data->gamma_lut[i].r = vcgt_data[i];
      data->gamma_lut[i].g = vcgt_data[vcgt_channel_length + i];
      data->gamma_lut[i].b = vcgt_data[(vcgt_channel_length * 2) + i];
    }
  } else {
    VLOG(1) << "Using full degamma/gamma/CTM from profile.";
    qcms_profile* srgb_profile = qcms_profile_sRGB();

    qcms_transform* transform =
        qcms_transform_create(srgb_profile, QCMS_DATA_RGB_8, display_profile,
                              QCMS_DATA_RGB_8, QCMS_INTENT_PERCEPTUAL);

    if (!transform) {
      LOG(WARNING)
          << "Unable to create transformation from sRGB to display profile.";

      qcms_profile_release(display_profile);
      qcms_profile_release(srgb_profile);
      return nullptr;
    }

    if (!qcms_transform_is_matrix(transform)) {
      LOG(WARNING) << "No transformation matrix available";

      qcms_transform_release(transform);
      qcms_profile_release(display_profile);
      qcms_profile_release(srgb_profile);
      return nullptr;
    }

    size_t degamma_size = qcms_transform_get_input_trc_rgba(
        transform, srgb_profile, QCMS_TRC_USHORT, NULL);
    size_t gamma_size = qcms_transform_get_output_trc_rgba(
        transform, display_profile, QCMS_TRC_USHORT, NULL);

    if (degamma_size == 0 || gamma_size == 0) {
      LOG(WARNING)
          << "Invalid number of elements in gamma tables: degamma size = "
          << degamma_size << " gamma size = " << gamma_size;

      qcms_transform_release(transform);
      qcms_profile_release(display_profile);
      qcms_profile_release(srgb_profile);
      return nullptr;
    }

    std::vector<uint16_t> degamma_data;
    std::vector<uint16_t> gamma_data;
    degamma_data.resize(degamma_size * 4);
    gamma_data.resize(gamma_size * 4);

    qcms_transform_get_input_trc_rgba(transform, srgb_profile, QCMS_TRC_USHORT,
                                      &degamma_data[0]);
    qcms_transform_get_output_trc_rgba(transform, display_profile,
                                       QCMS_TRC_USHORT, &gamma_data[0]);

    data->degamma_lut.resize(degamma_size);
    for (size_t i = 0; i < degamma_size; ++i) {
      data->degamma_lut[i].r = degamma_data[i * 4];
      data->degamma_lut[i].g = degamma_data[(i * 4) + 1];
      data->degamma_lut[i].b = degamma_data[(i * 4) + 2];
    }

    data->gamma_lut.resize(gamma_size);
    for (size_t i = 0; i < gamma_size; ++i) {
      data->gamma_lut[i].r = gamma_data[i * 4];
      data->gamma_lut[i].g = gamma_data[(i * 4) + 1];
      data->gamma_lut[i].b = gamma_data[(i * 4) + 2];
    }

    data->correction_matrix.resize(9);
    for (int i = 0; i < 9; ++i) {
      data->correction_matrix[i] =
          qcms_transform_get_matrix(transform, i / 3, i % 3);
    }

    qcms_transform_release(transform);
    qcms_profile_release(srgb_profile);
  }

  VLOG(1) << "ICC file successfully parsed";
  qcms_profile_release(display_profile);
  return data;
}

}  // namespace

DisplayColorManager::DisplayColorManager(
    ui::DisplayConfigurator* configurator,
    base::SequencedWorkerPool* blocking_pool)
    : configurator_(configurator),
      blocking_pool_(blocking_pool),
      weak_ptr_factory_(this) {
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
    ColorCalibrationData* data = calibration_map_[product_id];
    if (!configurator_->SetColorCorrection(display_id, data->degamma_lut,
                                           data->gamma_lut,
                                           data->correction_matrix))
      LOG(WARNING) << "Error applying color correction data";
  }
}

void DisplayColorManager::LoadCalibrationForDisplay(
    const ui::DisplaySnapshot* display) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (display->display_id() == display::Display::kInvalidDisplayID) {
    LOG(WARNING) << "Trying to load calibration data for invalid display id";
    return;
  }

  quirks::QuirksManager::Get()->RequestIccProfilePath(
      display->product_id(),
      base::Bind(&DisplayColorManager::FinishLoadCalibrationForDisplay,
                 weak_ptr_factory_.GetWeakPtr(), display->display_id(),
                 display->product_id(), display->has_color_correction_matrix(),
                 display->type()));
}

void DisplayColorManager::FinishLoadCalibrationForDisplay(
    int64_t display_id,
    int64_t product_id,
    bool has_color_correction_matrix,
    ui::DisplayConnectionType type,
    const base::FilePath& path,
    bool file_downloaded) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string product_string = quirks::IdToHexString(product_id);
  if (path.empty()) {
    VLOG(1) << "No ICC file found with product id: " << product_string
            << " for display id: " << display_id;
    return;
  }

  if (file_downloaded && type == ui::DISPLAY_CONNECTION_TYPE_INTERNAL) {
    VLOG(1) << "Downloaded ICC file with product id: " << product_string
            << " for internal display id: " << display_id
            << ". Profile will be applied on next startup.";
    return;
  }

  VLOG(1) << "Loading ICC file " << path.value()
          << " for display id: " << display_id
          << " with product id: " << product_string;

  base::PostTaskAndReplyWithResult(
      blocking_pool_, FROM_HERE,
      base::Bind(&ParseDisplayProfile, path, has_color_correction_matrix),
      base::Bind(&DisplayColorManager::UpdateCalibrationData,
                 weak_ptr_factory_.GetWeakPtr(), display_id, product_id));
}

void DisplayColorManager::UpdateCalibrationData(
    int64_t display_id,
    int64_t product_id,
    std::unique_ptr<ColorCalibrationData> data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data) {
    // The map takes over ownership of the underlying memory.
    calibration_map_[product_id] = data.release();
    ApplyDisplayColorCalibration(display_id, product_id);
  }
}

DisplayColorManager::ColorCalibrationData::ColorCalibrationData() {}

DisplayColorManager::ColorCalibrationData::~ColorCalibrationData() {}

}  // namespace ash
