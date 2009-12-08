// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/notifications/notification.h"

class Balloon;
class BalloonCollection;
class Profile;
class SiteInstance;

// Interface for a view that displays a balloon.
class BalloonView {
 public:
  virtual ~BalloonView() { }

  // Show the view on the screen.
  virtual void Show(Balloon* balloon) = 0;

  // Reposition the view to match the position of its balloon.
  virtual void RepositionToBalloon() = 0;

  // Close the view.
  virtual void Close(bool by_user) = 0;

  // The total size of the view.
  virtual gfx::Size GetSize() const = 0;
};

// Represents a Notification on the screen.
class Balloon {
 public:
  Balloon(const Notification& notification,
          Profile* profile,
          BalloonCollection* collection);
  virtual ~Balloon();

  const Notification& notification() const { return notification_; }
  Profile* profile() const { return profile_; }

  const gfx::Point& position() const { return position_; }
  void SetPosition(const gfx::Point& upper_left, bool reposition);

  const gfx::Size& content_size() const { return content_size_; }
  void set_content_size(const gfx::Size& size) { content_size_ = size; }

  // Request a new content size for this balloon.  This will get passed
  // to the balloon collection for checking against available space and
  // min/max restrictions.
  void SetContentPreferredSize(const gfx::Size& size);

  // Provides a view for this balloon.  Ownership transfers
  // to this object.
  void set_view(BalloonView* balloon_view);

  // Returns the viewing size for the balloon (content + frame).
  gfx::Size GetViewSize() const { return balloon_view_->GetSize(); }

  // Shows the balloon.
  virtual void Show();

  // Called when the balloon is closed, either by user (through the UI)
  // or by a script.
  virtual void OnClose(bool by_user);

  // Called by script to cause the balloon to close.
  virtual void CloseByScript();

 private:
  // Non-owned pointer to the profile.
  Profile* profile_;

  // The notification being shown in this balloon.
  Notification notification_;

  // The collection that this balloon belongs to.  Non-owned pointer.
  BalloonCollection* collection_;

  // The actual UI element for the balloon.
  scoped_ptr<BalloonView> balloon_view_;

  // Position and size of the balloon on the screen.
  gfx::Point position_;
  gfx::Size content_size_;

  DISALLOW_COPY_AND_ASSIGN(Balloon);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_H_
