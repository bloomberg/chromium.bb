// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_FAKE_BALLOON_VIEW_H_
#define CHROME_BROWSER_NOTIFICATIONS_FAKE_BALLOON_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/notifications/balloon.h"

// Test version of a balloon view which doesn't do anything viewable, but does
// know how to close itself the same as a regular BalloonView.
class FakeBalloonView : public BalloonView {
 public:
  explicit FakeBalloonView(Balloon* balloon);
  virtual ~FakeBalloonView();

 private:
  // Overridden from BalloonView:
  virtual void Show(Balloon* balloon) OVERRIDE;
  virtual void Update() OVERRIDE;
  virtual void RepositionToBalloon() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual BalloonHost* GetHost() const OVERRIDE;

  // Non-owned pointer.
  Balloon* balloon_;

  DISALLOW_COPY_AND_ASSIGN(FakeBalloonView);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_FAKE_BALLOON_VIEW_H_
