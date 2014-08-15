// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport.h"

// Stub to build on platforms we don't fully support yet.

namespace mojo {

class PlatformViewportStub : public PlatformViewport {
 public:
  PlatformViewportStub(Delegate* delegate) : delegate_(delegate) {
  }
  virtual ~PlatformViewportStub() {
  }

 private:
  // Overridden from PlatformViewport:
  virtual void Init() OVERRIDE {
  }
  virtual void Show() OVERRIDE {
  }
  virtual void Hide() OVERRIDE {
  }
  virtual void Close() OVERRIDE {
    delegate_->OnDestroyed();
  }
  virtual gfx::Size GetSize() OVERRIDE {
    return gfx::Size();
  }
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {
  }

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportStub);
};

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(
      new PlatformViewportStub(delegate)).Pass();
}

}  // namespace mojo
