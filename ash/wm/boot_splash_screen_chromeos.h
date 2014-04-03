// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BOOT_SPLASH_SCREEN_CHROMEOS_H_
#define ASH_WM_BOOT_SPLASH_SCREEN_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace aura {
class WindowTreeHost;
}

namespace ui {
class Layer;
class LayerDelegate;
}

namespace ash {

// BootSplashScreen manages a ui::Layer, stacked at the top of the root layer's
// children, that displays a copy of the initial contents of the host window.
// This allows us to continue displaying the Chrome OS boot splash screen (which
// is displayed before Ash starts) after the compositor has taken over so we can
// animate the transition between the splash screen and the login screen.
class BootSplashScreen {
 public:
  explicit BootSplashScreen(aura::WindowTreeHost* host);
  ~BootSplashScreen();

  // Begins animating |layer_|'s opacity to 0 over |duration|.
  void StartHideAnimation(base::TimeDelta duration);

 private:
  class CopyHostContentLayerDelegate;

  // Copies the host window's content to |layer_|.
  scoped_ptr<CopyHostContentLayerDelegate> layer_delegate_;

  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(BootSplashScreen);
};

}  // namespace ash

#endif  // ASH_WM_BOOT_SPLASH_SCREEN_CHROMEOS_H_
