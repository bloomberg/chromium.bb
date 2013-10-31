// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

namespace mojo {
namespace services {

class NativeViewportX11 : public NativeViewport {
 public:
  NativeViewportX11(NativeViewportDelegate* delegate)
      : delegate_(delegate) {
  }
  virtual ~NativeViewportX11() {
  }

 private:
  // Overridden from NativeViewport:
  virtual void Close() OVERRIDE {
    // TODO(beng): perform this in response to XWindow destruction.
    delegate_->OnDestroyed();
  }

  NativeViewportDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportX11);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportX11(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
