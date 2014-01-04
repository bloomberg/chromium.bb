// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_OZONE_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_OZONE_H_

#include "chrome/browser/extensions/global_shortcut_listener.h"

namespace extensions {

// Ozone-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts. Handles basic keyboard intercepting and
// forwards its output to the base class for processing.
class GlobalShortcutListenerOzone : public GlobalShortcutListener {
 public:
  GlobalShortcutListenerOzone();
  virtual ~GlobalShortcutListenerOzone();

 private:
  // GlobalShortcutListener implementation.
  virtual void StartListening() OVERRIDE;
  virtual void StopListening() OVERRIDE;
  virtual bool RegisterAcceleratorImpl(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual void UnregisterAcceleratorImpl(
      const ui::Accelerator& accelerator) OVERRIDE;

  // Whether this object is listening for global shortcuts.
  bool is_listening_;

  DISALLOW_COPY_AND_ASSIGN(GlobalShortcutListenerOzone);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_OZONE_H_
