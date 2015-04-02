// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/focus_controller.h"

#include "base/auto_reset.h"
#include "mojo/services/window_manager/focus_controller_observer.h"
#include "mojo/services/window_manager/focus_rules.h"
#include "mojo/services/window_manager/view_target.h"
#include "mojo/services/window_manager/window_manager_app.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_property.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view_tracker.h"
#include "ui/events/event.h"

DECLARE_VIEW_PROPERTY_TYPE(window_manager::FocusController*);

using mojo::View;

namespace window_manager {

namespace {
DEFINE_VIEW_PROPERTY_KEY(FocusController*, kRootViewFocusController, nullptr);
}  // namespace

FocusController::FocusController(scoped_ptr<FocusRules> rules)
    : active_view_(nullptr),
      focused_view_(nullptr),
      updating_focus_(false),
      updating_activation_(false),
      rules_(rules.Pass()),
      observer_manager_(this) {
  DCHECK(rules_);
}

FocusController::~FocusController() {}

void FocusController::AddObserver(FocusControllerObserver* observer) {
  focus_controller_observers_.AddObserver(observer);
}

void FocusController::RemoveObserver(FocusControllerObserver* observer) {
  focus_controller_observers_.RemoveObserver(observer);
}

void FocusController::ActivateView(View* view) {
  FocusView(view);
}

void FocusController::DeactivateView(View* view) {
  if (view)
    FocusView(rules_->GetNextActivatableView(view));
}

View* FocusController::GetActiveView() {
  return active_view_;
}

View* FocusController::GetActivatableView(View* view) {
  return rules_->GetActivatableView(view);
}

View* FocusController::GetToplevelView(View* view) {
  return rules_->GetToplevelView(view);
}

bool FocusController::CanActivateView(View* view) const {
  return rules_->CanActivateView(view);
}

void FocusController::FocusView(View* view) {
  if (view &&
      (view->Contains(focused_view_) || view->Contains(active_view_))) {
    return;
  }

  // Focusing a window also activates its containing activatable window. Note
  // that the rules could redirect activation activation and/or focus.
  View* focusable = rules_->GetFocusableView(view);
  View* activatable =
      focusable ? rules_->GetActivatableView(focusable) : nullptr;

  // We need valid focusable/activatable windows in the event we're not clearing
  // focus. "Clearing focus" is inferred by whether or not |window| passed to
  // this function is non-null.
  if (view && (!focusable || !activatable))
    return;
  DCHECK((focusable && activatable) || !view);

  // Activation change observers may change the focused window. If this happens
  // we must not adjust the focus below since this will clobber that change.
  View* last_focused_view = focused_view_;
  if (!updating_activation_)
    SetActiveView(view, activatable);

  // If the window's ActivationChangeObserver shifted focus to a valid window,
  // we don't want to focus the window we thought would be focused by default.
  bool activation_changed_focus = last_focused_view != focused_view_;
  if (!updating_focus_ && (!activation_changed_focus || !focused_view_)) {
    if (active_view_ && focusable)
      DCHECK(active_view_->Contains(focusable));
    SetFocusedView(focusable);
  }
}

void FocusController::ResetFocusWithinActiveView(View* view) {
  DCHECK(view);
  if (!active_view_)
    return;
  if (!active_view_->Contains(view))
    return;
  SetFocusedView(view);
}

View* FocusController::GetFocusedView() {
  return focused_view_;
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, ui::EventHandler implementation:

void FocusController::OnKeyEvent(ui::KeyEvent* event) {
}

void FocusController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED && !event->handled()) {
    View* view = static_cast<ViewTarget*>(event->target())->view();
    ViewFocusedFromInputEvent(view);
  }
}

void FocusController::OnScrollEvent(ui::ScrollEvent* event) {
}

void FocusController::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED && !event->handled()) {
    View* view = static_cast<ViewTarget*>(event->target())->view();
    ViewFocusedFromInputEvent(view);
  }
}

