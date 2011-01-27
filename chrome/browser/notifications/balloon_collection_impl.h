// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
#pragma once

#include <deque>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/balloon_collection_base.h"
#include "gfx/point.h"
#include "gfx/rect.h"

// Mac balloons grow from the top down and have close buttons on top, so
// offsetting is not necessary for easy multiple-closing.  Other platforms grow
// from the bottom up and have close buttons on top, so it is necessary.
#if defined(OS_MACOSX)
#define USE_OFFSETS 0
#else
#define USE_OFFSETS 1
#endif

// A balloon collection represents a set of notification balloons being
// shown on the screen.  It positions new notifications according to
// a layout, and monitors for balloons being closed, which it reports
// up to its parent, the notification UI manager.
class BalloonCollectionImpl : public BalloonCollection
#if USE_OFFSETS
                            , public MessageLoopForUI::Observer
#endif
{
 public:
  BalloonCollectionImpl();
  virtual ~BalloonCollectionImpl();

  // BalloonCollection interface.
  virtual void Add(const Notification& notification,
                   Profile* profile);
  virtual bool RemoveById(const std::string& id);
  virtual bool RemoveBySourceOrigin(const GURL& source_origin);
  virtual void RemoveAll();
  virtual bool HasSpace() const;
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size);
  virtual void SetPositionPreference(PositionPreference position);
  virtual void DisplayChanged();
  virtual void OnBalloonClosed(Balloon* source);
  virtual const Balloons& GetActiveBalloons() { return base_.balloons(); }

  // MessageLoopForUI::Observer interface.
#if defined(OS_WIN)
  virtual void WillProcessMessage(const MSG& event) {}
  virtual void DidProcessMessage(const MSG& event);
#endif
#if defined(OS_LINUX)
  virtual void WillProcessEvent(GdkEvent* event) {}
  virtual void DidProcessEvent(GdkEvent* event);
#endif

 protected:
  // Calculates layout values for the balloons including
  // the scaling, the max/min sizes, and the upper left corner of each.
  class Layout {
   public:
    Layout();

    // These enumerations all are based on a screen orientation where
    // the origin is the top-left.
    enum Placement {
      VERTICALLY_FROM_TOP_LEFT,
      VERTICALLY_FROM_TOP_RIGHT,
      VERTICALLY_FROM_BOTTOM_LEFT,
      VERTICALLY_FROM_BOTTOM_RIGHT
    };

    // Refresh the work area and balloon placement.
    void OnDisplaySettingsChanged();

    // TODO(johnnyg): Scale the size to account for the system font factor.
    static int min_balloon_width() { return kBalloonMinWidth; }
    static int max_balloon_width() { return kBalloonMaxWidth; }
    static int min_balloon_height() { return kBalloonMinHeight; }
    static int max_balloon_height() { return kBalloonMaxHeight; }

    // Utility function constrains the input rectangle to the min and max sizes.
    static gfx::Size ConstrainToSizeLimits(const gfx::Size& rect);

    void set_placement(Placement placement) { placement_ = placement; }

    // Returns both the total space available and the maximum
    // allowed per balloon.
    //
    // The size may be a height or length depending on the way that
    // balloons are laid out.
    void GetMaxLinearSize(int* max_balloon_size, int* total_size) const;

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

    // Return a offscreen location which is offscreen for this layout,
    // to be used as the initial position for an animation into view.
    gfx::Point OffScreenLocation() const;

    // Returns true if the layout requires offsetting for keeping the close
    // buttons under the cursor during rapid-close interaction.
    bool RequiresOffsets() const;

   private:
    // Layout parameters
    int VerticalEdgeMargin() const;
    int HorizontalEdgeMargin() const;
    int InterBalloonMargin() const;

    // Minimum and maximum size of balloon content.
    static const int kBalloonMinWidth = 300;
    static const int kBalloonMaxWidth = 300;
    static const int kBalloonMinHeight = 24;
    static const int kBalloonMaxHeight = 160;

    Placement placement_;
    gfx::Rect work_area_;
    DISALLOW_COPY_AND_ASSIGN(Layout);
  };

  // Creates a new balloon. Overridable by unit tests.  The caller is
  // responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);

  // Gets a bounding box for all the current balloons in screen coordinates.
  gfx::Rect GetBalloonsBoundingBox() const;

 private:
  // Adjusts the positions of the balloons (e.g., when one is closed).
  // Implemented by each platform for specific UI requirements.
  void PositionBalloons(bool is_reposition);

  // Cross-platform internal implementation for PositionBalloons.
  void PositionBalloonsInternal(bool is_reposition);

#if defined(OS_MACOSX)
  // Get the work area on Mac OS, without inverting the coordinates.
  static gfx::Rect GetMacWorkArea();
#endif

  // Base implementation for the collection of active balloons.
  BalloonCollectionBase base_;

  // The layout parameters for balloons in this collection.
  Layout layout_;

#if USE_OFFSETS
  // Start and stop observing all UI events.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // Cancel all offset and reposition the balloons normally.
  void CancelOffsets();

  // Handles a mouse motion while the balloons are temporarily offset.
  void HandleMouseMoveEvent();

  // Is the current cursor in the balloon area?
  bool IsCursorInBalloonCollection() const;

  // Factory for generating delayed reposition tasks on mouse motion.
  ScopedRunnableMethodFactory<BalloonCollectionImpl> reposition_factory_;

  // Is the balloon collection currently listening for UI events?
  bool added_as_message_loop_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
