// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/linux/platform_sensor_utils_linux.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"

namespace device {

namespace {

bool InitSensorPaths(const std::vector<std::string>& input_names,
                     const char* base_path,
                     std::vector<base::FilePath>* sensor_paths) {
  // Search the iio/devices directory for a subdirectory (eg "device0" or
  // "iio:device0") that contains the specified input_name file (eg
  // "in_illuminance_input" or "in_illuminance0_input").
  base::FileEnumerator dir_enumerator(base::FilePath(base_path), false,
                                      base::FileEnumerator::DIRECTORIES);
  for (base::FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    for (auto const& file_name : input_names) {
      base::FilePath full_path = check_path.Append(file_name);
      if (base::PathExists(full_path)) {
        sensor_paths->push_back(full_path);
        return true;
      }
    }
  }
  return false;
}

bool GetSensorFilePaths(const SensorDataLinux& data,
                        std::vector<base::FilePath>* sensor_paths) {
  DCHECK(sensor_paths->empty());
  // Depending on a sensor, there can be up to three sets of files that need
  // to be checked. If one of three files is not found, a sensor is
  // treated as a non-existing one.
  for (auto const& file_names : data.sensor_file_names) {
    // Supply InitSensorPaths() with a set of files.
    // Only one file from each set should be found.
    if (!InitSensorPaths(file_names, data.base_path_sensor_linux, sensor_paths))
      return false;
  }
  return true;
}

}  // namespace

// static
std::unique_ptr<SensorReader> SensorReader::Create(
    const SensorDataLinux& data) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::vector<base::FilePath> sensor_paths;
  if (!GetSensorFilePaths(data, &sensor_paths))
    return nullptr;
  return base::WrapUnique(new SensorReader(std::move(sensor_paths)));
}

SensorReader::SensorReader(std::vector<base::FilePath> sensor_paths)
    : sensor_paths_(std::move(sensor_paths)) {
  DCHECK(!sensor_paths_.empty());
}

SensorReader::~SensorReader() = default;

bool SensorReader::ReadSensorReading(SensorReading* reading) {
  base::ThreadRestrictions::AssertIOAllowed();
  SensorReading readings;
  DCHECK_LE(sensor_paths_.size(), arraysize(readings.values));
  int i = 0;
  for (const auto& path : sensor_paths_) {
    std::string new_read_value;
    if (!base::ReadFileToString(path, &new_read_value))
      return false;

    double new_value = 0;
    base::TrimWhitespaceASCII(new_read_value, base::TRIM_ALL, &new_read_value);
    if (!base::StringToDouble(new_read_value, &new_value))
      return false;
    readings.values[i++] = new_value;
  }
  *reading = readings;
  return true;
}

}  // namespace device
