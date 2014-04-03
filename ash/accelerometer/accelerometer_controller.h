// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_CONTROLLER_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"

#if defined(OS_CHROMEOS)
#include "chromeos/accelerometer/accelerometer_reader.h"
#endif

namespace base {
class TaskRunner;
}

namespace gfx {
class Vector3dF;
}

namespace ash {

class AccelerometerObserver;

// This class owns the communication interface for talking to the accelerometer
// on supporting devices. Observers will be delivered OnAccelerometerRead
// notifications if an accelerometer was detected.
class ASH_EXPORT AccelerometerController
#if defined(OS_CHROMEOS)
    : public chromeos::AccelerometerReader::Delegate {
#else
    {
#endif
 public:
  AccelerometerController();
  virtual ~AccelerometerController();

  // Initialize the accelerometer reader.
  void Initialize(scoped_refptr<base::TaskRunner> blocking_task_runner);

  // Add/remove observer.
  void AddObserver(AccelerometerObserver* observer);
  void RemoveObserver(AccelerometerObserver* observer);

#if defined(OS_CHROMEOS)
  // This needs to be CHROMEOS only as on other platforms it does not actually
  // override a method.
  // chromeos::AccelerometerReader::Delegate:
  virtual void HandleAccelerometerReading(const gfx::Vector3dF& base,
                                          const gfx::Vector3dF& lid) OVERRIDE;
#endif

 private:
#if defined(OS_CHROMEOS)
  // The AccelerometerReader which directly triggers and reads from the
  // accelerometer device.
  scoped_ptr<chromeos::AccelerometerReader> reader_;
#endif

  ObserverList<AccelerometerObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(AccelerometerController);
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_CONTROLLER_H_
