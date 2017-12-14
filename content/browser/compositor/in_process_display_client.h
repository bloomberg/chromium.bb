// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IN_PROCESS_DISPLAY_CLIENT_H_
#define CONTENT_BROWSER_COMPOSITOR_IN_PROCESS_DISPLAY_CLIENT_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/display_private.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// A DisplayClient that can be used to display received
// gfx::CALayerParams in a CALayer tree in this process.
class InProcessDisplayClient : public viz::mojom::DisplayClient {
 public:
  InProcessDisplayClient(gfx::AcceleratedWidget widget);
  ~InProcessDisplayClient() override;

  viz::mojom::DisplayClientPtr GetBoundPtr();

 private:
  // viz::mojom::DisplayClient implementation:
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;

  mojo::Binding<viz::mojom::DisplayClient> binding_;
#if defined(OS_MACOSX)
  gfx::AcceleratedWidget widget_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_IN_PROCESS_DISPLAY_CLIENT_H_
