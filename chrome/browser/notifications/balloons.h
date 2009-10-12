// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOONS_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOONS_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/notifications/notification.h"

class Balloon;
class BalloonView;
class Profile;
class SiteInstance;

class BalloonCloseListener {
 public:
  virtual ~BalloonCloseListener() { }

  // Called when a balloon is closed.
  virtual void OnBalloonClosed(Balloon* source) = 0;
};

class BalloonSpaceChangeListener {
 public:
  virtual ~BalloonSpaceChangeListener() { }

  // Called when there is more or less space for balloons due to
  // monitor size changes or balloons disappearing.
  virtual void OnBalloonSpaceChanged() = 0;
};

class BalloonCollectionInterface {
 public:
  virtual ~BalloonCollectionInterface() {}

  // Adds a new balloon for the specified notification.
  virtual void Add(const Notification& notification,
                   Profile* profile,
                   SiteInstance* site_instance) = 0;

  // Is there room to add another notification?
  virtual bool HasSpace() const = 0;
};

typedef std::deque<Balloon*> Balloons;

// Represents a Notification on the screen.
class Balloon {
 public:
  Balloon(const Notification& notification,
          Profile* profile,
          SiteInstance* site_instance,
          BalloonCloseListener* listener);
  ~Balloon();

  const Notification& notification() const {
    return notification_;
  }

  Profile* profile() const {
    return profile_;
  }

  SiteInstance* site_instance() const {
    return site_instance_;
  }

  void SetPosition(const gfx::Point& upper_left, bool reposition);
  void SetSize(const gfx::Size& size);
  void Show();
  void Close();

  const gfx::Point& position() const;
  const gfx::Size& size() const;

 private:
  Profile* profile_;
  SiteInstance* site_instance_;
  Notification notification_;
  BalloonCloseListener* close_listener_;
  scoped_ptr<BalloonView> balloon_view_;
  gfx::Point position_;
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(Balloon);
};

class BalloonCollection : public BalloonCollectionInterface,
                          public BalloonCloseListener {
 public:
  explicit BalloonCollection();

  BalloonSpaceChangeListener* space_change_listener() { return listener_; }
  void set_space_change_listener(BalloonSpaceChangeListener* listener) {
    listener_ = listener;
  }

  // BalloonCollectionInterface overrides
  virtual void Add(const Notification& notification,
                   Profile* profile,
                   SiteInstance* site_instance);
  virtual void ShowAll();
  virtual void HideAll();
  virtual bool HasSpace() const;

  // BalloonCloseListener interface
  virtual void OnBalloonClosed(Balloon* source);

 protected:
  // Overridable by unit tests.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile,
                               SiteInstance* site_instance) {
    return new Balloon(notification, profile, site_instance, this);
  }

 private:
  // The number of balloons being displayed.
  int count() const { return balloons_.size(); }

  // Calculates layout values for the balloons including
  // the scaling, the max/min sizes, and the upper left corner of each.
  class Layout {
   public:
    Layout();

    // Refresh the work area and balloon placement.
    void OnDisplaySettingsChanged();

    int min_balloon_width() const;
    int max_balloon_width() const;
    int min_balloon_height() const;
    int max_balloon_height() const;

    // Returns both the total length available and the maximum
    // allowed per balloon.
    //
    // The length may be a height or length depending on the way that
    // balloons are laid out.
    const void GetMaxLengths(int* max_balloon_length, int* total_length) const;

    // Scale the size to count in the system font factor.
    int ScaleSize(int size) const;

    // Refresh the cached values for work area and drawing metrics.
    // This is done automatically first time and the application should
    // call this method to re-acquire metrics after any
    // resolution or settings change.
    //
    // Return true if and only if a metric changed.
    bool RefreshSystemMetrics();

    // Returns the starting value for NextUpperLeftPosition.
    gfx::Point GetStartPosition() const;

    // Compute the position for the next balloon.
    //   Modifies origin.
    //   Returns the position of the upper left coordinate for the given
    //   balloon.
    gfx::Point NextPosition(const gfx::Size& balloon_size,
                            gfx::Point* origin) const;

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
    double font_scale_factor_;
    DISALLOW_COPY_AND_ASSIGN(Layout);
  };

  // Non-owned pointer to an object listening for space changes.
  BalloonSpaceChangeListener* listener_;

  Balloons balloons_;
  Layout layout_;
  DISALLOW_COPY_AND_ASSIGN(BalloonCollection);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOONS_H_
