// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_DELEGATE_H_

namespace surfaces {

class SurfacesImpl;

// A SurfacesDelegate is an object that tracsk the lifetime of a SurfacesImpl
// object.
class SurfacesDelegate {
 public:
  virtual void OnSurfaceConnectionClosed(SurfacesImpl* surface) = 0;

 protected:
  ~SurfacesDelegate() {}
};

}  // namespace surfaces

#endif  // COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_DELEGATE_H_
