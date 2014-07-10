// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_INPUT_PUBLIC_INPUT_MANAGER_H_
#define ATHENA_INPUT_PUBLIC_INPUT_MANAGER_H_

#include "athena/athena_export.h"

namespace aura {
class Window;
}

namespace ui {
class EventTarget;
}

namespace athena {
class AcceleratorManager;

class ATHENA_EXPORT InputManager {
 public:
  // Creates and deletes the singleton object of the InputManager
  // implementation.
  static InputManager* Create();
  static InputManager* Get();
  static void Shutdown();

  // TODO(oshima): Fix the initialization process and replace this
  // with EnvObserver::WindowInitialized
  virtual void OnRootWindowCreated(aura::Window* root_window) = 0;

  virtual ui::EventTarget* GetTopmostEventTarget() = 0;
  virtual AcceleratorManager* GetAcceleratorManager() = 0;

  virtual ~InputManager() {}
};

}  // namespace athena

#endif  // ATHENA_INPUT_PUBLIC_INPUT_MANAGER_H_
