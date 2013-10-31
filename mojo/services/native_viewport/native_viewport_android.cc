// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

namespace mojo {
namespace services {

class NativeViewportAndroid : public NativeViewport {
 public:
  NativeViewportAndroid(NativeViewportDelegate* delegate)
      : delegate_(delegate) {
  }
  virtual ~NativeViewportAndroid() {
  }

 private:
  // Overridden from NativeViewport:
  virtual void Close() OVERRIDE {
    // TODO(beng): close activity containing MojoView?

    // TODO(beng): perform this in response to view destruction.
    delegate_->OnDestroyed();
  }

  NativeViewportDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportAndroid);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportAndroid(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
