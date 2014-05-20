// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_BACKGROUND_CONTROLLER_H_
#define ATHENA_SCREEN_BACKGROUND_CONTROLLER_H_

#include "base/macros.h"

namespace aura {
class Window;
}

namespace gfx {
class ImageSkia;
}

namespace athena {
class BackgroundView;

// Controls background image switching.
class BackgroundController {
 public:
  explicit BackgroundController(aura::Window* container);
  ~BackgroundController();

  void SetImage(const gfx::ImageSkia& image);

 private:
  BackgroundView* background_view_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundController);
};
}

#endif  // ATHENA_SCREEN_BACKGROUND_CONTROLLER_H_
