// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_FOCUS_CONTROLLER_H_
#define SERVICES_WINDOW_MANAGER_FOCUS_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_observer.h"
#include "ui/events/event_handler.h"

namespace window_manager {

class FocusControllerObserver;
class FocusRules;

// FocusController handles focus and activation changes in a mojo window
// manager. Within the window manager, there can only be one focused and one
// active window at a time. When focus or activation changes, notifications are
// sent using the FocusControllerObserver interface.
class FocusController : public ui::EventHandler, public mojo::ViewObserver {
 public:
  // |rules| cannot be null.
  explicit FocusController(scoped_ptr<FocusRules> rules);
  ~FocusController() override;

  void AddObserver(FocusControllerObserver* observer);
  void RemoveObserver(FocusControllerObserver* observer);

  void ActivateView(mojo::View* view);
  void DeactivateView(mojo::View* view);
  mojo::View* GetActiveView();
  mojo::View* GetActivatableView(mojo::View* view);
  mojo::View* GetToplevelView(mojo::View* view);
  bool CanActivateView(mojo::View* view) const;

  void FocusView(mojo::View* view);

  void ResetFocusWithinActiveView(mojo::View* view);
  mojo::View* GetFocusedView();

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from ViewObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnViewVisibilityChanged(mojo::View* view) override;
  void OnViewDestroying(mojo::View* view) override;

 private:
  // Internal implementation that sets the focused view, fires events etc.
  // This function must be called with a valid focusable view.
  void SetFocusedView(mojo::View* view);

  // Internal implementation that sets the active window, fires events etc.
  // This function must be called with a valid |activatable_window|.
  // |requested window| refers to the window that was passed in to an external
  // request (e.g. FocusWindow or ActivateWindow). It may be null, e.g. if
  // SetActiveWindow was not called by an external request. |activatable_window|
  // refers to the actual window to be activated, which may be different.
  void SetActiveView(mojo::View* requested_view, mojo::View* activatable_view);

  // Called when a window's disposition changed such that it and its hierarchy
  // are no longer focusable/activatable. |next| is a valid window that is used
  // as a starting point for finding a window to focus next based on rules.
  void ViewLostFocusFromDispositionChange(mojo::View* view, mojo::View* next);

  // Called when an attempt is made to focus or activate a window via an input
  // event targeted at that window. Rules determine the best focusable window
  // for the input window.
  void ViewFocusedFromInputEvent(mojo::View* view);

  mojo::View* active_view_;
  mojo::View* focused_view_;

  bool updating_focus_;
  bool updating_activation_;

  scoped_ptr<FocusRules> rules_;

  ObserverList<FocusControllerObserver> focus_controller_observers_;

  ScopedObserver<mojo::View, ViewObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

// Sets/Gets the focus controller for a view.
void SetFocusController(mojo::View* view, FocusController* focus_controller);
FocusController* GetFocusController(mojo::View* view);

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_FOCUS_CONTROLLER_H_
