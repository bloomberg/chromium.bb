// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class Balloon;
class BalloonCollection;
class BalloonHost;
class Notification;
class Profile;
class SiteInstance;

// Interface for a view that displays a balloon.
class BalloonView {
 public:
  virtual ~BalloonView() { }

  // Show the view on the screen.
  virtual void Show(Balloon* balloon) = 0;

  // Notify that the content of notification has chagned.
  virtual void Update() = 0;

  // Reposition the view to match the position of its balloon.
  virtual void RepositionToBalloon() = 0;

  // Close the view.
  virtual void Close(bool by_user) = 0;

  // The total size of the view.
  virtual gfx::Size GetSize() const = 0;

  // The host for the view's contents.
  virtual BalloonHost* GetHost() const = 0;
};

// Represents a Notification on the screen.
class Balloon {
 public:
  Balloon(const Notification& notification,
          Profile* profile,
          BalloonCollection* collection);
  virtual ~Balloon();

  const Notification& notification() const { return *notification_.get(); }
  Profile* profile() const { return profile_; }

  gfx::Point GetPosition() const {
    return position_.Add(offset_);
  }
  void SetPosition(const gfx::Point& upper_left, bool reposition);

  const gfx::Point& offset() { return offset_;}
  void set_offset(const gfx::Point& offset) { offset_ = offset; }
  void add_offset(const gfx::Point& offset) { offset_ = offset_.Add(offset); }

  const gfx::Size& content_size() const { return content_size_; }
  void set_content_size(const gfx::Size& size) { content_size_ = size; }

  const BalloonCollection* collection() const { return collection_; }

  const gfx::Size& min_scrollbar_size() const { return min_scrollbar_size_; }
  void set_min_scrollbar_size(const gfx::Size& size) {
    min_scrollbar_size_ = size;
  }

  // Request a new content size for this balloon.  This will get passed
  // to the balloon collection for checking against available space and
  // min/max restrictions.
  void SetContentPreferredSize(const gfx::Size& size);

  // Provides a view for this balloon.  Ownership transfers
  // to this object.
  void set_view(BalloonView* balloon_view);

  // Returns the balloon view associated with the balloon.
  BalloonView* view() const {
    return balloon_view_.get();
  }

  // Returns the viewing size for the balloon (content + frame).
  gfx::Size GetViewSize() const { return balloon_view_->GetSize(); }

  // Shows the balloon.
  virtual void Show();

  // Notify that the content of notification has changed.
  virtual void Update(const Notification& notification);

  // Called when the balloon is clicked by the user.
  virtual void OnClick();

  // Called when the balloon is closed, either by user (through the UI)
  // or by a script.
  virtual void OnClose(bool by_user);

  // Called by script to cause the balloon to close.
  virtual void CloseByScript();

 private:
  // Non-owned pointer to the profile.
  Profile* profile_;

  // The notification being shown in this balloon.
  scoped_ptr<Notification> notification_;

  // The collection that this balloon belongs to.  Non-owned pointer.
  BalloonCollection* collection_;

  // The actual UI element for the balloon.
  scoped_ptr<BalloonView> balloon_view_;

  // Position and size of the balloon on the screen.
  gfx::Point position_;
  gfx::Size content_size_;

  // Temporary offset for balloons that need to be positioned in a non-standard
  // position for keeping the close buttons under the mouse cursor.
  gfx::Point offset_;

  // Smallest size for this balloon where scrollbars will be shown.
  gfx::Size min_scrollbar_size_;

  DISALLOW_COPY_AND_ASSIGN(Balloon);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_H_
