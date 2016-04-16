// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_bootstrap.h"

#include <utility>

#include "base/logging.h"
#include "components/arc/common/arc_bridge.mojom.h"
#include "components/arc/test/fake_arc_bridge_instance.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace arc {

FakeArcBridgeBootstrap::FakeArcBridgeBootstrap(FakeArcBridgeInstance* instance)
    : instance_(instance) {
}

void FakeArcBridgeBootstrap::Start() {
  DCHECK(delegate_);
  mojom::ArcBridgeInstancePtr instance;
  instance_->Bind(mojo::GetProxy(&instance));
  delegate_->OnConnectionEstablished(std::move(instance));
}

void FakeArcBridgeBootstrap::Stop() {
  DCHECK(delegate_);
  instance_->Unbind();
  delegate_->OnStopped();
}

}  // namespace arc
