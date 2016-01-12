// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ARC_VIDEO_BRIDGE_H
#define COMPONENTS_ARC_VIDEO_ARC_VIDEO_BRIDGE_H

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/video/video_host_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class VideoHostDelegate;

// ArcVideoBridge bridges ArcBridgeService and VideoHostDelegate. It observes
// ArcBridgeService events and pass VideoHost proxy to VideoInstance.
class ArcVideoBridge : public ArcBridgeService::Observer {
 public:
  explicit ArcVideoBridge(scoped_ptr<VideoHostDelegate> video_host_delegate);
  ~ArcVideoBridge() override;

  // Starts listening to state changes of the ArcBridgeService.
  void StartObservingBridgeServiceChanges();

  // arc::ArcBridgeService::Observer implementation.
  void OnStateChanged(arc::ArcBridgeService::State state) override;
  void OnVideoInstanceReady() override;

 private:
  scoped_ptr<VideoHostDelegate> video_host_delegate_;
  mojo::Binding<arc::VideoHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcVideoBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ARC_VIDEO_BRIDGE_H
