// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_ORIENTATION_CONTROLLER_H_
#define ATHENA_SYSTEM_ORIENTATION_CONTROLLER_H_

#include "athena/system/device_socket_listener.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/display.h"

namespace base {
class FilePath;
class FilePathWatcher;
class TaskRunner;
}

namespace athena {

// Monitors accelerometers, detecting orientation changes. When a change is
// detected rotates the root window to match.
class OrientationController
    : public DeviceSocketListener,
      public base::RefCountedThreadSafe<OrientationController> {
 public:
  OrientationController(scoped_refptr<base::TaskRunner> io_task_runner);

 private:
  friend class base::RefCountedThreadSafe<OrientationController>;

  virtual ~OrientationController();

  // Watch for the socket path to be created, called on the IO thread.
  void WatchForSocketPathOnIO();
  void OnFilePathChangedOnIO(const base::FilePath& path, bool error);

  // Overridden from device::DeviceSocketListener:
  virtual void OnDataAvailableOnIO(const void* data) OVERRIDE;

  // Rotates the display to |rotation|, called on the UI thread.
  void RotateOnUI(gfx::Display::Rotation rotation);

  // The last configured rotation.
  gfx::Display::Rotation current_rotation_;

  // The timestamp of the last applied orientation change.
  int64_t last_orientation_change_time_;

  // A task runner for the UI thread.
  scoped_refptr<base::TaskRunner> ui_task_runner_;

  // File path watcher used to detect when sensors are present.
  scoped_ptr<base::FilePathWatcher> watcher_;

  base::WeakPtrFactory<OrientationController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OrientationController);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_ORIENTATION_CONTROLLER_H_
