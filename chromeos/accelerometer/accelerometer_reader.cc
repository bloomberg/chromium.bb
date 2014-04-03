// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/accelerometer/accelerometer_reader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"

namespace chromeos {

namespace {

// Paths to access necessary data from the accelerometer device.
const base::FilePath::CharType kAccelerometerTriggerPath[] =
    FILE_PATH_LITERAL("/sys/bus/iio/devices/trigger0/trigger_now");
const base::FilePath::CharType kAccelerometerDevicePath[] =
    FILE_PATH_LITERAL("/dev/cros-ec-accel");
const base::FilePath::CharType kAccelerometerIioBasePath[] =
    FILE_PATH_LITERAL("/sys/bus/iio/devices/");

// Files within the device in kAccelerometerIioBasePath containing the scales of
// the accelerometers.
const base::FilePath::CharType kAccelerometerBaseScaleName[] =
    FILE_PATH_LITERAL("in_accel_base_scale");
const base::FilePath::CharType kAccelerometerLidScaleName[] =
    FILE_PATH_LITERAL("in_accel_lid_scale");

// The filename giving the path to read the scan index of each accelerometer
// axis.
const char kAccelerometerScanIndexPath[] =
    "scan_elements/in_accel_%s_%s_index";

// The names of the accelerometers and axes in the order we want to read them.
const char kAccelerometerNames[][5] = {"base", "lid"};
const char kAccelerometerAxes[][2] = {"x", "y", "z"};
const size_t kTriggerDataValues =
    arraysize(kAccelerometerNames) * arraysize(kAccelerometerAxes);
const size_t kTriggerDataLength = kTriggerDataValues * 2;

// The length required to read uint values from configuration files.
const size_t kMaxAsciiUintLength = 21;

// The time to wait between reading the accelerometer.
const int kDelayBetweenReadsMs = 100;

// Reads |path| to the unsigned int pointed to by |value|. Returns true on
// success or false on failure.
bool ReadFileToUint(const base::FilePath& path, unsigned int* value) {
  std::string s;
  DCHECK(value);
  if (!base::ReadFileToString(path, &s, kMaxAsciiUintLength)) {
    LOG(ERROR) << "Failed to read " << path.value();
    return false;
  }
  base::TrimWhitespaceASCII(s, base::TRIM_ALL, &s);
  if (!base::StringToUint(s, value)) {
    LOG(ERROR) << "Failed to parse \"" << s << "\" from " << path.value();
    return false;
  }
  return true;
}

bool DetectAndReadAccelerometerConfiguration(
    scoped_refptr<AccelerometerReader::Configuration> configuration) {
  // Check for accelerometer symlink which will be created by the udev rules
  // file on detecting the device.
  base::FilePath device;
  if (!base::ReadSymbolicLink(base::FilePath(kAccelerometerDevicePath),
                              &device)) {
    return false;
  }

  if (!base::PathExists(base::FilePath(kAccelerometerTriggerPath))) {
    LOG(ERROR) << "Accelerometer trigger does not exist at"
               << kAccelerometerTriggerPath;
    return false;
  }

  base::FilePath iio_path(base::FilePath(kAccelerometerIioBasePath).Append(
      device));
  // Read accelerometer scales
  if (!ReadFileToUint(iio_path.Append(kAccelerometerBaseScaleName),
                      &(configuration->data.base_scale))) {
    return false;
  }
  if (!ReadFileToUint(iio_path.Append(kAccelerometerLidScaleName),
                      &(configuration->data.lid_scale))) {
    return false;
  }

  // Read indices of each accelerometer axis from each accelerometer from
  // /sys/bus/iio/devices/iio:deviceX/scan_elements/in_accel_{x,y,z}_%s_index
  for (size_t i = 0; i < arraysize(kAccelerometerNames); ++i) {
    for (size_t j = 0; j < arraysize(kAccelerometerAxes); ++j) {
      std::string accelerometer_index_path = base::StringPrintf(
          kAccelerometerScanIndexPath, kAccelerometerAxes[j],
          kAccelerometerNames[i]);
      unsigned int index = 0;
      if (!ReadFileToUint(iio_path.Append(accelerometer_index_path.c_str()),
                          &index)) {
        return false;
      }
      if (index >= kTriggerDataValues) {
        LOG(ERROR) << "Field index from " << accelerometer_index_path
                   << " out of bounds: " << index;
        return false;
      }
      configuration->data.index.push_back(index);
    }
  }
  return true;
}

bool ReadAccelerometer(
    scoped_refptr<AccelerometerReader::Reading> reading) {
  // Initiate the trigger to read accelerometers simultaneously
  int bytes_written = base::WriteFile(
      base::FilePath(kAccelerometerTriggerPath), "1\n", 2);
  if (bytes_written < 2) {
    PLOG(ERROR) << "Accelerometer trigger failure: " << bytes_written;
    return false;
  }

  // Read resulting sample from /dev/cros-ec-accel.
  int bytes_read = base::ReadFile(base::FilePath(kAccelerometerDevicePath),
                                  reading->data, kTriggerDataLength);
  if (bytes_read < static_cast<int>(kTriggerDataLength)) {
    LOG(ERROR) << "Read " << bytes_read << " byte(s), expected "
               << kTriggerDataLength << " bytes from accelerometer";
    return false;
  }
  return true;
}

}  // namespace

AccelerometerReader::ConfigurationData::ConfigurationData() {
}

AccelerometerReader::ConfigurationData::~ConfigurationData() {
}

AccelerometerReader::AccelerometerReader(
    base::TaskRunner* task_runner,
    AccelerometerReader::Delegate* delegate)
    : task_runner_(task_runner),
      delegate_(delegate),
      configuration_(new AccelerometerReader::Configuration()),
      weak_factory_(this) {
  DCHECK(task_runner_);
  DCHECK(delegate_);
  // Asynchronously detect and initialize the accelerometer to avoid delaying
  // startup.
  base::PostTaskAndReplyWithResult(task_runner_.get(), FROM_HERE,
      base::Bind(&DetectAndReadAccelerometerConfiguration, configuration_),
      base::Bind(&AccelerometerReader::OnInitialized,
                 weak_factory_.GetWeakPtr(), configuration_));
}

AccelerometerReader::~AccelerometerReader() {
}

void AccelerometerReader::OnInitialized(
    scoped_refptr<AccelerometerReader::Configuration> configuration,
    bool success) {
  if (success)
    TriggerRead();
}

void AccelerometerReader::TriggerRead() {
  DCHECK(!task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<AccelerometerReader::Reading> reading(
      new AccelerometerReader::Reading());
  base::PostTaskAndReplyWithResult(task_runner_, FROM_HERE,
      base::Bind(&ReadAccelerometer, reading),
      base::Bind(&AccelerometerReader::OnDataRead,
          weak_factory_.GetWeakPtr(), reading));
}

void AccelerometerReader::OnDataRead(
    scoped_refptr<AccelerometerReader::Reading> reading,
    bool success) {
  DCHECK(!task_runner_->RunsTasksOnCurrentThread());

  if (success) {
    gfx::Vector3dF base_reading, lid_reading;
    int16* values = reinterpret_cast<int16*>(reading->data);
    base_reading.set_x(values[configuration_->data.index[0]]);
    base_reading.set_y(values[configuration_->data.index[1]]);
    base_reading.set_z(values[configuration_->data.index[2]]);
    base_reading.Scale(1.0f / configuration_->data.base_scale);

    lid_reading.set_x(values[configuration_->data.index[3]]);
    lid_reading.set_y(values[configuration_->data.index[4]]);
    lid_reading.set_z(values[configuration_->data.index[5]]);
    lid_reading.Scale(1.0f / configuration_->data.lid_scale);
    delegate_->HandleAccelerometerReading(base_reading, lid_reading);
  }

  // Trigger another read after the current sampling delay.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AccelerometerReader::TriggerRead,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDelayBetweenReadsMs));
}

}  // namespace chromeos
