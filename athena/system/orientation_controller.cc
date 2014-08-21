// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"
#include "athena/system/orientation_controller.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path_watcher.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"

namespace athena {

namespace {

// Path of the socket which the sensor daemon creates.
const char kSocketPath[] = "/dev/sensors/orientation";

// Threshold after which to rotate in a given direction.
const int kGravityThreshold = 6.0f;

// Minimum delay before triggering another orientation change.
const int kOrientationChangeDelayNS = 500000000;

enum SensorType {
  SENSOR_ACCELEROMETER,
  SENSOR_LIGHT,
  SENSOR_PROXIMITY
};

// A sensor event from the device.
struct DeviceSensorEvent {
  // The type of event from the SensorType enum above.
  int32_t type;

  // The time in nanoseconds at which the event happened.
  int64_t timestamp;

  union {
    // Accelerometer X,Y,Z values in SI units (m/s^2) including gravity.
    // The orientation is described at
    // http://www.html5rocks.com/en/tutorials/device/orientation/.
    float data[3];

    // Ambient (room) temperature in degrees Celcius.
    float temperature;

    // Proximity sensor distance in centimeters.
    float distance;

    // Ambient light level in SI lux units.
    float light;
  };
};

}  // namespace

OrientationController::OrientationController()
    : DeviceSocketListener(kSocketPath, sizeof(DeviceSensorEvent)),
      last_orientation_change_time_(0),
      shutdown_(false) {
  CHECK(base::MessageLoopForUI::current());
  ui_task_runner_ = base::MessageLoopForUI::current()->task_runner();
}

void OrientationController::InitWith(
    scoped_refptr<base::TaskRunner> file_task_runner) {
  file_task_runner_ = file_task_runner;
  file_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&OrientationController::WatchForSocketPathOnFILE, this));
}

OrientationController::~OrientationController() {
}

void OrientationController::Shutdown() {
  CHECK(file_task_runner_);
  StopListening();
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OrientationController::ShutdownOnFILE, this));
}

void OrientationController::ShutdownOnFILE() {
  shutdown_ = true;
  watcher_.reset();
}

void OrientationController::WatchForSocketPathOnFILE() {
  CHECK(base::MessageLoopForIO::current());
  if (base::PathExists(base::FilePath(kSocketPath))) {
    ui_task_runner_->PostTask(FROM_HERE,
        base::Bind(&OrientationController::StartListening, this));
  } else {
    watcher_.reset(new base::FilePathWatcher);
    watcher_->Watch(
        base::FilePath(kSocketPath),
        false,
        base::Bind(&OrientationController::OnFilePathChangedOnFILE, this));
  }
}

void OrientationController::OnFilePathChangedOnFILE(const base::FilePath& path,
                                                    bool error) {
  watcher_.reset();
  if (error || shutdown_)
    return;

  ui_task_runner_->PostTask(FROM_HERE,
      base::Bind(&OrientationController::StartListening, this));
}

void OrientationController::OnDataAvailableOnFILE(const void* data) {
  const DeviceSensorEvent* event =
      static_cast<const DeviceSensorEvent*>(data);
  if (event->type != SENSOR_ACCELEROMETER)
    return;

  float gravity_x = event->data[0];
  float gravity_y = event->data[1];
  gfx::Display::Rotation rotation;
  if (gravity_x < -kGravityThreshold) {
    rotation = gfx::Display::ROTATE_270;
  } else if (gravity_x > kGravityThreshold) {
    rotation = gfx::Display::ROTATE_90;
  } else if (gravity_y < -kGravityThreshold) {
    rotation = gfx::Display::ROTATE_180;
  } else if (gravity_y > kGravityThreshold) {
    rotation = gfx::Display::ROTATE_0;
  } else {
    // No rotation as gravity threshold was not hit.
    return;
  }

  if (rotation == current_rotation_ ||
      event->timestamp - last_orientation_change_time_ <
          kOrientationChangeDelayNS) {
    return;
  }

  last_orientation_change_time_ = event->timestamp;
  current_rotation_ = rotation;
  ui_task_runner_->PostTask(FROM_HERE,
      base::Bind(&OrientationController::RotateOnUI, this, rotation));
}

void OrientationController::RotateOnUI(gfx::Display::Rotation rotation) {
  ScreenManager* screen_manager = ScreenManager::Get();
  // Since this is called from the FILE thread, the screen manager may no longer
  // exist.
  if (screen_manager)
    screen_manager->SetRotation(rotation);
}

}  // namespace athena
