// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
#pragma once

#include <deque>
#include <string>

#include "base/callback.h"
#include "base/scoped_ptr.h"

class Balloon;
class GURL;
class Notification;
class Profile;

namespace gfx {
class Size;
}  // namespace gfx

class BalloonCollection {
 public:
  class BalloonSpaceChangeListener {
   public:
    virtual ~BalloonSpaceChangeListener() {}

    // Called when there is more or less space for balloons due to
    // monitor size changes or balloons disappearing.
    virtual void OnBalloonSpaceChanged() = 0;
  };

  // Do not change existing values without migration path; these
  // are stored as integers in user preferences.
  enum PositionPreference {
    UPPER_RIGHT        = 0,
    LOWER_RIGHT        = 1,
    UPPER_LEFT         = 2,
    LOWER_LEFT         = 3,

    // The default position is different on different platforms.
    DEFAULT_POSITION   = -1
  };

  static BalloonCollection* Create();

  BalloonCollection()
      : space_change_listener_(NULL) {
  }

  virtual ~BalloonCollection() {}

  // Adds a new balloon for the specified notification.
  virtual void Add(const Notification& notification,
                   Profile* profile) = 0;

  // Removes any balloons that have this notification id.  Returns
  // true if anything was removed.
  virtual bool RemoveById(const std::string& id) = 0;

  // Removes any balloons that have this source origin.  Returns
  // true if anything was removed.
  virtual bool RemoveBySourceOrigin(const GURL& source_origin) = 0;

  // Removes all balloons.
  virtual void RemoveAll() = 0;

  // Is there room to add another notification?
  virtual bool HasSpace() const = 0;

  // Request the resizing of a balloon.
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size) = 0;

  // Set the position preference for the collection.
  virtual void SetPositionPreference(PositionPreference position) = 0;

  // Update for new screen dimensions.
  virtual void DisplayChanged() = 0;

  // Inform the collection that a balloon was closed.
  virtual void OnBalloonClosed(Balloon* source) = 0;

  // Get const collection of the active balloons.
  typedef std::deque<Balloon*> Balloons;
  virtual const Balloons& GetActiveBalloons() = 0;

  BalloonSpaceChangeListener* space_change_listener() {
    return space_change_listener_;
  }
  void set_space_change_listener(BalloonSpaceChangeListener* listener) {
    space_change_listener_ = listener;
  }

  void set_on_collection_changed_callback(Callback0::Type* callback) {
    on_collection_changed_callback_.reset(callback);
  }

 protected:
  // Non-owned pointer to an object listening for space changes.
  BalloonSpaceChangeListener* space_change_listener_;

  // For use only with testing. This callback is invoked when a balloon
  // is added or removed from the collection.
  scoped_ptr<Callback0::Type> on_collection_changed_callback_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
