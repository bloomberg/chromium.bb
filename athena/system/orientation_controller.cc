// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"
#include "athena/system/orientation_controller.h"
#include "base/bind.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"

namespace athena {

namespace {

// Threshold after which to rotate in a given direction.
const int kGravityThreshold = 6.0f;

}  // namespace

OrientationController::OrientationController() {
}

void OrientationController::InitWith(
    scoped_refptr<base::TaskRunner> blocking_task_runner) {
  accelerometer_reader_.reset(
      new chromeos::AccelerometerReader(blocking_task_runner, this));
}

OrientationController::~OrientationController() {
}

void OrientationController::Shutdown() {
  accelerometer_reader_.reset();
}

void OrientationController::HandleAccelerometerUpdate(
    const ui::AccelerometerUpdate& update) {
  if (!update.has(ui::ACCELEROMETER_SOURCE_SCREEN))
    return;

  float gravity_x = update.get(ui::ACCELEROMETER_SOURCE_SCREEN).x();
  float gravity_y = update.get(ui::ACCELEROMETER_SOURCE_SCREEN).y();
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

  if (rotation == current_rotation_)
    return;

  current_rotation_ = rotation;
  ScreenManager::Get()->SetRotation(rotation);
}

}  // namespace athena
