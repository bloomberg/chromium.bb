// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEBRUNNER_SCREEN_H_
#define FUCHSIA_ENGINE_BROWSER_WEBRUNNER_SCREEN_H_

#include "base/macros.h"

#include "ui/display/screen_base.h"

// display::Screen implementation for WebRunner on Fuchsia.
class DISPLAY_EXPORT WebRunnerScreen : public display::ScreenBase {
 public:
  WebRunnerScreen();
  ~WebRunnerScreen() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRunnerScreen);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEBRUNNER_SCREEN_H_
