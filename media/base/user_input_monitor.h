// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_USER_INPUT_MONITOR_H_
#define MEDIA_BASE_USER_INPUT_MONITOR_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// Monitors and notifies about keyboard events.
// Thread safe.
class MEDIA_EXPORT UserInputMonitor {
 public:
  UserInputMonitor();
  virtual ~UserInputMonitor();

  // Creates a platform-specific instance of UserInputMonitor.
  // |io_task_runner| is the task runner for an IO thread.
  // |ui_task_runner| is the task runner for a UI thread.
  static std::unique_ptr<UserInputMonitor> Create(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  // A caller must call EnableKeyPressMonitoring and
  // DisableKeyPressMonitoring in pair.
  void EnableKeyPressMonitoring();
  void DisableKeyPressMonitoring();

  // Returns the number of keypresses. The starting point from when it is
  // counted is not guaranteed, but consistent within the pair of calls of
  // EnableKeyPressMonitoring and DisableKeyPressMonitoring. So a caller can
  // use the difference between the values returned at two times to get the
  // number of keypresses happened within that time period, but should not make
  // any assumption on the initial value.
  virtual size_t GetKeyPressCount() const = 0;

 private:
  virtual void StartKeyboardMonitoring() = 0;
  virtual void StopKeyboardMonitoring() = 0;

  base::Lock lock_;
  size_t key_press_counter_references_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitor);
};

}  // namespace media

#endif  // MEDIA_BASE_USER_INPUT_MONITOR_H_
