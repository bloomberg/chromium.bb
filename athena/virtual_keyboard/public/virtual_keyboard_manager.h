// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_VIRTUAL_KEYBOARD_PUBLIC_VIRTUAL_KEYBOARD_MANAGER_H_
#define ATHENA_VIRTUAL_KEYBOARD_PUBLIC_VIRTUAL_KEYBOARD_MANAGER_H_

#include "athena/athena_export.h"

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace athena {

class ATHENA_EXPORT VirtualKeyboardManager {
 public:
  virtual ~VirtualKeyboardManager() {}

  static VirtualKeyboardManager* Create(content::BrowserContext* context);
  static VirtualKeyboardManager* Get();
  static void Shutdown();
};

}  // namespace athena

#endif  // ATHENA_VIRTUAL_KEYBOARD_PUBLIC_VIRTUAL_KEYBOARD_MANAGER_H_
