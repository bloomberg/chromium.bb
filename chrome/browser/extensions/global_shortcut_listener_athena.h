// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_ATHENA_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_ATHENA_H_

#include "chrome/browser/extensions/global_shortcut_listener.h"

namespace extensions {

// Athena implementation of GlobalShortcutListener.
class GlobalShortcutListenerAthena : public GlobalShortcutListener {
 public:
  GlobalShortcutListenerAthena();
  ~GlobalShortcutListenerAthena() override;

 private:
  // GlobalShortcutListener:
  void StartListening() override;
  void StopListening() override;
  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override;

  DISALLOW_COPY_AND_ASSIGN(GlobalShortcutListenerAthena);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_ATHENA_H_
