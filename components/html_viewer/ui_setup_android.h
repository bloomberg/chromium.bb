// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_UI_SETUP_ANDROID_H_
#define COMPONENTS_HTML_VIEWER_UI_SETUP_ANDROID_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Screen;
class Size;
}

namespace ui {
class GestureConfiguration;
}

namespace html_viewer {

class GestureConfigurationMandoline;

class UISetup {
 public:
  UISetup(const gfx::Size& screen_size_in_pixels, float device_pixel_ratio);
  ~UISetup();

 private:
  scoped_ptr<gfx::Screen> screen_;
  scoped_ptr<GestureConfigurationMandoline> gesture_configuration_;

  DISALLOW_COPY_AND_ASSIGN(UISetup);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_UI_SETUP_ANDROID_H_
