// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/accelerometer/accelerometer_reader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
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

// File within the device in kAccelerometerIioBasePath containing the scale of
// the accelerometers.
const base::FilePath::CharType kScaleNameFormatString[] = "in_accel_%s_scale";

// The filename giving the path to read the scan index of each accelerometer
// axis.
const char kAccelerometerScanIndexPath[] =
    "scan_elements/in_accel_%s_%s_index";

// The names of the accelerometers. Matches up with the enum AccelerometerSource
// in ui/accelerometer/accelerometer_types.h.
const char kAccelerometerNames[ui::ACCELEROMETER_SOURCE_COUNT][5] = {
    "lid", "base"};

// The axes on each accelerometer.
const char kAccelerometerAxes[][2] = {"y", "x", "z"};

// The length required to read uint values from configuration files.
const size_t kMaxAsciiUintLength = 21;

// The size of individual values.
const size_t kDataSize = 2;

// The time to wait between reading the accelerometer.
const int kDelayBetweenReadsMs = 100;

// The mean acceleration due to gravity on Earth in m/s^2.
const float kMeanGravity = 9.80665f;

// Reads |path| to the unsigned int pointed to by |value|. Returns true on
// success or false on failure.
bool ReadFileToInt(const base::FilePath& path, int* value) {
  std::string s;
  DCHECK(value);
  if (!base::ReadFileToString(path, &s, kMaxAsciiUintLength)) {
    return false;
  }
  base::TrimWhitespaceASCII(s, base::TRIM_ALL, &s);
  if (!base::StringToInt(s, value)) {
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
  // Read configuration of each accelerometer axis from each accelerometer from
  // /sys/bus/iio/devices/iio:deviceX/.
  for (size_t i = 0; i < arraysize(kAccelerometerNames); ++i) {
    // Read scale of accelerometer.
    std::string accelerometer_scale_path = base::StringPrintf(
        kScaleNameFormatString, kAccelerometerNames[i]);
    int scale_divisor;
    if (!ReadFileToInt(iio_path.Append(accelerometer_scale_path.c_str()),
        &scale_divisor)) {
      configuration->data.has[i] = false;
      continue;
    }

    configuration->data.has[i] = true;
    configuration->data.count++;
    for (size_t j = 0; j < arraysize(kAccelerometerAxes); ++j) {
      configuration->data.scale[i][j] = kMeanGravity / scale_divisor;
      std::string accelerometer_index_path = base::StringPrintf(
          kAccelerometerScanIndexPath, kAccelerometerAxes[j],
          kAccelerometerNames[i]);
      if (!ReadFileToInt(iio_path.Append(accelerometer_index_path.c_str()),
                         &(configuration->data.index[i][j]))) {
        return false;
      }
    }
  }

  // Adjust the directions of accelerometers to match the AccelerometerUpdate
  // type specified in ui/accelerometer/accelerometer_types.h.
  configuration->data.scale[ui::ACCELEROMETER_SOURCE_SCREEN][0] *= -1.0f;
  for (int i = 0; i < 3; ++i) {
    configuration->data.scale[ui::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD][i] *=
        -1.0f;
  }

  // Verify indices are within bounds.
  for (int i = 0; i < ui::ACCELEROMETER_SOURCE_COUNT; ++i) {
    if (!configuration->data.has[i])
      continue;
    for (int j = 0; j < 3; ++j) {
      if (configuration->data.index[i][j] < 0 ||
          configuration->data.index[i][j] >=
              3 * static_cast<int>(configuration->data.count)) {
        LOG(ERROR) << "Field index for " << kAccelerometerNames[i] << " "
                   << kAccelerometerAxes[j] << " axis out of bounds.";
        return false;
      }
    }
  }
  configuration->data.length = kDataSize * 3 * configuration->data.count;
  return true;
}

bool ReadAccelerometer(
    scoped_refptr<AccelerometerReader::Reading> reading,
    size_t length) {
  // Initiate the trigger to read accelerometers simultaneously
  int bytes_written = base::WriteFile(
      base::FilePath(kAccelerometerTriggerPath), "1\n", 2);
  if (bytes_written < 2) {
    PLOG(ERROR) << "Accelerometer trigger failure: " << bytes_written;
    return false;
  }

  // Read resulting sample from /dev/cros-ec-accel.
  int bytes_read = base::ReadFile(base::FilePath(kAccelerometerDevicePath),
                                  reading->data, length);
  if (bytes_read < static_cast<int>(length)) {
    LOG(ERROR) << "Read " << bytes_read << " byte(s), expected "
               << length << " bytes from accelerometer";
    return false;
  }
  return true;
}

}  // namespace

AccelerometerReader::ConfigurationData::ConfigurationData()
    : count(0) {
  for (int i = 0; i < ui::ACCELEROMETER_SOURCE_COUNT; ++i) {
    has[i] = false;
    for (int j = 0; j < 3; ++j) {
      scale[i][j] = 0;
      index[i][j] = -1;
    }
  }
}

AccelerometerReader::ConfigurationData::~ConfigurationData() {
}

AccelerometerReader::AccelerometerReader(
    scoped_refptr<base::TaskRunner> blocking_task_runner,
    AccelerometerReader::Delegate* delegate)
    : task_runner_(blocking_task_runner),
      delegate_(delegate),
      configuration_(new AccelerometerReader::Configuration()),
      weak_factory_(this) {
  DCHECK(task_runner_.get());
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
  base::PostTaskAndReplyWithResult(task_runner_.get(),
                                   FROM_HERE,
                                   base::Bind(&ReadAccelerometer, reading,
                                              configuration_->data.length),
                                   base::Bind(&AccelerometerReader::OnDataRead,
                                              weak_factory_.GetWeakPtr(),
                                              reading));
}

void AccelerometerReader::OnDataRead(
    scoped_refptr<AccelerometerReader::Reading> reading,
    bool success) {
  DCHECK(!task_runner_->RunsTasksOnCurrentThread());

  if (success) {
    for (int i = 0; i < ui::ACCELEROMETER_SOURCE_COUNT; ++i) {
      if (!configuration_->data.has[i])
        continue;

      int16* values = reinterpret_cast<int16*>(reading->data);
      update_.Set(static_cast<ui::AccelerometerSource>(i),
                  values[configuration_->data.index[i][0]] *
                      configuration_->data.scale[i][0],
                  values[configuration_->data.index[i][1]] *
                      configuration_->data.scale[i][1],
                  values[configuration_->data.index[i][2]] *
                      configuration_->data.scale[i][2]);
    }
    delegate_->HandleAccelerometerUpdate(update_);
  }

  // Trigger another read after the current sampling delay.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AccelerometerReader::TriggerRead,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDelayBetweenReadsMs));
}

}  // namespace chromeos
