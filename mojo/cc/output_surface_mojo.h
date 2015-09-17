// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CC_OUTPUT_SURFACE_MOJO_H_
#define MOJO_CC_OUTPUT_SURFACE_MOJO_H_

#include "base/macros.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/public/cpp/view_surface.h"
#include "components/mus/public/cpp/view_surface_client.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {

class OutputSurfaceMojo : public cc::OutputSurface,
                          public mojo::ViewSurfaceClient {
 public:
  OutputSurfaceMojo(const scoped_refptr<cc::ContextProvider>& context_provider,
                    scoped_ptr<mojo::ViewSurface> surface);
  ~OutputSurfaceMojo() override;

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

 private:
  // uip::SurfaceObserver implementation:
  void OnResourcesReturned(
      mojo::ViewSurface* surface,
      mojo::Array<mojo::ReturnedResourcePtr> resources) override;

  scoped_ptr<mojo::ViewSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceMojo);
};
}

#endif  // MOJO_CC_OUTPUT_SURFACE_MOJO_H_
