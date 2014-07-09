// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_CHROMEOS_H_

#include "chrome/browser/extensions/global_shortcut_listener.h"

#include "ui/base/accelerators/accelerator.h"

namespace extensions {

// ChromeOS-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts. Handles basic keyboard intercepting and
// forwards its output to the base class for processing.
class GlobalShortcutListenerChromeOS : public GlobalShortcutListener,
                                       ui::AcceleratorTarget {
 public:
  GlobalShortcutListenerChromeOS();
  virtual ~GlobalShortcutListenerChromeOS();

 private:
  // GlobalShortcutListener implementation.
  virtual void StartListening() OVERRIDE;
  virtual void StopListening() OVERRIDE;
  virtual bool RegisterAcceleratorImpl(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual void UnregisterAcceleratorImpl(
      const ui::Accelerator& accelerator) OVERRIDE;

  // ui::AcceleratorTarget implementation.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  // Whether this object is listening for global shortcuts.
  bool is_listening_;

  DISALLOW_COPY_AND_ASSIGN(GlobalShortcutListenerChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_CHROMEOS_H_
