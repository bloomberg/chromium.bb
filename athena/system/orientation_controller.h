// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_ORIENTATION_CONTROLLER_H_
#define ATHENA_SYSTEM_ORIENTATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "ui/gfx/display.h"

namespace base {
class TaskRunner;
}

namespace athena {

// Monitors accelerometers, detecting orientation changes. When a change is
// detected rotates the root window to match.
class OrientationController
    : public chromeos::AccelerometerReader::Delegate {
 public:
  OrientationController();
  virtual ~OrientationController();

  void InitWith(scoped_refptr<base::TaskRunner> blocking_task_runner);
  void Shutdown();

  // chromeos::AccelerometerReader::Delegate
  virtual void HandleAccelerometerUpdate(
      const ui::AccelerometerUpdate& update) OVERRIDE;

 private:
  // The last configured rotation.
  gfx::Display::Rotation current_rotation_;

  scoped_ptr<chromeos::AccelerometerReader> accelerometer_reader_;

  DISALLOW_COPY_AND_ASSIGN(OrientationController);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_ORIENTATION_CONTROLLER_H_
