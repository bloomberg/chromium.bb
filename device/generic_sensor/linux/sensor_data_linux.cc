// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"
#include "base/version.h"
#include "device/generic_sensor/generic_sensor_consts.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"

namespace device {

namespace {

using mojom::SensorType;

#if defined(OS_CHROMEOS)
// ChromeOS kernel version, when axes were changed to XYZ. Before 3.18,
// they were YXZ.
const char kChangedAxisKernelVersion[] = "3.18.0";
#endif

void InitAmbientLightSensorData(SensorPathsLinux* data) {
  std::vector<std::string> file_names{
      "in_illuminance0_input", "in_illuminance_input", "in_illuminance0_raw",
      "in_illuminance_raw", "in_intensity_both_raw"};
  data->sensor_file_names.push_back(std::move(file_names));
  data->sensor_frequency_file_name = "in_intensity_sampling_frequency";
  data->sensor_scale_name = "in_intensity_scale";
  data->apply_scaling_func = base::Bind(
      [](double scaling_value, double offset, SensorReading& reading) {
        reading.values[0] = scaling_value * (reading.values[0] + offset);
      });
  data->default_configuration =
      PlatformSensorConfiguration(kDefaultAmbientLightFrequencyHz);
}

// Depending on a kernel version, CrOS has a different axes plane.
// Before 3.18 it was YXZ and after XYZ.
// TODO(maksims): Track crbug.com/501184. 3.14 will have the same sensor stack
// as 3.18 has, which will probably change the order of axes.
void MaybeCheckKernelVersionAndAssignFileNames(
    const std::vector<std::string>& file_names_x,
    const std::vector<std::string>& file_names_y,
    const std::vector<std::string>& file_names_z,
    SensorPathsLinux* data) {
#if defined(OS_CHROMEOS)
  const base::Version checked_kernel_version(kChangedAxisKernelVersion);
  DCHECK(checked_kernel_version.IsValid());
  const base::Version current_version(base::SysInfo::OperatingSystemVersion());
  if (current_version.IsValid() && current_version < checked_kernel_version) {
    data->sensor_file_names.push_back(file_names_y);
    data->sensor_file_names.push_back(file_names_x);
  } else {
    data->sensor_file_names.push_back(file_names_x);
    data->sensor_file_names.push_back(file_names_y);
  }
  data->sensor_file_names.push_back(file_names_z);
#else
  data->sensor_file_names.push_back(file_names_x);
  data->sensor_file_names.push_back(file_names_y);
  data->sensor_file_names.push_back(file_names_z);
#endif
}

// TODO(maksims): add support for lid accelerometer on chromeos.
void InitAccelerometerSensorData(SensorPathsLinux* data) {
  std::vector<std::string> file_names_x{"in_accel_x_base_raw",
                                        "in_accel_x_raw"};
  std::vector<std::string> file_names_y{"in_accel_y_base_raw",
                                        "in_accel_y_raw"};
  std::vector<std::string> file_names_z{"in_accel_z_base_raw",
                                        "in_accel_z_raw"};

#if defined(OS_CHROMEOS)
  data->sensor_scale_name = "in_accel_base_scale";
  data->sensor_frequency_file_name = "in_accel_base_sampling_frequency";
  data->apply_scaling_func = base::Bind(
      [](double scaling_value, double offset, SensorReading& reading) {
        double scaling = kMeanGravity / scaling_value;
        reading.values[0] = scaling * (reading.values[0] + offset);
        reading.values[1] = scaling * (reading.values[1] + offset);
        reading.values[2] = scaling * (reading.values[2] + offset);
      });
#else
  data->sensor_scale_name = "in_accel_scale";
  data->sensor_offset_file_name = "in_accel_offset";
  data->sensor_frequency_file_name = "in_accel_sampling_frequency";
  data->apply_scaling_func = base::Bind(
      [](double scaling_value, double offset, SensorReading& reading) {
        // Adapt Linux reading values to generic sensor api specs.
        reading.values[0] = -scaling_value * (reading.values[0] + offset);
        reading.values[1] = -scaling_value * (reading.values[1] + offset);
        reading.values[2] = -scaling_value * (reading.values[2] + offset);
      });
#endif

  MaybeCheckKernelVersionAndAssignFileNames(file_names_x, file_names_y,
                                            file_names_z, data);
  data->default_configuration =
      PlatformSensorConfiguration(kDefaultAccelerometerFrequencyHz);
}

void InitGyroscopeSensorData(SensorPathsLinux* data) {
  std::vector<std::string> file_names_x{"in_anglvel_x_base_raw",
                                        "in_anglvel_x_raw"};
  std::vector<std::string> file_names_y{"in_anglvel_y_base_raw",
                                        "in_anglvel_y_raw"};
  std::vector<std::string> file_names_z{"in_anglvel_z_base_raw",
                                        "in_anglvel_z_raw"};
#if defined(OS_CHROMEOS)
  data->sensor_scale_name = "in_anglvel_base_scale";
  data->sensor_frequency_file_name = "in_anglvel_base_frequency";
  data->apply_scaling_func = base::Bind(
      [](double scaling_value, double offset, SensorReading& reading) {
        double scaling = kMeanGravity * kRadiansInDegrees / scaling_value;
        // Adapt CrOS reading values to generic sensor api specs.
        reading.values[0] = -scaling * (reading.values[0] + offset);
        reading.values[1] = -scaling * (reading.values[1] + offset);
        reading.values[2] = -scaling * (reading.values[2] + offset);
      });
#else
  data->sensor_scale_name = "in_anglvel_scale";
  data->sensor_offset_file_name = "in_anglvel_offset";
  data->sensor_frequency_file_name = "in_anglvel_sampling_frequency";
  data->apply_scaling_func = base::Bind(
      [](double scaling_value, double offset, SensorReading& reading) {
        reading.values[0] = scaling_value * (reading.values[0] + offset);
        reading.values[1] = scaling_value * (reading.values[1] + offset);
        reading.values[2] = scaling_value * (reading.values[2] + offset);
      });
#endif

  MaybeCheckKernelVersionAndAssignFileNames(file_names_x, file_names_y,
                                            file_names_z, data);
  data->default_configuration =
      PlatformSensorConfiguration(kDefaultGyroscopeFrequencyHz);
}

// TODO(maksims): Verify magnitometer works correctly on a chromebook when
// I get one with that sensor onboard.
void InitMagnitometerSensorData(SensorPathsLinux* data) {
  std::vector<std::string> file_names_x{"in_magn_x_raw"};
  std::vector<std::string> file_names_y{"in_magn_y_raw"};
  std::vector<std::string> file_names_z{"in_magn_z_raw"};

  data->sensor_scale_name = "in_magn_scale";
  data->sensor_offset_file_name = "in_magn_offset";
  data->sensor_frequency_file_name = "in_magn_sampling_frequency";
  data->apply_scaling_func = base::Bind(
      [](double scaling_value, double offset, SensorReading& reading) {
        double scaling = scaling_value * kMicroteslaInGauss;
        reading.values[0] = scaling * (reading.values[0] + offset);
        reading.values[1] = scaling * (reading.values[1] + offset);
        reading.values[2] = scaling * (reading.values[2] + offset);
      });

  MaybeCheckKernelVersionAndAssignFileNames(file_names_x, file_names_y,
                                            file_names_z, data);
  data->default_configuration =
      PlatformSensorConfiguration(kDefaultMagnetometerFrequencyHz);
}

}  // namespace

SensorPathsLinux::SensorPathsLinux() = default;
SensorPathsLinux::~SensorPathsLinux() = default;
SensorPathsLinux::SensorPathsLinux(const SensorPathsLinux& other) = default;

bool InitSensorData(SensorType type, SensorPathsLinux* data) {
  DCHECK(data);

  data->type = type;
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      InitAmbientLightSensorData(data);
      break;
    case SensorType::ACCELEROMETER:
      InitAccelerometerSensorData(data);
      break;
    case SensorType::GYROSCOPE:
      InitGyroscopeSensorData(data);
      break;
    case SensorType::MAGNETOMETER:
      InitMagnitometerSensorData(data);
      break;
    default:
      return false;
  }

  return true;
}

SensorInfoLinux::SensorInfoLinux(
    const std::string& sensor_device_node,
    double sensor_device_frequency,
    double sensor_device_scaling_value,
    double sensor_device_offset_value,
    mojom::ReportingMode mode,
    SensorPathsLinux::ReaderFunctor scaling_func,
    std::vector<base::FilePath> device_reading_files)
    : device_node(sensor_device_node),
      device_frequency(sensor_device_frequency),
      device_scaling_value(sensor_device_scaling_value),
      device_offset_value(sensor_device_offset_value),
      reporting_mode(mode),
      apply_scaling_func(scaling_func),
      device_reading_files(std::move(device_reading_files)) {}

SensorInfoLinux::~SensorInfoLinux() = default;

}  // namespace device
