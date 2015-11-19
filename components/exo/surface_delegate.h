// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_DELEGATE_H_
#define COMPONENTS_EXO_SURFACE_DELEGATE_H_

namespace exo {

// Handles events on surfaces in context-specific ways.
class SurfaceDelegate {
 public:
  // Called when the surface is destroyed. The delegate can use this
  // opportunity to delete itself if necessary.
  virtual void OnSurfaceDestroying() = 0;

  // Called when surface was requested to commit all double-buffered state.
  virtual void OnSurfaceCommit() = 0;

 protected:
  virtual ~SurfaceDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_DELEGATE_H_
