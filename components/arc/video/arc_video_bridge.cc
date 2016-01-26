// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video/arc_video_bridge.h"

#include <utility>

namespace arc {

ArcVideoBridge::ArcVideoBridge(
    ArcBridgeService* bridge_service,
    scoped_ptr<VideoHostDelegate> video_host_delegate)
    : ArcService(bridge_service),
      video_host_delegate_(std::move(video_host_delegate)),
      binding_(video_host_delegate_.get()) {
  arc_bridge_service()->AddObserver(this);
}

ArcVideoBridge::~ArcVideoBridge() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcVideoBridge::OnStateChanged(arc::ArcBridgeService::State state) {
  switch (state) {
    case arc::ArcBridgeService::State::STOPPING:
      video_host_delegate_->OnStopping();
      break;
    default:
      break;
  }
}

void ArcVideoBridge::OnVideoInstanceReady() {
  arc_bridge_service()->video_instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

}  // namespace arc
