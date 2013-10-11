// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_GTK_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_GTK_H_

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"

namespace extensions {

// Linux-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts. Handles basic keyboard intercepting and
// forwards its output to the base class for processing.
// TODO(zhaobin): Implement this class.
class GlobalShortcutListenerGtk : public GlobalShortcutListener {
 public:
  virtual ~GlobalShortcutListenerGtk();

  virtual void StartListening() OVERRIDE;
  virtual void StopListening() OVERRIDE;

 private:
  friend struct base::DefaultLazyInstanceTraits<GlobalShortcutListenerGtk>;

  GlobalShortcutListenerGtk();

  // Register an |accelerator| with the particular |observer|.
  virtual void RegisterAccelerator(
      const ui::Accelerator& accelerator,
      GlobalShortcutListener::Observer* observer) OVERRIDE;
  // Unregister an |accelerator| with the particular |observer|.
  virtual void UnregisterAccelerator(
      const ui::Accelerator& accelerator,
      GlobalShortcutListener::Observer* observer) OVERRIDE;

  // Whether this object is listening for global shortcuts.
  bool is_listening_;

  DISALLOW_COPY_AND_ASSIGN(GlobalShortcutListenerGtk);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_GTK_H_
