// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_VIEW_SURFACE_CLIENT_H_
#define COMPONENTS_MUS_PUBLIC_CPP_VIEW_SURFACE_CLIENT_H_

namespace mus {

class ViewSurface;

class ViewSurfaceClient {
 public:
  virtual void OnResourcesReturned(
      ViewSurface* surface,
      mojo::Array<mojo::ReturnedResourcePtr> resources) = 0;

 protected:
  ~ViewSurfaceClient() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_VIEW_SURFACE_CLIENT_H_
