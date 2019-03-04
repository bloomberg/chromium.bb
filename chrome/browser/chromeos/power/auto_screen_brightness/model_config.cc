// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

ModelConfig::ModelConfig() = default;

ModelConfig::ModelConfig(const ModelConfig& config) {
  auto_brightness_als_horizon_seconds =
      config.auto_brightness_als_horizon_seconds;
  log_lux = config.log_lux;
  brightness = config.brightness;
  metrics_key = config.metrics_key;
  model_als_horizon_seconds = config.model_als_horizon_seconds;
}

ModelConfig::~ModelConfig() = default;

bool IsValidModelConfig(const ModelConfig& model_config) {
  if (model_config.auto_brightness_als_horizon_seconds <= 0)
    return false;

  if (model_config.log_lux.size() != model_config.brightness.size() ||
      model_config.brightness.empty())
    return false;

  if (model_config.metrics_key.empty())
    return false;

  if (model_config.model_als_horizon_seconds <= 0)
    return false;

  return true;
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
