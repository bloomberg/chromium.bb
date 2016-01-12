// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video/arc_video_bridge.h"

#include <utility>

namespace arc {

ArcVideoBridge::ArcVideoBridge(
    scoped_ptr<VideoHostDelegate> video_host_delegate)
    : video_host_delegate_(std::move(video_host_delegate)),
      binding_(video_host_delegate_.get()) {}

ArcVideoBridge::~ArcVideoBridge() {
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  DCHECK(bridge_service);
  bridge_service->RemoveObserver(this);
}

void ArcVideoBridge::StartObservingBridgeServiceChanges() {
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  DCHECK(bridge_service);
  bridge_service->AddObserver(this);

  // If VideoInstance was ready before we AddObserver(), we won't get
  // OnVideoInstanceReady events. For such case, we have to call it explicitly.
  if (bridge_service->video_instance())
    OnVideoInstanceReady();
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
  arc::ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  DCHECK(bridge_service);

  arc::VideoHostPtr host;
  binding_.Bind(mojo::GetProxy(&host));
  bridge_service->video_instance()->Init(std::move(host));
}

}  // namespace arc
