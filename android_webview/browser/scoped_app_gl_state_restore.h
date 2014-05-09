// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class GLContext;
}

namespace android_webview {

namespace internal {
class ScopedAppGLStateRestoreImpl;
}

// This class is not thread safe and should only be used on the UI thread.
class ScopedAppGLStateRestore {
 public:
  enum CallMode {
    MODE_DRAW,
    MODE_RESOURCE_MANAGEMENT,
  };

  ScopedAppGLStateRestore(CallMode mode);
  ~ScopedAppGLStateRestore();

  bool stencil_enabled() const;
  int framebuffer_binding_ext() const;

 private:
  scoped_ptr<internal::ScopedAppGLStateRestoreImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppGLStateRestore);
};

}  // namespace android_webview
