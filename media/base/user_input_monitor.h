// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_USER_INPUT_MONITOR_H_
#define MEDIA_BASE_USER_INPUT_MONITOR_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

struct SkIPoint;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// Monitors and notifies about mouse movements and keyboard events.
// Thread safe. The thread on which the listenters are called is not guaranteed.
// The callers should not perform expensive/blocking tasks in the callback since
// it might be called on the browser UI/IO threads.
// The object must outlive the browser UI/IO threads to make sure the callbacks
// will not access a deleted object.
class MEDIA_EXPORT UserInputMonitor {
 public:
  // The interface to receive mouse movement events.
  class MEDIA_EXPORT MouseEventListener {
   public:
    // |position| is the new mouse position.
    virtual void OnMouseMoved(const SkIPoint& position) = 0;

   protected:
    virtual ~MouseEventListener() {}
  };

  UserInputMonitor();
  virtual ~UserInputMonitor();

  // Creates a platform-specific instance of UserInputMonitor.
  // |io_task_runner| is the task runner for an IO thread.
  // |ui_task_runner| is the task runner for a UI thread.
  static scoped_ptr<UserInputMonitor> Create(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  // The same |listener| should only be added once.
  // The clients should make sure to call Remove*Listener before |listener| is
  // destroyed.
  void AddMouseListener(MouseEventListener* listener);
  void RemoveMouseListener(MouseEventListener* listener);

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

 protected:
  // Called by the platform-specific sub-classes to propagate the events to the
  // listeners.
  void OnMouseEvent(const SkIPoint& position);

 private:
  virtual void StartMouseMonitoring() = 0;
  virtual void StopMouseMonitoring() = 0;
  virtual void StartKeyboardMonitoring() = 0;
  virtual void StopKeyboardMonitoring() = 0;

  base::Lock lock_;
  ObserverList<MouseEventListener, true> mouse_listeners_;
  bool monitoring_mouse_;
  size_t key_press_counter_references_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitor);
};

}  // namespace media

#endif  // MEDIA_BASE_USER_INPUT_MONITOR_H_
