// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_CAPTION_BUTTONS_FRAME_MAXIMIZE_BUTTON_H_
#define ASH_FRAME_CAPTION_BUTTONS_FRAME_MAXIMIZE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/frame/caption_buttons/caption_button_types.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/wm/workspace/snap_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace ash {
class FrameMaximizeButtonObserver;
class MaximizeBubbleController;
class PhantomWindowController;

// Button used for the maximize control on the frame. Handles snapping logic.
class ASH_EXPORT FrameMaximizeButton : public FrameCaptionButton,
                                       public views::WidgetObserver,
                                       public aura::WindowObserver {
 public:
  FrameMaximizeButton(views::ButtonListener* listener,
                      views::Widget* frame);
  virtual ~FrameMaximizeButton();

  void AddObserver(FrameMaximizeButtonObserver* observer);
  void RemoveObserver(FrameMaximizeButtonObserver* observer);

  // Updates |snap_type_| based on a a given snap type. This is used by
  // external hover events from the button menu.
  void SnapButtonHovered(SnapType type);

  // The user clicked the |type| button and the action needs to be performed,
  // which will at the same time close the window.
  void ExecuteSnapAndCloseMenu(SnapType type);

  // Called by the MaximizeBubbleController when the maximize bubble is shown.
  void OnMaximizeBubbleShown(views::Widget* bubble);

  // Remove the maximize menu from the screen (and destroy it).
  void DestroyMaximizeMenu();

  // Returns true when the user clicks and drags the button.
  bool is_snap_enabled() const { return is_snap_enabled_; }

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // WidgetObserver overrides:
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                        bool active) OVERRIDE;

  // ImageButton overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // views::View overwrite:
  virtual void SetVisible(bool visible) OVERRIDE;

  // Unit test overwrite: Change the UI delay used for the bubble show up.
  void set_bubble_appearance_delay_ms(int bubble_appearance_delay_ms) {
    bubble_appearance_delay_ms_ = bubble_appearance_delay_ms;
  }

  // Unit test accessor for the maximize bubble.
  MaximizeBubbleController* maximizer() { return maximizer_.get(); }

  // Unit test to see if phantom window is open.
  bool phantom_window_open() { return phantom_window_.get() != NULL; }

 private:
  class EscapeEventFilter;

  // Initializes the snap-gesture based on the event. This should only be called
  // when the event is confirmed to have started a snap gesture.
  void ProcessStartEvent(const ui::LocatedEvent& event);

  // Updates the snap-state based on the current event. This should only be
  // called after the snap gesture has already started.
  void ProcessUpdateEvent(const ui::LocatedEvent& event);

  // Returns true if the window was snapped. Returns false otherwise.
  bool ProcessEndEvent(const ui::LocatedEvent& event);

  // Cancels snap behavior. If |keep_menu_open| is set, a possibly opened
  // bubble help will remain open.
  void Cancel(bool keep_menu_open);

  // Installs/uninstalls an EventFilter to track when escape is pressed.
  void InstallEventFilter();
  void UninstallEventFilter();

  // Updates the snap position from the event location. This is invoked by
  // |update_timer_|.
  void UpdateSnapFromEventLocation();

  // Updates |snap_type_| based on a mouse drag.
  void UpdateSnap(const gfx::Point& location);

  // Returns the type of snap based on the specified location.
  SnapType SnapTypeForLocation(const gfx::Point& location) const;

  // Returns the bounds of the resulting window for the specified type.
  gfx::Rect ScreenBoundsForType(SnapType type) const;

  // Snaps the window to the current snap position determined by |snap_type_|.
  void Snap();

  // Determine the maximize type of this window.
  MaximizeBubbleFrameState GetMaximizeBubbleFrameState() const;

  // Widget that the maximize button acts on. This is different than the widget
  // which contains the button in the case of AppNonClientFrameViewAsh.
  views::Widget* frame_;

  // True if we have put observers on |frame_|.
  bool observing_frame_;

  // Renders the snap position.
  scoped_ptr<PhantomWindowController> phantom_window_;

  // Is snapping enabled? Set on press so that in drag we know whether we
  // should show the snap locations.
  bool is_snap_enabled_;

  // Did the user drag far enough to trigger snapping?
  bool exceeded_drag_threshold_;

  // Location of the press.
  gfx::Point press_location_;

  // Current snap type.
  SnapType snap_type_;

  scoped_ptr<EscapeEventFilter> escape_event_filter_;

  base::OneShotTimer<FrameMaximizeButton> update_timer_;

  scoped_ptr<MaximizeBubbleController> maximizer_;

  // The delay of the bubble appearance.
  int bubble_appearance_delay_ms_;

  ObserverList<FrameMaximizeButtonObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FrameMaximizeButton);
};

}  // namespace ash

#endif  // ASH_FRAME_CAPTION_BUTTONS_FRAME_MAXIMIZE_BUTTON_H_
