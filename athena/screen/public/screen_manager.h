// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_H_
#define ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_H_

#include <string>

#include "athena/common/athena_export.h"

namespace aura {
class Window;
}

namespace gfx {
class ImageSkia;
}

namespace athena {

// Mananges basic UI components on the screen such as background, and provide
// API for other UI components, such as window manager, home card, to
// create and manage their windows on the screen.
class ATHENA_EXPORT ScreenManager {
 public:
  // Creates, returns and deletes the singleton object of the ScreenManager
  // implementation.
  static ScreenManager* Create(aura::Window* root);
  static ScreenManager* Get();
  static void Shutdown();

  virtual ~ScreenManager() {}

  // Creates the container window that is used when creating a normal
  // widget without specific parent.
  virtual aura::Window* CreateDefaultContainer(const std::string& name) = 0;

  // Creates the container window on the screen.
  virtual aura::Window* CreateContainer(const std::string& name) = 0;

  // Return the context object to be used for widget creation.
  virtual aura::Window* GetContext() = 0;

  // Sets the background image.
  virtual void SetBackgroundImage(const gfx::ImageSkia& image) = 0;
};

}  // namespace athena

#endif  // ATHENA_SCREEN_PUBLIC_SCREEN_MANAGER_H_
