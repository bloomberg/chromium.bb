// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_H_

#include <map>

#include "base/observer_list.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class Accelerator;
}

namespace extensions {

// Platform-neutral implementation of a class that keeps track of observers and
// monitors keystrokes. It relays messages to the appropriate observers when a
// global shortcut has been struck by the user.
class GlobalShortcutListener {
 public:
  class Observer {
   public:
    // Called when your global shortcut (|accelerator|) is struck.
    virtual void OnKeyPressed(const ui::Accelerator& accelerator) = 0;
  };

  virtual ~GlobalShortcutListener();

  static GlobalShortcutListener* GetInstance();

  // Implemented by platform-specific implementations of this class.
  virtual void StartListening() = 0;
  virtual void StopListening() = 0;

  // Register an observer for when a certain |accelerator| is struck.
  virtual void RegisterAccelerator(
      const ui::Accelerator& accelerator, Observer* observer);
  // Stop listening for the given |accelerator|.
  virtual void UnregisterAccelerator(
      const ui::Accelerator& accelerator, Observer* observer);

 protected:
  GlobalShortcutListener();

  // Called by platform specific implementations of this class whenever a key
  // is struck. Only called for keys that have observers registered.
  void NotifyKeyPressed(const ui::Accelerator& accelerator);

  // The map of accelerators that have been successfully registered as global
  // shortcuts and their observer lists.
  typedef ObserverList<Observer> Observers;
  typedef std::map< ui::Accelerator, Observers* > AcceleratorMap;
  AcceleratorMap accelerator_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalShortcutListener);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_H_
