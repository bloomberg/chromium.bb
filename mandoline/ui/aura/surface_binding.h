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
class View;
}

namespace mandoline {

// SurfaceBinding is responsible for managing the connections necessary to
// bind a View to the surfaces service.
// Internally SurfaceBinding manages one connection (and related structures) per
// ViewManager. That is, all Views from a particular ViewManager share the same
// connection.
class SurfaceBinding {
 public:
  SurfaceBinding(mojo::Shell* shell, mojo::View* view);
  ~SurfaceBinding();

  // Creates an OutputSurface that renders to the View supplied to the
  // constructor.
  scoped_ptr<cc::OutputSurface> CreateOutputSurface();

 private:
  class PerViewManagerState;

  mojo::Shell* shell_;
  mojo::View* view_;
  scoped_refptr<PerViewManagerState> state_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceBinding);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_SURFACE_BINDING_H_
