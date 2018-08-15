// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_COMPOSITOR_DELEGATE_H_
#define CHROME_BROWSER_VR_COMPOSITOR_DELEGATE_H_

#include "chrome/browser/vr/vr_export.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace vr {

class VR_EXPORT CompositorDelegate {
 public:
  typedef base::OnceCallback<void()> SkiaContextCallback;

  virtual ~CompositorDelegate() {}

  // These methods return true when succeeded.
  virtual bool Initialize(const scoped_refptr<gl::GLSurface>& surface) = 0;
  virtual bool RunInSkiaContext(SkiaContextCallback callback) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_COMPOSITOR_DELEGATE_H_
