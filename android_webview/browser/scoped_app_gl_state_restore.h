// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/macros.h"

namespace android_webview {

namespace internal {
class ScopedAppGLStateRestoreImpl;
}

struct StencilState {
  unsigned char stencil_test_enabled;
  int stencil_front_func;
  int stencil_front_ref;
  int stencil_front_mask;
  int stencil_back_func;
  int stencil_back_ref;
  int stencil_back_mask;
  int stencil_clear;
  int stencil_front_writemask;
  int stencil_back_writemask;
  int stencil_front_fail_op;
  int stencil_front_z_fail_op;
  int stencil_front_z_pass_op;
  int stencil_back_fail_op;
  int stencil_back_z_fail_op;
  int stencil_back_z_pass_op;
};

// This class is not thread safe and should only be used on the UI thread.
class ScopedAppGLStateRestore {
 public:
  enum CallMode {
    MODE_DRAW,
    MODE_RESOURCE_MANAGEMENT,
  };

  static ScopedAppGLStateRestore* Current();

  explicit ScopedAppGLStateRestore(CallMode mode);
  ~ScopedAppGLStateRestore();

  StencilState stencil_state() const;
  int framebuffer_binding_ext() const;

 private:
  std::unique_ptr<internal::ScopedAppGLStateRestoreImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppGLStateRestore);
};

}  // namespace android_webview
