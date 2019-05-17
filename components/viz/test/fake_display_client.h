// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_

#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/display_private.mojom.h"

namespace viz {

class FakeDisplayClient : public mojom::DisplayClient {
 public:
  FakeDisplayClient();
  ~FakeDisplayClient() override;

  mojom::DisplayClientPtr BindInterfacePtr();

  // mojom::DisplayClient implementation.
#if defined(OS_MACOSX)
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
#endif

#if defined(OS_WIN)
  void CreateLayeredWindowUpdater(
      mojom::LayeredWindowUpdaterRequest request) override;
#endif

 private:
  mojo::Binding<mojom::DisplayClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeDisplayClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_DISPLAY_CLIENT_H_
