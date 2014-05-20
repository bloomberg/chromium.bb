// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_H_
#define ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_H_

#include "athena/athena_export.h"

namespace aura {
class Window;
}

namespace gfx {
class ImageSkia;
}

namespace athena {

// Mananges screen and UI components on the screen such as background,
// home card, etc.
class ATHENA_EXPORT ScreenManager {
 public:
  // Creates, returns and deletes the singleton object of the ScreenManager
  // implementation.
  static ScreenManager* Create(aura::Window* root);
  static ScreenManager* Get();
  static void Shutdown();

  virtual ~ScreenManager() {}

  // Returns the container window for window on the screen.
  virtual aura::Window* GetContainerWindow() = 0;

  // Return the context object to be used for widget creation.
  virtual aura::Window* GetContext() = 0;

  // Sets the background image.
  virtual void SetBackgroundImage(const gfx::ImageSkia& image) = 0;
};

}  // namespace athena

#endif  // ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_H_
