// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_COMPOSITOR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_COMPOSITOR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/public/browser/android/compositor_client.h"

namespace cc {
class Layer;
}

namespace content {
class Compositor;
class ContentViewCore;
}

namespace ui {
class WindowAndroid;
}

namespace vr_shell {

class VrCompositor : public content::CompositorClient {
 public:
  explicit VrCompositor(ui::WindowAndroid* window);
  ~VrCompositor() override;

  void SurfaceDestroyed();
  void SurfaceChanged(int width,
                      int height,
                      const base::android::JavaParamRef<jobject>& surface);
  void SetLayer(content::ContentViewCore* core);

  // CompositorClient implementation:
  void UpdateLayerTreeHost() override;
  void OnSwapBuffersCompleted(int pending_swap_buffers) override;

 private:
  std::unique_ptr<content::Compositor> compositor_;

  cc::Layer* layer_ = nullptr;
  cc::Layer* layer_parent_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VrCompositor);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_COMPOSITOR_H_
