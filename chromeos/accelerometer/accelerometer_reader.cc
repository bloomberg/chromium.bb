// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/accelerometer/accelerometer_reader.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
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
// in chromeos/accelerometer/accelerometer_types.h.
const char kAccelerometerNames[ACCELEROMETER_SOURCE_COUNT][5] = {"lid", "base"};

// The axes on each accelerometer.
const char kAccelerometerAxes[][2] = {"y", "x", "z"};

// The length required to read uint values from configuration files.
const size_t kMaxAsciiUintLength = 21;

// The size of individual values.
const size_t kDataSize = 2;

// The mean acceleration due to gravity on Earth in m/s^2.
const float kMeanGravity = 9.80665f;

// The number of accelerometers.
const int kNumberOfAccelerometers = 2;

// The number of axes for which there are acceleration readings.
const int kNumberOfAxes = 3;

// The size of data in one reading of the accelerometers.
const int kSizeOfReading = kDataSize * kNumberOfAccelerometers * kNumberOfAxes;

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

}  // namespace

const int AccelerometerReader::kDelayBetweenReadsMs = 100;

// Work that runs on a base::TaskRunner. It determines the accelerometer
// configuartion, and reads the data. Upon a successful read it will notify
// all observers.
class AccelerometerFileReader
    : public base::RefCountedThreadSafe<AccelerometerFileReader> {
 public:
  AccelerometerFileReader();

  // Detects the accelerometer configuration, if an accelerometer is available
  // triggers reads.
  void Initialize(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Attempts to read the accelerometer data. Upon a success, converts the raw
  // reading to an AccelerometerUpdate and notifies observers. Triggers another
  // read at the current sampling rate.
  void Read();

  // Add/Remove observers.
  void AddObserver(AccelerometerReader::Observer* observer);
  void RemoveObserver(AccelerometerReader::Observer* observer);

 private:
  friend class base::RefCountedThreadSafe<AccelerometerFileReader>;

  // Configuration structure for accelerometer device.
  struct ConfigurationData {
    ConfigurationData();
    ~ConfigurationData();

    // Number of accelerometers on device.
    size_t count;

    // Length of accelerometer updates.
    size_t length;

    // Which accelerometers are present on device.
    bool has[ACCELEROMETER_SOURCE_COUNT];

    // Scale of accelerometers (i.e. raw value * scale = m/s^2).
    float scale[ACCELEROMETER_SOURCE_COUNT][3];

    // Index of each accelerometer axis in data stream.
    int index[ACCELEROMETER_SOURCE_COUNT][3];
  };

  ~AccelerometerFileReader() {}

  // Attempts to read the accelerometer data. Upon a success, converts the raw
  // reading to an AccelerometerUpdate and notifies observers.
  void ReadFileAndNotify();

  // True if Initialize completed successfully, and there is an accelerometer
  // file to read.
  bool initialization_successful_;

  // The accelerometer configuration.
  ConfigurationData configuration_;

  // The observers to notify of accelerometer updates.
  scoped_refptr<base::ObserverListThreadSafe<AccelerometerReader::Observer>>
      observers_;

  // The task runner to use for blocking tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The last seen accelerometer data.
  scoped_refptr<AccelerometerUpdate> update_;

  DISALLOW_COPY_AND_ASSIGN(AccelerometerFileReader);
};

AccelerometerFileReader::AccelerometerFileReader()
    : initialization_successful_(false),
      observers_(
          new base::ObserverListThreadSafe<AccelerometerReader::Observer>()) {
}

void AccelerometerFileReader::Initialize(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  DCHECK(
      base::SequencedWorkerPool::GetSequenceTokenForCurrentThread().IsValid());
  task_runner_ = sequenced_task_runner;

  // Check for accelerometer symlink which will be created by the udev rules
  // file on detecting the device.
  base::FilePath device;
  if (!base::ReadSymbolicLink(base::FilePath(kAccelerometerDevicePath),
                              &device)) {
    return;
  }

  if (!base::PathExists(base::FilePath(kAccelerometerTriggerPath))) {
    LOG(ERROR) << "Accelerometer trigger does not exist at"
               << kAccelerometerTriggerPath;
    return;
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
      configuration_.has[i] = false;
      continue;
    }

    configuration_.has[i] = true;
    configuration_.count++;
    for (size_t j = 0; j < arraysize(kAccelerometerAxes); ++j) {
      configuration_.scale[i][j] = kMeanGravity / scale_divisor;
      std::string accelerometer_index_path = base::StringPrintf(
          kAccelerometerScanIndexPath, kAccelerometerAxes[j],
          kAccelerometerNames[i]);
      if (!ReadFileToInt(iio_path.Append(accelerometer_index_path.c_str()),
                         &(configuration_.index[i][j]))) {
        return;
      }
    }
  }

  // Adjust the directions of accelerometers to match the AccelerometerUpdate
  // type specified in chromeos/accelerometer/accelerometer_types.h.
  configuration_.scale[ACCELEROMETER_SOURCE_SCREEN][0] *= -1.0f;
  for (int i = 0; i < 3; ++i) {
    configuration_.scale[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD][i] *= -1.0f;
  }

  // Verify indices are within bounds.
  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    if (!configuration_.has[i])
      continue;
    for (int j = 0; j < 3; ++j) {
      if (configuration_.index[i][j] < 0 ||
          configuration_.index[i][j] >=
              3 * static_cast<int>(configuration_.count)) {
        LOG(ERROR) << "Field index for " << kAccelerometerNames[i] << " "
                   << kAccelerometerAxes[j] << " axis out of bounds.";
        return;
      }
    }
  }
  configuration_.length = kDataSize * 3 * configuration_.count;
  initialization_successful_ = true;
  Read();
}

