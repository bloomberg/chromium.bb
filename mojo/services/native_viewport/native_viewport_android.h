// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "ui/gfx/size.h"

namespace gpu {
class GLInProcessContext;
}

struct ANativeWindow;

namespace mojo {
namespace services {

class NativeViewportAndroid : public NativeViewport {
 public:
  explicit NativeViewportAndroid(NativeViewportDelegate* delegate);
  virtual ~NativeViewportAndroid();

  base::WeakPtr<NativeViewportAndroid> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnNativeWindowCreated(ANativeWindow* window);
  void OnNativeWindowDestroyed();
  void OnResized(const gfx::Size& size);

 private:
  // Overridden from NativeViewport:
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void Close() OVERRIDE;

  void ReleaseWindow();

  NativeViewportDelegate* delegate_;
  ANativeWindow* window_;
  gfx::Size size_;

  base::WeakPtrFactory<NativeViewportAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportAndroid);
};


}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_
