// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_ARC_INPUT_ARC_INPUT_BRIDGE_H_
#define COMPONENTS_EXO_ARC_INPUT_ARC_INPUT_BRIDGE_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

class ArcBridgeService;

// The ArcInputBridge is responsible for sending input events from ARC windows
// to the ARC instance.
// It hooks into aura::Env to watch for ExoSurface windows that are running ARC
// applications. On those windows the input bridge will attach an EventPreTarget
// to capture all input events.
// To send those events to the ARC instance it will create bridge input devices
// through the ArcBridgeService, which will provide a file descriptor to which
// we can send linux input_event's.
// ui::Events to the ARC windows are translated to linux input_event's, which
// are then sent through the respective file descriptor.
class ArcInputBridge {
 public:
  virtual ~ArcInputBridge() {}

  // Creates a new instance of ArcInputBridge. It will register an Observer
  // with aura::Env and the ArcBridgeService.
  static scoped_ptr<ArcInputBridge> Create(
      ArcBridgeService* arc_bridge_service);

 protected:
  ArcInputBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcInputBridge);
};

}  // namespace arc

#endif  // COMPONENTS_EXO_ARC_INPUT_ARC_INPUT_BRIDGE_H_
