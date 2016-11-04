// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/sensors/public/cpp/device_sensors_consts.h"

namespace device {

namespace {

using mojom::SensorType;

const base::FilePath::CharType* kSensorsBasePath =
    FILE_PATH_LITERAL("/sys/bus/iio/devices");

void InitAmbientLightSensorData(SensorDataLinux* data) {
  std::vector<std::string> file_names{
      "in_illuminance0_input", "in_illuminance_input", "in_illuminance0_raw",
      "in_illuminance_raw"};

  data->sensor_file_names.push_back(std::move(file_names));
  data->reporting_mode = mojom::ReportingMode::ON_CHANGE;
  data->default_configuration =
      PlatformSensorConfiguration(kDefaultAmbientLightFrequencyHz);
}

}  // namespace

SensorDataLinux::SensorDataLinux() : base_path_sensor_linux(kSensorsBasePath) {}

SensorDataLinux::~SensorDataLinux() = default;

SensorDataLinux::SensorDataLinux(const SensorDataLinux& other) = default;

bool InitSensorData(SensorType type, SensorDataLinux* data) {
  DCHECK(data);

  switch (type) {
    case SensorType::AMBIENT_LIGHT: {
      InitAmbientLightSensorData(data);
      break;
    }
    default: {
      NOTIMPLEMENTED();
      return false;
    }
  }
  return true;
}

}  // namespace device
