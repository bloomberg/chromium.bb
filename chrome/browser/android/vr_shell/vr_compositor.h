// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_COMPOSITOR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_COMPOSITOR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/public/browser/android/compositor_client.h"
#include "ui/gfx/geometry/size.h"

typedef unsigned int SkColor;

namespace cc {
class Layer;
}

namespace content {
class Compositor;
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace vr_shell {

class VrCompositor : public content::CompositorClient {
 public:
  VrCompositor(ui::WindowAndroid* window, bool is_transparent);
  ~VrCompositor() override;

  void SurfaceDestroyed();
  void SetWindowBounds(gfx::Size size);
  void SurfaceChanged(jobject surface);
  void SetLayer(content::WebContents* web_contents);

 private:
  void RestoreLayer();

  std::unique_ptr<content::Compositor> compositor_;

  cc::Layer* layer_ = nullptr;
  cc::Layer* layer_parent_ = nullptr;
  SkColor background_color_;
  bool transparent_;

  DISALLOW_COPY_AND_ASSIGN(VrCompositor);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_COMPOSITOR_H_