void FocusController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1 &&
      !event->handled()) {
    View* view = static_cast<ViewTarget*>(event->target())->view();
    ViewFocusedFromInputEvent(view);
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, mojo::ViewObserver implementation:

void FocusController::OnViewVisibilityChanged(View* view) {
  bool visible = view->visible();
  if (!visible)
    ViewLostFocusFromDispositionChange(view, view->parent());
}

void FocusController::OnViewDestroying(View* view) {
  ViewLostFocusFromDispositionChange(view, view->parent());
}

void FocusController::OnTreeChanging(const TreeChangeParams& params) {
  // TODO(erg): In the aura version, you could get into a situation where you
  // have different focus clients, so you had to check for that. Does that
  // happen here? Could we get away with not checking if it does?
  if (params.receiver == active_view_ &&
      params.target->Contains(params.receiver) &&
      (!params.new_parent ||
       /* different_focus_clients */ false)) {
    ViewLostFocusFromDispositionChange(params.receiver, params.old_parent);
  }
}

void FocusController::OnTreeChanged(const TreeChangeParams& params) {
  // TODO(erg): Same as Changing version.
  if (params.receiver == focused_view_ &&
      params.target->Contains(params.receiver) &&
      (!params.new_parent ||
       /* different_focus_clients */ false)) {
    ViewLostFocusFromDispositionChange(params.receiver, params.old_parent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusController, private:

void FocusController::SetFocusedView(View* view) {
  if (updating_focus_ || view == focused_view_)
    return;
  DCHECK(rules_->CanFocusView(view));
  if (view)
    DCHECK_EQ(view, rules_->GetFocusableView(view));

  base::AutoReset<bool> updating_focus(&updating_focus_, true);
  View* lost_focus = focused_view_;

  // TODO(erg): In the aura version, we reset the text input client here. Do
  // that if we bring in something like the TextInputClient.

  // Allow for the window losing focus to be deleted during dispatch. If it is
  // deleted pass null to observers instead of a deleted window.
  mojo::ViewTracker view_tracker;
  if (lost_focus)
    view_tracker.Add(lost_focus);
  if (focused_view_ && observer_manager_.IsObserving(focused_view_) &&
      focused_view_ != active_view_) {
    observer_manager_.Remove(focused_view_);
  }
  focused_view_ = view;
  if (focused_view_ && !observer_manager_.IsObserving(focused_view_))
    observer_manager_.Add(focused_view_);

  FOR_EACH_OBSERVER(FocusControllerObserver, focus_controller_observers_,
                    OnFocused(focused_view_));

  // TODO(erg): In aura, there's a concept of a single FocusChangeObserver that
  // is attached to an aura::Window. We don't currently have this in
  // mojo::View, but if we add it later, we should make something analogous
  // here.

  // TODO(erg): In the aura version, we reset the TextInputClient here, too.
}

void FocusController::SetActiveView(View* requested_view, View* view) {
  if (updating_activation_)
    return;

  if (view == active_view_) {
    if (requested_view) {
      FOR_EACH_OBSERVER(FocusControllerObserver,
                        focus_controller_observers_,
                        OnAttemptToReactivateView(requested_view,
                                                  active_view_));
    }
    return;
  }

  DCHECK(rules_->CanActivateView(view));
  if (view)
    DCHECK_EQ(view, rules_->GetActivatableView(view));

  base::AutoReset<bool> updating_activation(&updating_activation_, true);
  View* lost_activation = active_view_;
  // Allow for the window losing activation to be deleted during dispatch. If
  // it is deleted pass null to observers instead of a deleted window.
  mojo::ViewTracker view_tracker;
  if (lost_activation)
    view_tracker.Add(lost_activation);
  if (active_view_ && observer_manager_.IsObserving(active_view_) &&
      focused_view_ != active_view_) {
    observer_manager_.Remove(active_view_);
  }
  active_view_ = view;
  if (active_view_ && !observer_manager_.IsObserving(active_view_))
    observer_manager_.Add(active_view_);

  if (active_view_) {
    // TODO(erg): Reenable this when we have modal windows.
    // StackTransientParentsBelowModalWindow(active_view_);

    active_view_->MoveToFront();
  }

  // TODO(erg): Individual windows can have a single ActivationChangeObserver
  // set on them. In the aura version of this code, it sends an
  // OnWindowActivated message to both the window that lost activation, and the
  // window that gained it.

  FOR_EACH_OBSERVER(FocusControllerObserver, focus_controller_observers_,
                    OnActivated(active_view_));
}

void FocusController::ViewLostFocusFromDispositionChange(
    View* view,
    View* next) {
  // TODO(erg): We clear the modality state here in the aura::Window version of
  // this class, and should probably do the same once we have modal windows.

  // TODO(beng): See if this function can be replaced by a call to
  //             FocusWindow().
  // Activation adjustments are handled first in the event of a disposition
  // changed. If an activation change is necessary, focus is reset as part of
  // that process so there's no point in updating focus independently.
  if (view == active_view_) {
    View* next_activatable = rules_->GetNextActivatableView(view);
    SetActiveView(nullptr, next_activatable);
    if (!(active_view_ && active_view_->Contains(focused_view_)))
      SetFocusedView(next_activatable);
  } else if (view->Contains(focused_view_)) {
    // Active window isn't changing, but focused window might be.
    SetFocusedView(rules_->GetFocusableView(next));
  }
}

void FocusController::ViewFocusedFromInputEvent(View* view) {
  // Only focus |window| if it or any of its parents can be focused. Otherwise
  // FocusWindow() will focus the topmost window, which may not be the
  // currently focused one.
  if (rules_->CanFocusView(GetToplevelView(view)))
    FocusView(view);
}

void SetFocusController(View* root_view, FocusController* focus_controller) {
  DCHECK_EQ(root_view->GetRoot(), root_view);
  root_view->SetLocalProperty(kRootViewFocusController, focus_controller);
}

FocusController* GetFocusController(View* root_view) {
  if (root_view)
    DCHECK_EQ(root_view->GetRoot(), root_view);
  return root_view ?
      root_view->GetLocalProperty(kRootViewFocusController) : nullptr;
}

}  // namespace window_manager
