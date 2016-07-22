// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_instance.h"

#include <utility>

#include "base/run_loop.h"

namespace arc {

FakeArcBridgeInstance::FakeArcBridgeInstance() : binding_(this) {}
FakeArcBridgeInstance::~FakeArcBridgeInstance() {}

void FakeArcBridgeInstance::Init(mojom::ArcBridgeHostPtr host) {
  host_ptr_ = std::move(host);
  init_calls_++;

  // Wake WaitForInitCall().
  if (!quit_closure_.is_null())
    quit_closure_.Run();
  quit_closure_.Reset();
}

void FakeArcBridgeInstance::Unbind() {
  host_ptr_.reset();
  if (binding_.is_bound())
    binding_.Close();
}

void FakeArcBridgeInstance::Bind(
    mojo::InterfaceRequest<mojom::ArcBridgeInstance> interface_request) {
  binding_.Bind(std::move(interface_request));
}

void FakeArcBridgeInstance::WaitForInitCall() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  binding_.set_connection_error_handler(run_loop.QuitClosure());
  run_loop.Run();
}

void FakeArcBridgeInstance::Stop(ArcBridgeService::StopReason reason) {
  if (!delegate_)
    return;
  delegate_->OnStopped(reason);
}

}  // namespace arc
