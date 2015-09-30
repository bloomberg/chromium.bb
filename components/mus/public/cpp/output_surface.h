// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_OUTPUT_SURFACE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/public/cpp/view_surface.h"
#include "components/mus/public/cpp/view_surface_client.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mus {

class OutputSurface : public cc::OutputSurface, public ViewSurfaceClient {
 public:
  OutputSurface(const scoped_refptr<cc::ContextProvider>& context_provider,
                scoped_ptr<ViewSurface> surface);
  ~OutputSurface() override;

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void DetachFromClient() override;

 private:
  // ViewSurfaceClient implementation:
  void OnResourcesReturned(
      ViewSurface* surface,
      mojo::Array<mojo::ReturnedResourcePtr> resources) override;

  void SwapBuffersComplete();

  scoped_ptr<ViewSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_OUTPUT_SURFACE_H_
