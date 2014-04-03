// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_
#define CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace base {
class TaskRunner;
}

namespace chromeos {

// Reads an accelerometer device and reports data back to an
// AccelerometerDelegate.
class CHROMEOS_EXPORT AccelerometerReader {
 public:
  // Configuration structure for accelerometer device.
  struct ConfigurationData {
    ConfigurationData();
    ~ConfigurationData();

    // Scale of accelerometers (i.e. raw value * 1.0f / scale = G's).
    unsigned int base_scale;
    unsigned int lid_scale;

    // Index of each accelerometer axis in data stream.
    std::vector<unsigned int> index;
  };
  typedef base::RefCountedData<ConfigurationData> Configuration;
  typedef base::RefCountedData<char[12]> Reading;

  // An interface to receive data from the AccelerometerReader.
  class Delegate {
   public:
    virtual void HandleAccelerometerReading(const gfx::Vector3dF& base,
                                            const gfx::Vector3dF& lid) = 0;
  };

  AccelerometerReader(base::TaskRunner* blocking_task_runner,
                      Delegate* delegate);
  ~AccelerometerReader();

 private:
  // Dispatched when initialization is complete. If |success|, |configuration|
  // provides the details of the detected accelerometer.
  void OnInitialized(scoped_refptr<Configuration> configuration, bool success);

  // Triggers an asynchronous read from the accelerometer, signalling
  // OnDataRead with the result.
  void TriggerRead();

  // If |success|, converts the raw reading to a pair of Vector3dF
  // values and notifies the |delegate_| with the new readings.
  // Triggers another read from the accelerometer at the current sampling rate.
  void OnDataRead(scoped_refptr<Reading> reading, bool success);

  // The task runner to use for blocking tasks.
  scoped_refptr<base::TaskRunner> task_runner_;

  // A weak pointer to the delegate to send accelerometer readings to.
  Delegate* delegate_;

  // The accelerometer configuration.
  scoped_refptr<Configuration> configuration_;

  base::WeakPtrFactory<AccelerometerReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccelerometerReader);
};

}  // namespace chromeos

#endif  // CHROMEOS_ACCELEROMETER_ACCELEROMETER_READER_H_
