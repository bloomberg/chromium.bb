// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_USER_INPUT_MONITOR_H_
#define MEDIA_BASE_USER_INPUT_MONITOR_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"

struct SkIPoint;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// Monitors and notifies about mouse movements and keyboard events.
// Thread safe. The thread on which the listenters are called is not guaranteed.
// The callers should not perform expensive/blocking tasks in the callback since
// it might be called on the browser UI/IO threads.
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
  // The interface to receive key stroke events.
  class MEDIA_EXPORT KeyStrokeListener {
   public:
    // Called when any key is pressed. Called only once until the key is
    // released, i.e. holding down a key for a long period will generate one
    // callback just when the key is pressed down.
    virtual void OnKeyStroke() = 0;

   protected:
    virtual ~KeyStrokeListener() {}
  };

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
  void AddKeyStrokeListener(KeyStrokeListener* listener);
  void RemoveKeyStrokeListener(KeyStrokeListener* listener);

 protected:
  UserInputMonitor();

  // Called by the platform-specific sub-classes to propagate the events to the
  // listeners.
  void OnMouseEvent(const SkIPoint& position);
  void OnKeyboardEvent(ui::EventType event, ui::KeyboardCode key_code);

 private:
  virtual void StartMouseMonitoring() = 0;
  virtual void StopMouseMonitoring() = 0;
  virtual void StartKeyboardMonitoring() = 0;
  virtual void StopKeyboardMonitoring() = 0;

  base::Lock lock_;
  ObserverList<MouseEventListener, true> mouse_listeners_;
  ObserverList<KeyStrokeListener, true> key_stroke_listeners_;
  bool monitoring_mouse_;
  bool monitoring_keyboard_;
  // The set of keys currently held down. Used for convering raw keyboard events
  // into KeyStrokeListener callbacks.
  std::set<ui::KeyboardCode> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitor);
};

}  // namespace media

#endif  // MEDIA_BASE_USER_INPUT_MONITOR_H_
