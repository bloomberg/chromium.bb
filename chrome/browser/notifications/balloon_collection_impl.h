// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles the visible notification (or balloons).

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_

#include <deque>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/balloon_collection_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

// A balloon collection represents a set of notification balloons being
// shown on the screen.  It positions new notifications according to
// a layout, and monitors for balloons being closed, which it reports
// up to its parent, the notification UI manager.
class BalloonCollectionImpl : public BalloonCollection,
                              public content::NotificationObserver,
                              public base::MessageLoopForUI::Observer {
 public:
  BalloonCollectionImpl();
  virtual ~BalloonCollectionImpl();

  // BalloonCollection interface.
  virtual void Add(const Notification& notification,
                   Profile* profile) OVERRIDE;
  virtual const Notification* FindById(const std::string& id) const OVERRIDE;
  virtual bool RemoveById(const std::string& id) OVERRIDE;
  virtual bool RemoveBySourceOrigin(const GURL& source_origin) OVERRIDE;
  virtual bool RemoveByProfile(Profile* profile) OVERRIDE;
  virtual void RemoveAll() OVERRIDE;
  virtual bool HasSpace() const OVERRIDE;
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size) OVERRIDE;
  virtual void SetPositionPreference(PositionPreference position) OVERRIDE;
  virtual void DisplayChanged() OVERRIDE;
  virtual void OnBalloonClosed(Balloon* source) OVERRIDE;
  virtual const Balloons& GetActiveBalloons() OVERRIDE;

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // MessageLoopForUI::Observer interface.
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE;
  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE;

  // base_ is embedded, so this is a simple accessor for the number of
  // balloons in the collection.
  int count() const { return base_.count(); }

  static int min_balloon_width() { return Layout::min_balloon_width(); }
  static int max_balloon_width() { return Layout::max_balloon_width(); }
  static int min_balloon_height() { return Layout::min_balloon_height(); }
  static int max_balloon_height() { return Layout::max_balloon_height(); }

 protected:
  // Calculates layout values for the balloons including
  // the scaling, the max/min sizes, and the upper left corner of each.
  class Layout {
   public:
    Layout();

    // These enumerations all are based on a screen orientation where
    // the origin is the top-left.
    enum Placement {
      INVALID,
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

    // Returns true if there is change to the offset that requires the balloons
    // to be repositioned.
    bool ComputeOffsetToMoveAbovePanels();

    void enable_computing_panel_offset() {
      need_to_compute_panel_offset_ = true;
    }

   private:
    // Layout parameters
    int VerticalEdgeMargin() const;
    int HorizontalEdgeMargin() const;
    int InterBalloonMargin() const;

    bool NeedToMoveAboveLeftSidePanels() const;
    bool NeedToMoveAboveRightSidePanels() const;

    // Minimum and maximum size of balloon content.
    static const int kBalloonMinWidth = 300;
    static const int kBalloonMaxWidth = 300;
    static const int kBalloonMinHeight = 24;
    static const int kBalloonMaxHeight = 160;

    Placement placement_;
    gfx::Rect work_area_;

    // The flag that indicates that the panel offset computation should be
    // performed.
    bool need_to_compute_panel_offset_;

    // The offset that guarantees that the notificaitions shown in the
    // lower-right or lower-left corner of the screen will go above currently
    // shown panels and will not be obscured by them.
    int offset_to_move_above_panels_;

    DISALLOW_COPY_AND_ASSIGN(Layout);
  };

  // Creates a new balloon. Overridable by derived classes and unit tests.
  // The caller is responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile);

  // Protected implementation of Add with additional add_to_front parameter
  // for use by derived classes.
  void AddImpl(const Notification& notification,
               Profile* profile,
               bool add_to_front);

  // Gets a bounding box for all the current balloons in screen coordinates.
  gfx::Rect GetBalloonsBoundingBox() const;

  BalloonCollectionBase& base() { return base_; }
  Layout& layout() { return layout_; }

 private:
  // Adjusts the positions of the balloons (e.g., when one is closed).
  // Implemented by each platform for specific UI requirements.
  void PositionBalloons(bool is_reposition);

  // Cross-platform internal implementation for PositionBalloons.
  void PositionBalloonsInternal(bool is_reposition);

  // Base implementation for the collection of active balloons.
  BalloonCollectionBase base_;

  // The layout parameters for balloons in this collection.
  Layout layout_;

  content::NotificationRegistrar registrar_;

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
  base::WeakPtrFactory<BalloonCollectionImpl> reposition_factory_;

  // Is the balloon collection currently listening for UI events?
  bool added_as_message_loop_observer_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_COLLECTION_IMPL_H_
