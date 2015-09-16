// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_SURFACE_BINDING_H_
#define MANDOLINE_UI_AURA_SURFACE_BINDING_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace cc {
class OutputSurface;
}

namespace mojo {
class Shell;
}

namespace mus {
class View;
}

namespace mandoline {

// SurfaceBinding is responsible for managing the connections necessary to
// bind a View to the surfaces service.
// Internally SurfaceBinding manages one connection (and related structures) per
// ViewTreeConnection. That is, all Views from a particular ViewTreeConnection
// share the same connection.
class SurfaceBinding {
 public:
  SurfaceBinding(mojo::Shell* shell, mus::View* view);
  ~SurfaceBinding();

  // Creates an OutputSurface that renders to the View supplied to the
  // constructor.
  scoped_ptr<cc::OutputSurface> CreateOutputSurface();

 private:
  class PerConnectionState;

  mojo::Shell* shell_;
  mus::View* view_;
  scoped_refptr<PerConnectionState> state_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceBinding);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_SURFACE_BINDING_H_
