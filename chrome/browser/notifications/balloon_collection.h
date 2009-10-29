// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_

#include <deque>

#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"

class Balloon;
class Profile;

class BalloonCollection {
 public:
  virtual ~BalloonCollection() {}

  // Adds a new balloon for the specified notification.
  virtual void Add(const Notification& notification,
                   Profile* profile) = 0;

  // Is there room to add another notification?
  virtual bool HasSpace() const = 0;
};

// A balloon collection represents a set of notification balloons being
// shown on the screen.  It positions new notifications according to
// a layout, and monitors for balloons being closed, which it reports
// up to its parent, the notification UI manager.
class BalloonCollectionImpl : public BalloonCollection,
                              public Balloon::BalloonCloseListener {
 public:
  class BalloonSpaceChangeListener {
   public:
    virtual ~BalloonSpaceChangeListener() {}

    // Called when there is more or less space for balloons due to
    // monitor size changes or balloons disappearing.
    virtual void OnBalloonSpaceChanged() = 0;
  };

  BalloonCollectionImpl();
  virtual ~BalloonCollectionImpl();

  BalloonSpaceChangeListener* space_change_listener() {
    return space_change_listener_;
  }
  void set_space_change_listener(BalloonSpaceChangeListener* listener) {
    space_change_listener_ = listener;
  }

  // BalloonCollectionInterface overrides
  virtual void Add(const Notification& notification,
                   Profile* profile);
  virtual bool HasSpace() const;

  // Balloon::BalloonCloseListener interface
  virtual void OnBalloonClosed(Balloon* source);

 protected:
  // Creates a new balloon. Overridable by unit tests.  The caller is
  // responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);

 private:
  // The number of balloons being displayed.
  int count() const { return balloons_.size(); }

  // Adjusts the positions of the balloons (e.g., when one is closed).
  void PositionBalloons(bool is_reposition);

  // Calculates layout values for the balloons including
  // the scaling, the max/min sizes, and the upper left corner of each.
  class Layout {
   public:
    Layout();

    // Refresh the work area and balloon placement.
    void OnDisplaySettingsChanged();

    // TODO(johnnyg): Scale the size to account for the system font factor.
    int min_balloon_width() const { return kBalloonMinWidth; }
    int max_balloon_width() const { return kBalloonMaxWidth; }
    int min_balloon_height() const { return kBalloonMinHeight; }
    int max_balloon_height() const { return kBalloonMaxHeight; }

    // Returns both the total space available and the maximum
    // allowed per balloon.
    //
    // The size may be a height or length depending on the way that
    // balloons are laid out.
    const void GetMaxLinearSize(int* max_balloon_size, int* total_size) const;

    // Refresh the cached values for work area and drawing metrics.
    // The application should call this method to re-acquire metrics after
    // any resolution or settings change.
    // Returns true if and only if a metric changed.
    bool RefreshSystemMetrics();

    // Returns the origin for the sequence of balloons depending on layout.
    // Should not be used to place a balloon -- only to call NextPosition().
    gfx::Point GetLayoutOrigin() const;

    // Compute the position for the next balloon.
    // Start with *position_iterator = GetLayoutOrigin() and call repeatedly
    // to get a sequence of positions. Return value is the upper-left coordinate
    // for each next balloon.
    gfx::Point NextPosition(const gfx::Size& balloon_size,
                            gfx::Point* position_iterator) const;

   private:
    enum Placement {
      HORIZONTALLY_FROM_BOTTOM_LEFT,
      HORIZONTALLY_FROM_BOTTOM_RIGHT,
      VERTICALLY_FROM_TOP_RIGHT,
      VERTICALLY_FROM_BOTTOM_RIGHT
    };

    // Minimum and maximum size of balloon
    static const int kBalloonMinWidth = 300;
    static const int kBalloonMaxWidth = 300;
    static const int kBalloonMinHeight = 90;
    static const int kBalloonMaxHeight = 120;

    static Placement placement_;
    gfx::Rect work_area_;
    DISALLOW_COPY_AND_ASSIGN(Layout);
  };

  // Non-owned pointer to an object listening for space changes.
  BalloonSpaceChangeListener* space_change_listener_;

  // Queue of active balloons.
  typedef std::deque<Balloon*> Balloons;
  Balloons balloons_;

  // The layout parameters for balloons in this collection.
  Layout layout_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_H_
