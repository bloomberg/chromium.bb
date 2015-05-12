// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_UI_SETUP_H_
#define COMPONENTS_HTML_VIEWER_UI_SETUP_H_

#include "base/basictypes.h"

namespace gfx {
class Size;
}

namespace html_viewer {

// UISetup is intended for platform specific UI setup.
class UISetup {
 public:
  UISetup(const gfx::Size& screen_size_in_pixels, float device_pixel_ratio) {}
  ~UISetup() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UISetup);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_UI_SETUP_H_
