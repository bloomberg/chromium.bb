// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_reader_linux.h"

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/platform_sensor_linux.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"

namespace device {

class PollingSensorReader : public SensorReader {
 public:
  PollingSensorReader(
      const SensorInfoLinux* sensor_device,
      PlatformSensorLinux* sensor,
      scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner);
  ~PollingSensorReader() override;

  // SensorReader implements:
  bool StartFetchingData(
      const PlatformSensorConfiguration& configuration) override;
  void StopFetchingData() override;

 private:
  // Initializes a read timer.
  void InitializeTimer(const PlatformSensorConfiguration& configuration);

  // Polls data and sends it to a |sensor_|.
  void PollForData();

  // Paths to sensor read files.
  const std::vector<base::FilePath> sensor_file_paths_;

  // Scaling value that are applied to raw data from sensors.
  const double scaling_value_;

  // Offset value.
  const double offset_value_;

  // Used to apply scalings and invert signs if needed.
  const SensorPathsLinux::ReaderFunctor apply_scaling_func_;

  // Owned pointer to a timer. Will be deleted on a polling thread once
  // destructor is called.
  base::RepeatingTimer* timer_;

  base::WeakPtrFactory<PollingSensorReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PollingSensorReader);
};

PollingSensorReader::PollingSensorReader(
    const SensorInfoLinux* sensor_device,
    PlatformSensorLinux* sensor,
    scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner)
    : SensorReader(sensor, polling_task_runner),
      sensor_file_paths_(sensor_device->device_reading_files),
      scaling_value_(sensor_device->device_scaling_value),
      offset_value_(sensor_device->device_offset_value),
      apply_scaling_func_(sensor_device->apply_scaling_func),
      timer_(new base::RepeatingTimer()),
      weak_factory_(this) {}

PollingSensorReader::~PollingSensorReader() {
  polling_task_runner_->DeleteSoon(FROM_HERE, timer_);
}

bool PollingSensorReader::StartFetchingData(
    const PlatformSensorConfiguration& configuration) {
  if (is_reading_active_)
    StopFetchingData();

  return polling_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PollingSensorReader::InitializeTimer,
                            weak_factory_.GetWeakPtr(), configuration));
}

void PollingSensorReader::StopFetchingData() {
  is_reading_active_ = false;
  timer_->Stop();
}

void PollingSensorReader::InitializeTimer(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(polling_task_runner_->BelongsToCurrentThread());
  timer_->Start(FROM_HERE, base::TimeDelta::FromMicroseconds(
                               base::Time::kMicrosecondsPerSecond /
                               configuration.frequency()),
                this, &PollingSensorReader::PollForData);
  is_reading_active_ = true;
}

void PollingSensorReader::PollForData() {
  DCHECK(polling_task_runner_->BelongsToCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  SensorReading readings;
  DCHECK_LE(sensor_file_paths_.size(), arraysize(readings.values));
  int i = 0;
  for (const auto& path : sensor_file_paths_) {
    std::string new_read_value;
    if (!base::ReadFileToString(path, &new_read_value)) {
      NotifyReadError();
      StopFetchingData();
      return;
    }

    double new_value = 0;
    base::TrimWhitespaceASCII(new_read_value, base::TRIM_ALL, &new_read_value);
    if (!base::StringToDouble(new_read_value, &new_value)) {
      NotifyReadError();
      StopFetchingData();
      return;
    }
    readings.values[i++] = new_value;
  }
  if (!apply_scaling_func_.is_null())
    apply_scaling_func_.Run(scaling_value_, offset_value_, readings);

  if (is_reading_active_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&PlatformSensorLinux::UpdatePlatformSensorReading,
                              base::Unretained(sensor_), readings));
  }
}

// static
std::unique_ptr<SensorReader> SensorReader::Create(
    const SensorInfoLinux* sensor_device,
    PlatformSensorLinux* sensor,
    scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner) {
  // TODO(maksims): implement triggered reading. At the moment,
  // only polling read is supported.
  return base::MakeUnique<PollingSensorReader>(sensor_device, sensor,
                                               polling_thread_task_runner);
}

SensorReader::SensorReader(
    PlatformSensorLinux* sensor,
    scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner)
    : sensor_(sensor),
      polling_task_runner_(polling_task_runner),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      is_reading_active_(false) {}

SensorReader::~SensorReader() = default;

void SensorReader::NotifyReadError() {
  if (is_reading_active_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&PlatformSensorLinux::NotifyPlatformSensorError,
                              base::Unretained(sensor_)));
  }
}

}  // namespace device
