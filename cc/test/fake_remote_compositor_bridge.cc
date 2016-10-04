// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_remote_compositor_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cc/blimp/remote_compositor_bridge_client.h"

namespace cc {

FakeRemoteCompositorBridge::FakeRemoteCompositorBridge(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner)
    : RemoteCompositorBridge(std::move(compositor_main_task_runner)),
      client_(nullptr),
      weak_factory_(this) {}

FakeRemoteCompositorBridge::~FakeRemoteCompositorBridge() {}

void FakeRemoteCompositorBridge::BindToClient(
    RemoteCompositorBridgeClient* client) {
  DCHECK(!client_);
  client_ = client;
}

void FakeRemoteCompositorBridge::ScheduleMainFrame() {
  compositor_main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FakeRemoteCompositorBridge::BeginMainFrame,
                            weak_factory_.GetWeakPtr()));
}

void FakeRemoteCompositorBridge::BeginMainFrame() {
  client_->BeginMainFrame();
}

}  // namespace cc
