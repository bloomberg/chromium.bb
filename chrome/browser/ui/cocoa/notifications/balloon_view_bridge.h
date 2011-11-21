// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_BALLOON_VIEW_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_BALLOON_VIEW_BRIDGE_H_
#pragma once

#include "chrome/browser/notifications/balloon.h"

@class BalloonController;
class BalloonHost;
namespace gfx {
class Size;
}

// Bridges from the cross-platform BalloonView interface to the Cocoa
// controller which will draw the view on screen.
class BalloonViewBridge : public BalloonView {
 public:
  BalloonViewBridge();
  virtual ~BalloonViewBridge();

  // BalloonView interface.
  virtual void Show(Balloon* balloon) OVERRIDE;
  virtual void Update() OVERRIDE;
  virtual void RepositionToBalloon() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual BalloonHost* GetHost() const OVERRIDE;

 private:
  // Weak pointer to the balloon controller which manages the UI.
  // This object cleans itself up when its windows close.
  BalloonController* controller_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_BALLOON_VIEW_BRIDGE_H_