void AccelerometerFileReader::Read() {
  DCHECK(
      base::SequencedWorkerPool::GetSequenceTokenForCurrentThread().IsValid());
  ReadFileAndNotify();
  task_runner_->PostNonNestableDelayedTask(
      FROM_HERE, base::Bind(&AccelerometerFileReader::Read, this),
      base::TimeDelta::FromMilliseconds(
          AccelerometerReader::kDelayBetweenReadsMs));
}

void AccelerometerFileReader::AddObserver(
    AccelerometerReader::Observer* observer) {
  observers_->AddObserver(observer);
  if (initialization_successful_) {
    task_runner_->PostNonNestableTask(
        FROM_HERE,
        base::Bind(&AccelerometerFileReader::ReadFileAndNotify, this));
  }
}

void AccelerometerFileReader::RemoveObserver(
    AccelerometerReader::Observer* observer) {
  observers_->RemoveObserver(observer);
}

void AccelerometerFileReader::ReadFileAndNotify() {
  DCHECK(initialization_successful_);
  char reading[kSizeOfReading];

  // Initiate the trigger to read accelerometers simultaneously
  int bytes_written = base::WriteFile(
      base::FilePath(kAccelerometerTriggerPath), "1\n", 2);
  if (bytes_written < 2) {
    PLOG(ERROR) << "Accelerometer trigger failure: " << bytes_written;
    return;
  }

  // Read resulting sample from /dev/cros-ec-accel.
  int bytes_read = base::ReadFile(base::FilePath(kAccelerometerDevicePath),
                                  reading, configuration_.length);
  if (bytes_read < static_cast<int>(configuration_.length)) {
    LOG(ERROR) << "Read " << bytes_read << " byte(s), expected "
               << configuration_.length << " bytes from accelerometer";
    return;
  }

  update_ = new AccelerometerUpdate();

  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    if (!configuration_.has[i])
      continue;

    int16* values = reinterpret_cast<int16*>(reading);
    update_->Set(
        static_cast<AccelerometerSource>(i),
        values[configuration_.index[i][0]] * configuration_.scale[i][0],
        values[configuration_.index[i][1]] * configuration_.scale[i][1],
        values[configuration_.index[i][2]] * configuration_.scale[i][2]);
  }

  observers_->Notify(FROM_HERE,
                     &AccelerometerReader::Observer::OnAccelerometerUpdated,
                     update_);
}

AccelerometerFileReader::ConfigurationData::ConfigurationData() : count(0) {
  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    has[i] = false;
    for (int j = 0; j < 3; ++j) {
      scale[i][j] = 0;
      index[i][j] = -1;
    }
  }
}

AccelerometerFileReader::ConfigurationData::~ConfigurationData() {
}

// static
AccelerometerReader* AccelerometerReader::GetInstance() {
  return Singleton<AccelerometerReader>::get();
}

void AccelerometerReader::Initialize(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  DCHECK(sequenced_task_runner.get());
  // Asynchronously detect and initialize the accelerometer to avoid delaying
  // startup.
  sequenced_task_runner->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&AccelerometerFileReader::Initialize,
                 accelerometer_file_reader_.get(), sequenced_task_runner));
}

void AccelerometerReader::AddObserver(Observer* observer) {
  accelerometer_file_reader_->AddObserver(observer);
}

void AccelerometerReader::RemoveObserver(Observer* observer) {
  accelerometer_file_reader_->RemoveObserver(observer);
}

AccelerometerReader::AccelerometerReader()
    : accelerometer_file_reader_(new AccelerometerFileReader()) {
}

AccelerometerReader::~AccelerometerReader() {
}

}  // namespace chromeos
