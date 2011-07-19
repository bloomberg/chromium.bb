// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/balloon_view_bridge.h"

#include "chrome/browser/ui/cocoa/notifications/balloon_controller.h"
#import "chrome/browser/ui/cocoa/notifications/balloon_view_host_mac.h"
#include "ui/gfx/size.h"

#import <Cocoa/Cocoa.h>

BalloonViewBridge::BalloonViewBridge() :
    controller_(NULL) {
}

BalloonViewBridge::~BalloonViewBridge() {
}

void BalloonViewBridge::Close(bool by_user) {
  [controller_ closeBalloon:by_user];
}

gfx::Size BalloonViewBridge::GetSize() const {
  if (controller_)
    return gfx::Size([controller_ desiredTotalWidth],
                     [controller_ desiredTotalHeight]);
  else
    return gfx::Size();
}

void BalloonViewBridge::RepositionToBalloon() {
  [controller_ repositionToBalloon];
}

void BalloonViewBridge::Show(Balloon* balloon) {
  controller_ = [[BalloonController alloc] initWithBalloon:balloon];
  [controller_ setShouldCascadeWindows:NO];
  [controller_ showWindow:nil];
}

BalloonHost* BalloonViewBridge::GetHost() const {
  return [controller_ getHost];
}

void BalloonViewBridge::Update() {
  [controller_ updateContents];
}
