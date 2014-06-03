// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_fullscreen_controller.h"

#include <set>

#include "ash/ash_constants.h"
#include "ash/shell.h"
#include "ash/wm/resize_handle_window_targeter.h"
#include "ash/wm/window_state.h"
#include "base/metrics/histogram.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

using views::View;

namespace ash {

namespace {

// Duration for the reveal show/hide slide animation. The slower duration is
// used for the initial slide out to give the user more change to see what
// happened.
const int kRevealSlowAnimationDurationMs = 400;
const int kRevealFastAnimationDurationMs = 200;

// The delay in milliseconds between the mouse stopping at the top edge of the
// screen and the top-of-window views revealing.
const int kMouseRevealDelayMs = 200;

// The maximum amount of pixels that the cursor can move for the cursor to be
// considered "stopped". This allows the user to reveal the top-of-window views
// without holding the cursor completely still.
const int kMouseRevealXThresholdPixels = 3;

// Used to multiply x value of an update in check to determine if gesture is
// vertical. This is used to make sure that gesture is close to vertical instead
// of just more vertical then horizontal.
const int kSwipeVerticalThresholdMultiplier = 3;

// The height in pixels of the region above the top edge of the display which
// hosts the immersive fullscreen window in which mouse events are ignored
// (cannot reveal or unreveal the top-of-window views).
// See ShouldIgnoreMouseEventAtLocation() for more details.
const int kHeightOfDeadRegionAboveTopContainer = 10;

// Returns the BubbleDelegateView corresponding to |maybe_bubble| if
// |maybe_bubble| is a bubble.
views::BubbleDelegateView* AsBubbleDelegate(aura::Window* maybe_bubble) {
  if (!maybe_bubble)
    return NULL;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(maybe_bubble);
  if (!widget)
    return NULL;
  return widget->widget_delegate()->AsBubbleDelegate();
}

// Returns true if |maybe_transient| is a transient child of |toplevel|.
bool IsWindowTransientChildOf(aura::Window* maybe_transient,
                              aura::Window* toplevel) {
  if (!maybe_transient || !toplevel)
    return false;

  for (aura::Window* window = maybe_transient; window;
       window = ::wm::GetTransientParent(window)) {
    if (window == toplevel)
      return true;
  }
  return false;
}

// Returns the location of |event| in screen coordinates.
gfx::Point GetEventLocationInScreen(const ui::LocatedEvent& event) {
  gfx::Point location_in_screen = event.location();
  aura::Window* target = static_cast<aura::Window*>(event.target());
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(target->GetRootWindow());
  screen_position_client->ConvertPointToScreen(target, &location_in_screen);
  return location_in_screen;
}

// Returns the bounds of the display nearest to |window| in screen coordinates.
gfx::Rect GetDisplayBoundsInScreen(aura::Window* window) {
  return Shell::GetScreen()->GetDisplayNearestWindow(window).bounds();
}

}  // namespace

// The height in pixels of the region below the top edge of the display in which
// the mouse can trigger revealing the top-of-window views.
#if defined(OS_WIN)
// Windows 8 reserves some pixels at the top of the screen for the hand icon
// that allows you to drag a metro app off the screen, so a few additional
// pixels of space must be reserved for the mouse reveal.
const int ImmersiveFullscreenController::kMouseRevealBoundsHeight = 9;
#else
// The height must be greater than 1px because the top pixel is used to trigger
// moving the cursor between displays if the user has a vertical display layout
// (primary display above/below secondary display).
const int ImmersiveFullscreenController::kMouseRevealBoundsHeight = 3;
#endif

////////////////////////////////////////////////////////////////////////////////

// Class which keeps the top-of-window views revealed as long as one of the
// bubbles it is observing is visible. The logic to keep the top-of-window
// views revealed based on the visibility of bubbles anchored to
// children of |ImmersiveFullscreenController::top_container_| is separate from
// the logic related to |ImmersiveFullscreenController::focus_revealed_lock_|
// so that bubbles which are not activatable and bubbles which do not close
// upon deactivation also keep the top-of-window views revealed for the
// duration of their visibility.
class ImmersiveFullscreenController::BubbleManager
    : public aura::WindowObserver {
 public:
  explicit BubbleManager(ImmersiveFullscreenController* controller);
  virtual ~BubbleManager();

  // Start / stop observing changes to |bubble|'s visibility.
  void StartObserving(aura::Window* bubble);
  void StopObserving(aura::Window* bubble);

 private:
  // Updates |revealed_lock_| based on whether any of |bubbles_| is visible.
  void UpdateRevealedLock();

  // aura::WindowObserver overrides:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  ImmersiveFullscreenController* controller_;

  std::set<aura::Window*> bubbles_;

  // Lock which keeps the top-of-window views revealed based on whether any of
  // |bubbles_| is visible.
  scoped_ptr<ImmersiveRevealedLock> revealed_lock_;

  DISALLOW_COPY_AND_ASSIGN(BubbleManager);
};

ImmersiveFullscreenController::BubbleManager::BubbleManager(
    ImmersiveFullscreenController* controller)
    : controller_(controller) {
}

ImmersiveFullscreenController::BubbleManager::~BubbleManager() {
  for (std::set<aura::Window*>::const_iterator it = bubbles_.begin();
       it != bubbles_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
}

void ImmersiveFullscreenController::BubbleManager::StartObserving(
    aura::Window* bubble) {
  if (bubbles_.insert(bubble).second) {
    bubble->AddObserver(this);
    UpdateRevealedLock();
  }
}

void ImmersiveFullscreenController::BubbleManager::StopObserving(
    aura::Window* bubble) {
  if (bubbles_.erase(bubble)) {
    bubble->RemoveObserver(this);
    UpdateRevealedLock();
  }
}

void ImmersiveFullscreenController::BubbleManager::UpdateRevealedLock() {
  bool has_visible_bubble = false;
  for (std::set<aura::Window*>::const_iterator it = bubbles_.begin();
       it != bubbles_.end(); ++it) {
    if ((*it)->IsVisible()) {
      has_visible_bubble = true;
      break;
    }
  }

  bool was_revealed = controller_->IsRevealed();
  if (has_visible_bubble) {
    if (!revealed_lock_.get()) {
      // Reveal the top-of-window views without animating because it looks
      // weird for the top-of-window views to animate and the bubble not to
      // animate along with the top-of-window views.
      revealed_lock_.reset(controller_->GetRevealedLock(
          ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
    }
  } else {
    revealed_lock_.reset();
  }

  if (!was_revealed && revealed_lock_.get()) {
    // Currently, there is no nice way for bubbles to reposition themselves
    // whenever the anchor view moves. Tell the bubbles to reposition themselves
    // explicitly instead. The hidden bubbles are also repositioned because
    // BubbleDelegateView does not reposition its widget as a result of a
    // visibility change.
    for (std::set<aura::Window*>::const_iterator it = bubbles_.begin();
         it != bubbles_.end(); ++it) {
      AsBubbleDelegate(*it)->OnAnchorBoundsChanged();
    }
  }
}

void ImmersiveFullscreenController::BubbleManager::OnWindowVisibilityChanged(
    aura::Window*,
    bool visible) {
  UpdateRevealedLock();
}

void ImmersiveFullscreenController::BubbleManager::OnWindowDestroying(
    aura::Window* window) {
  StopObserving(window);
}

////////////////////////////////////////////////////////////////////////////////

ImmersiveFullscreenController::ImmersiveFullscreenController()
    : delegate_(NULL),
      top_container_(NULL),
      widget_(NULL),
      native_window_(NULL),
      observers_enabled_(false),
      enabled_(false),
      reveal_state_(CLOSED),
      revealed_lock_count_(0),
      mouse_x_when_hit_top_in_screen_(-1),
      gesture_begun_(false),
      animation_(new gfx::SlideAnimation(this)),
      animations_disabled_for_test_(false),
      weak_ptr_factory_(this) {
}

ImmersiveFullscreenController::~ImmersiveFullscreenController() {
  EnableWindowObservers(false);
}

void ImmersiveFullscreenController::Init(Delegate* delegate,
                                         views::Widget* widget,
                                         views::View* top_container) {
  delegate_ = delegate;
  top_container_ = top_container;
  widget_ = widget;
  native_window_ = widget_->GetNativeWindow();
  native_window_->SetEventTargeter(scoped_ptr<ui::EventTargeter>(
      new ResizeHandleWindowTargeter(native_window_, this)));
}

void ImmersiveFullscreenController::SetEnabled(WindowType window_type,
                                               bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  EnableWindowObservers(enabled_);

  ash::wm::WindowState* window_state = wm::GetWindowState(native_window_);
  // Auto hide the shelf in immersive fullscreen instead of hiding it.
  window_state->set_hide_shelf_when_fullscreen(!enabled);

  // Update the window's immersive mode state for the window manager.
  window_state->set_in_immersive_fullscreen(enabled);

  Shell::GetInstance()->UpdateShelfVisibility();

  if (enabled_) {
    // Animate enabling immersive mode by sliding out the top-of-window views.
    // No animation occurs if a lock is holding the top-of-window views open.

    // Do a reveal to set the initial state for the animation. (And any
    // required state in case the animation cannot run because of a lock holding
    // the top-of-window views open.)
    MaybeStartReveal(ANIMATE_NO);

    // Reset the located event and the focus revealed locks so that they do not
    // affect whether the top-of-window views are hidden.
    located_event_revealed_lock_.reset();
    focus_revealed_lock_.reset();

    // Try doing the animation.
    MaybeEndReveal(ANIMATE_SLOW);

    if (reveal_state_ == REVEALED) {
      // Reveal was unsuccessful. Reacquire the revealed locks if appropriate.
      UpdateLocatedEventRevealedLock(NULL);
      UpdateFocusRevealedLock();
    } else {
      // Clearing focus is important because it closes focus-related popups like
      // the touch selection handles.
      widget_->GetFocusManager()->ClearFocus();
    }
  } else {
    // Stop cursor-at-top tracking.
    top_edge_hover_timer_.Stop();
    reveal_state_ = CLOSED;

    delegate_->OnImmersiveFullscreenExited();
  }

  if (enabled_) {
    UMA_HISTOGRAM_ENUMERATION("Ash.ImmersiveFullscreen.WindowType",
                              window_type,
                              WINDOW_TYPE_COUNT);
  }
}

bool ImmersiveFullscreenController::IsEnabled() const {
  return enabled_;
}

bool ImmersiveFullscreenController::IsRevealed() const {
  return enabled_ && reveal_state_ != CLOSED;
}

ImmersiveRevealedLock* ImmersiveFullscreenController::GetRevealedLock(
    AnimateReveal animate_reveal) {
  return new ImmersiveRevealedLock(weak_ptr_factory_.GetWeakPtr(),
                                   animate_reveal);
}

////////////////////////////////////////////////////////////////////////////////
// Testing interface:

void ImmersiveFullscreenController::SetupForTest() {
  DCHECK(!enabled_);
  animations_disabled_for_test_ = true;

  // Move the mouse off of the top-of-window views so that it does not keep the
  // top-of-window views revealed.
  std::vector<gfx::Rect> bounds_in_screen(
      delegate_->GetVisibleBoundsInScreen());
  DCHECK(!bounds_in_screen.empty());
  int bottommost_in_screen = bounds_in_screen[0].bottom();
  for (size_t i = 1; i < bounds_in_screen.size(); ++i) {
    if (bounds_in_screen[i].bottom() > bottommost_in_screen)
      bottommost_in_screen = bounds_in_screen[i].bottom();
  }
  gfx::Point cursor_pos(0, bottommost_in_screen + 100);
  aura::Env::GetInstance()->set_last_mouse_location(cursor_pos);
  UpdateLocatedEventRevealedLock(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void ImmersiveFullscreenController::OnMouseEvent(ui::MouseEvent* event) {
  if (!enabled_)
    return;

  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_PRESSED &&
      event->type() != ui::ET_MOUSE_RELEASED &&
      event->type() != ui::ET_MOUSE_CAPTURE_CHANGED) {
    return;
  }

  // Mouse hover can initiate revealing the top-of-window views while |widget_|
  // is inactive.

  if (reveal_state_ == SLIDING_OPEN || reveal_state_ == REVEALED) {
    top_edge_hover_timer_.Stop();
    UpdateLocatedEventRevealedLock(event);
  } else if (event->type() != ui::ET_MOUSE_CAPTURE_CHANGED) {
    // Trigger a reveal if the cursor pauses at the top of the screen for a
    // while.
    UpdateTopEdgeHoverTimer(event);
  }
}

void ImmersiveFullscreenController::OnTouchEvent(ui::TouchEvent* event) {
  if (!enabled_ || event->type() != ui::ET_TOUCH_PRESSED)
    return;

  // Touch should not initiate revealing the top-of-window views while |widget_|
  // is inactive.
  if (!widget_->IsActive())
    return;

  UpdateLocatedEventRevealedLock(event);
}

void ImmersiveFullscreenController::OnGestureEvent(ui::GestureEvent* event) {
  if (!enabled_)
    return;

  // Touch gestures should not initiate revealing the top-of-window views while
  // |widget_| is inactive.
  if (!widget_->IsActive())
    return;

  switch (event->type()) {
#if defined(OS_WIN)
    case ui::ET_GESTURE_WIN8_EDGE_SWIPE:
      UpdateRevealedLocksForSwipe(GetSwipeType(event));
      event->SetHandled();
      break;
#endif
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (ShouldHandleGestureEvent(GetEventLocationInScreen(*event))) {
        gesture_begun_ = true;
        // Do not consume the event. Otherwise, we end up consuming all
        // ui::ET_GESTURE_SCROLL_BEGIN events in the top-of-window views
        // when the top-of-window views are revealed.
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (gesture_begun_) {
        if (UpdateRevealedLocksForSwipe(GetSwipeType(event)))
          event->SetHandled();
        gesture_begun_ = false;
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      gesture_begun_ = false;
      break;
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::FocusChangeListener overrides:

void ImmersiveFullscreenController::OnWillChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
}

void ImmersiveFullscreenController::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  UpdateFocusRevealedLock();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver overrides:

void ImmersiveFullscreenController::OnWidgetDestroying(views::Widget* widget) {
  EnableWindowObservers(false);
  native_window_ = NULL;

  // Set |enabled_| to false such that any calls to MaybeStartReveal() and
  // MaybeEndReveal() have no effect.
  enabled_ = false;
}

void ImmersiveFullscreenController::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  UpdateFocusRevealedLock();
}

////////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void ImmersiveFullscreenController::AnimationEnded(
    const gfx::Animation* animation) {
  if (reveal_state_ == SLIDING_OPEN) {
    OnSlideOpenAnimationCompleted();
  } else if (reveal_state_ == SLIDING_CLOSED) {
    OnSlideClosedAnimationCompleted();
  }
}

void ImmersiveFullscreenController::AnimationProgressed(
    const gfx::Animation* animation) {
  delegate_->SetVisibleFraction(animation->GetCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ImmersiveFullscreenController::OnTransientChildAdded(
    aura::Window* window,
    aura::Window* transient) {
  views::BubbleDelegateView* bubble_delegate = AsBubbleDelegate(transient);
  if (bubble_delegate &&
      bubble_delegate->GetAnchorView() &&
      top_container_->Contains(bubble_delegate->GetAnchorView())) {
    // Observe the aura::Window because the BubbleDelegateView may not be
    // parented to the widget's root view yet so |bubble_delegate->GetWidget()|
    // may still return NULL.
    bubble_manager_->StartObserving(transient);
  }
}

void ImmersiveFullscreenController::OnTransientChildRemoved(
    aura::Window* window,
    aura::Window* transient) {
  bubble_manager_->StopObserving(transient);
}

////////////////////////////////////////////////////////////////////////////////
// ash::ImmersiveRevealedLock::Delegate overrides:

void ImmersiveFullscreenController::LockRevealedState(
    AnimateReveal animate_reveal) {
  ++revealed_lock_count_;
  Animate animate = (animate_reveal == ANIMATE_REVEAL_YES) ?
      ANIMATE_FAST : ANIMATE_NO;
  MaybeStartReveal(animate);
}

void ImmersiveFullscreenController::UnlockRevealedState() {
  --revealed_lock_count_;
  DCHECK_GE(revealed_lock_count_, 0);
  if (revealed_lock_count_ == 0) {
    // Always animate ending the reveal fast.
    MaybeEndReveal(ANIMATE_FAST);
  }
}

////////////////////////////////////////////////////////////////////////////////
// private:

void ImmersiveFullscreenController::EnableWindowObservers(bool enable) {
  if (observers_enabled_ == enable)
    return;
  observers_enabled_ = enable;

  views::FocusManager* focus_manager = widget_->GetFocusManager();

  if (enable) {
    widget_->AddObserver(this);
    focus_manager->AddFocusChangeListener(this);
    Shell::GetInstance()->AddPreTargetHandler(this);
    ::wm::TransientWindowManager::Get(native_window_)->
        AddObserver(this);

    RecreateBubbleManager();
  } else {
    widget_->RemoveObserver(this);
    focus_manager->RemoveFocusChangeListener(this);
    Shell::GetInstance()->RemovePreTargetHandler(this);
    ::wm::TransientWindowManager::Get(native_window_)->
        RemoveObserver(this);

    // We have stopped observing whether transient children are added or removed
    // to |native_window_|. The set of bubbles that BubbleManager is observing
    // will become stale really quickly. Destroy BubbleManager and recreate it
    // when we start observing |native_window_| again.
    bubble_manager_.reset();

    animation_->Stop();
  }
}

void ImmersiveFullscreenController::UpdateTopEdgeHoverTimer(
    ui::MouseEvent* event) {
  DCHECK(enabled_);
  DCHECK(reveal_state_ == SLIDING_CLOSED || reveal_state_ == CLOSED);

  // Check whether |native_window_| is the event target's parent window instead
  // of checking for activation. This allows the timer to be started when
  // |widget_| is inactive but prevents starting the timer if the mouse is over
  // a portion of the top edge obscured by an unrelated widget.
  if (!top_edge_hover_timer_.IsRunning() &&
      !native_window_->Contains(static_cast<aura::Window*>(event->target()))) {
    return;
  }

  // Mouse hover should not initiate revealing the top-of-window views while a
  // window has mouse capture.
  if (aura::client::GetCaptureWindow(native_window_))
    return;

  gfx::Point location_in_screen = GetEventLocationInScreen(*event);
  if (ShouldIgnoreMouseEventAtLocation(location_in_screen))
    return;

  // Stop the timer if the cursor left the top edge or is on a different
  // display.
  gfx::Rect hit_bounds_in_screen = GetDisplayBoundsInScreen(native_window_);
  hit_bounds_in_screen.set_height(kMouseRevealBoundsHeight);
  if (!hit_bounds_in_screen.Contains(location_in_screen)) {
    top_edge_hover_timer_.Stop();
    return;
  }

  // The cursor is now at the top of the screen. Consider the cursor "not
  // moving" even if it moves a little bit because users don't have perfect
  // pointing precision. (The y position is not tested because
  // |hit_bounds_in_screen| is short.)
  if (top_edge_hover_timer_.IsRunning() &&
      abs(location_in_screen.x() - mouse_x_when_hit_top_in_screen_) <=
          kMouseRevealXThresholdPixels)
    return;

  // Start the reveal if the cursor doesn't move for some amount of time.
  mouse_x_when_hit_top_in_screen_ = location_in_screen.x();
  top_edge_hover_timer_.Stop();
  // Timer is stopped when |this| is destroyed, hence Unretained() is safe.
  top_edge_hover_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMouseRevealDelayMs),
      base::Bind(
          &ImmersiveFullscreenController::AcquireLocatedEventRevealedLock,
          base::Unretained(this)));
}

void ImmersiveFullscreenController::UpdateLocatedEventRevealedLock(
    ui::LocatedEvent* event) {
  if (!enabled_)
    return;
  DCHECK(!event || event->IsMouseEvent() || event->IsTouchEvent());

  // Neither the mouse nor touch can initiate a reveal when the top-of-window
  // views are sliding closed or are closed with the following exceptions:
  // - Hovering at y = 0 which is handled in OnMouseEvent().
  // - Doing a SWIPE_OPEN edge gesture which is handled in OnGestureEvent().
  if (reveal_state_ == CLOSED || reveal_state_ == SLIDING_CLOSED)
    return;

  // For the sake of simplicity, ignore |widget_|'s activation in computing
  // whether the top-of-window views should stay revealed. Ideally, the
  // top-of-window views would stay revealed only when the mouse cursor is
  // hovered above a non-obscured portion of the top-of-window views. The
  // top-of-window views may be partially obscured when |widget_| is inactive.

  // Ignore all events while a window has capture. This keeps the top-of-window
  // views revealed during a drag.
  if (aura::client::GetCaptureWindow(native_window_))
    return;

  gfx::Point location_in_screen;
  if (event && event->type() != ui::ET_MOUSE_CAPTURE_CHANGED) {
    location_in_screen = GetEventLocationInScreen(*event);
  } else {
    aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
        native_window_->GetRootWindow());
    if (!cursor_client->IsMouseEventsEnabled()) {
      // If mouse events are disabled, the user's last interaction was probably
      // via touch. Do no do further processing in this case as there is no easy
      // way of retrieving the position of the user's last touch.
      return;
    }
    location_in_screen = aura::Env::GetInstance()->last_mouse_location();
  }

  if ((!event || event->IsMouseEvent()) &&
      ShouldIgnoreMouseEventAtLocation(location_in_screen)) {
    return;
  }

  // The visible bounds of |top_container_| should be contained in
  // |hit_bounds_in_screen|.
  std::vector<gfx::Rect> hit_bounds_in_screen =
      delegate_->GetVisibleBoundsInScreen();
  bool keep_revealed = false;
  for (size_t i = 0; i < hit_bounds_in_screen.size(); ++i) {
    // Allow the cursor to move slightly off the top-of-window views before
    // sliding closed. In the case of ImmersiveModeControllerAsh, this helps
    // when the user is attempting to click on the bookmark bar and overshoots
    // slightly.
    if (event && event->type() == ui::ET_MOUSE_MOVED) {
      const int kBoundsOffsetY = 8;
      hit_bounds_in_screen[i].Inset(0, 0, 0, -kBoundsOffsetY);
    }

    if (hit_bounds_in_screen[i].Contains(location_in_screen)) {
      keep_revealed = true;
      break;
    }
  }

  if (keep_revealed)
    AcquireLocatedEventRevealedLock();
  else
    located_event_revealed_lock_.reset();
}

void ImmersiveFullscreenController::AcquireLocatedEventRevealedLock() {
  // CAUTION: Acquiring the lock results in a reentrant call to
  // AcquireLocatedEventRevealedLock() when
  // |ImmersiveFullscreenController::animations_disabled_for_test_| is true.
  if (!located_event_revealed_lock_.get())
    located_event_revealed_lock_.reset(GetRevealedLock(ANIMATE_REVEAL_YES));
}

void ImmersiveFullscreenController::UpdateFocusRevealedLock() {
  if (!enabled_)
    return;

  bool hold_lock = false;
  if (widget_->IsActive()) {
    views::View* focused_view = widget_->GetFocusManager()->GetFocusedView();
    if (top_container_->Contains(focused_view))
      hold_lock = true;
  } else {
    aura::Window* active_window = aura::client::GetActivationClient(
        native_window_->GetRootWindow())->GetActiveWindow();
    views::BubbleDelegateView* bubble_delegate =
        AsBubbleDelegate(active_window);
    if (bubble_delegate && bubble_delegate->anchor_widget()) {
      // BubbleManager will already have locked the top-of-window views if the
      // bubble is anchored to a child of |top_container_|. Don't acquire
      // |focus_revealed_lock_| here for the sake of simplicity.
      // Note: Instead of checking for the existence of the |anchor_view|,
      // the existence of the |anchor_widget| is performed to avoid the case
      // where the view is already gone (and the widget is still running).
    } else {
      // The currently active window is not |native_window_| and it is not a
      // bubble with an anchor view. The top-of-window views should be revealed
      // if:
      // 1) The active window is a transient child of |native_window_|.
      // 2) The top-of-window views are already revealed. This restriction
      //    prevents a transient window opened by the web contents while the
      //    top-of-window views are hidden from from initiating a reveal.
      // The top-of-window views will stay revealed till |native_window_| is
      // reactivated.
      if (IsRevealed() &&
          IsWindowTransientChildOf(active_window, native_window_)) {
        hold_lock = true;
      }
    }
  }

  if (hold_lock) {
    if (!focus_revealed_lock_.get())
      focus_revealed_lock_.reset(GetRevealedLock(ANIMATE_REVEAL_YES));
  } else {
    focus_revealed_lock_.reset();
  }
}

bool ImmersiveFullscreenController::UpdateRevealedLocksForSwipe(
    SwipeType swipe_type) {
  if (!enabled_ || swipe_type == SWIPE_NONE)
    return false;

  // Swipes while |native_window_| is inactive should have been filtered out in
  // OnGestureEvent().
  DCHECK(widget_->IsActive());

  if (reveal_state_ == SLIDING_CLOSED || reveal_state_ == CLOSED) {
    if (swipe_type == SWIPE_OPEN && !located_event_revealed_lock_.get()) {
      located_event_revealed_lock_.reset(GetRevealedLock(ANIMATE_REVEAL_YES));
      return true;
    }
  } else {
    if (swipe_type == SWIPE_CLOSE) {
      // Attempt to end the reveal. If other code is holding onto a lock, the
      // attempt will be unsuccessful.
      located_event_revealed_lock_.reset();
      focus_revealed_lock_.reset();

      if (reveal_state_ == SLIDING_CLOSED || reveal_state_ == CLOSED) {
        widget_->GetFocusManager()->ClearFocus();
        return true;
      }

      // Ending the reveal was unsuccessful. Reaquire the locks if appropriate.
      UpdateLocatedEventRevealedLock(NULL);
      UpdateFocusRevealedLock();
    }
  }
  return false;
}

int ImmersiveFullscreenController::GetAnimationDuration(Animate animate) const {
  switch (animate) {
    case ANIMATE_NO:
      return 0;
    case ANIMATE_SLOW:
      return kRevealSlowAnimationDurationMs;
    case ANIMATE_FAST:
      return kRevealFastAnimationDurationMs;
  }
  NOTREACHED();
  return 0;
}

void ImmersiveFullscreenController::MaybeStartReveal(Animate animate) {
  if (!enabled_)
    return;

  if (animations_disabled_for_test_)
    animate = ANIMATE_NO;

  // Callers with ANIMATE_NO expect this function to synchronously reveal the
  // top-of-window views.
  if (reveal_state_ == REVEALED ||
      (reveal_state_ == SLIDING_OPEN && animate != ANIMATE_NO)) {
    return;
  }

  RevealState previous_reveal_state = reveal_state_;
  reveal_state_ = SLIDING_OPEN;
  if (previous_reveal_state == CLOSED) {
    delegate_->OnImmersiveRevealStarted();

    // Do not do any more processing if OnImmersiveRevealStarted() changed
    // |reveal_state_|.
    if (reveal_state_ != SLIDING_OPEN)
      return;
  }
  // Slide in the reveal view.
  if (animate == ANIMATE_NO) {
    animation_->Reset(1);
    OnSlideOpenAnimationCompleted();
  } else {
    animation_->SetSlideDuration(GetAnimationDuration(animate));
    animation_->Show();
  }
}

void ImmersiveFullscreenController::OnSlideOpenAnimationCompleted() {
  DCHECK_EQ(SLIDING_OPEN, reveal_state_);
  reveal_state_ = REVEALED;
  delegate_->SetVisibleFraction(1);

  // The user may not have moved the mouse since the reveal was initiated.
  // Update the revealed lock to reflect the mouse's current state.
  UpdateLocatedEventRevealedLock(NULL);
}

void ImmersiveFullscreenController::MaybeEndReveal(Animate animate) {
  if (!enabled_ || revealed_lock_count_ != 0)
    return;

  if (animations_disabled_for_test_)
    animate = ANIMATE_NO;

  // Callers with ANIMATE_NO expect this function to synchronously close the
  // top-of-window views.
  if (reveal_state_ == CLOSED ||
      (reveal_state_ == SLIDING_CLOSED && animate != ANIMATE_NO)) {
    return;
  }

  reveal_state_ = SLIDING_CLOSED;
  int duration_ms = GetAnimationDuration(animate);
  if (duration_ms > 0) {
    animation_->SetSlideDuration(duration_ms);
    animation_->Hide();
  } else {
    animation_->Reset(0);
    OnSlideClosedAnimationCompleted();
  }
}

void ImmersiveFullscreenController::OnSlideClosedAnimationCompleted() {
  DCHECK_EQ(SLIDING_CLOSED, reveal_state_);
  reveal_state_ = CLOSED;
  delegate_->OnImmersiveRevealEnded();
}

ImmersiveFullscreenController::SwipeType
ImmersiveFullscreenController::GetSwipeType(ui::GestureEvent* event) const {
#if defined(OS_WIN)
  if (event->type() == ui::ET_GESTURE_WIN8_EDGE_SWIPE)
    return SWIPE_OPEN;
#endif
  if (event->type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return SWIPE_NONE;
  // Make sure that it is a clear vertical gesture.
  if (std::abs(event->details().scroll_y()) <=
      kSwipeVerticalThresholdMultiplier * std::abs(event->details().scroll_x()))
    return SWIPE_NONE;
  if (event->details().scroll_y() < 0)
    return SWIPE_CLOSE;
  else if (event->details().scroll_y() > 0)
    return SWIPE_OPEN;
  return SWIPE_NONE;
}

bool ImmersiveFullscreenController::ShouldIgnoreMouseEventAtLocation(
    const gfx::Point& location) const {
  // Ignore mouse events in the region immediately above the top edge of the
  // display. This is to handle the case of a user with a vertical display
  // layout (primary display above/below secondary display) and the immersive
  // fullscreen window on the bottom display. It is really hard to trigger a
  // reveal in this case because:
  // - It is hard to stop the cursor in the top |kMouseRevealBoundsHeight|
  //   pixels of the bottom display.
  // - The cursor is warped to the top display if the cursor gets to the top
  //   edge of the bottom display.
  // Mouse events are ignored in the bottom few pixels of the top display
  // (Mouse events in this region cannot start or end a reveal). This allows a
  // user to overshoot the top of the bottom display and still reveal the
  // top-of-window views.
  gfx::Rect dead_region = GetDisplayBoundsInScreen(native_window_);
  dead_region.set_y(dead_region.y() - kHeightOfDeadRegionAboveTopContainer);
  dead_region.set_height(kHeightOfDeadRegionAboveTopContainer);
  return dead_region.Contains(location);
}

bool ImmersiveFullscreenController::ShouldHandleGestureEvent(
    const gfx::Point& location) const {
  DCHECK(widget_->IsActive());
  if (reveal_state_ == REVEALED) {
    std::vector<gfx::Rect> hit_bounds_in_screen(
        delegate_->GetVisibleBoundsInScreen());
    for (size_t i = 0; i < hit_bounds_in_screen.size(); ++i) {
      if (hit_bounds_in_screen[i].Contains(location))
        return true;
    }
    return false;
  }

  // When the top-of-window views are not fully revealed, handle gestures which
  // start in the top few pixels of the screen.
  gfx::Rect hit_bounds_in_screen(GetDisplayBoundsInScreen(native_window_));
  hit_bounds_in_screen.set_height(kImmersiveFullscreenTopEdgeInset);
  if (hit_bounds_in_screen.Contains(location))
    return true;

  // There may be a bezel sensor off screen logically above
  // |hit_bounds_in_screen|. The check for the event not contained by the
  // closest screen ensures that the event is from a valid bezel (as opposed to
  // another screen in an extended desktop).
  gfx::Rect screen_bounds =
      Shell::GetScreen()->GetDisplayNearestPoint(location).bounds();
  return (!screen_bounds.Contains(location) &&
          location.y() < hit_bounds_in_screen.y() &&
          location.x() >= hit_bounds_in_screen.x() &&
          location.x() < hit_bounds_in_screen.right());
}

void ImmersiveFullscreenController::RecreateBubbleManager() {
  bubble_manager_.reset(new BubbleManager(this));
  const std::vector<aura::Window*> transient_children =
      ::wm::GetTransientChildren(native_window_);
  for (size_t i = 0; i < transient_children.size(); ++i) {
    aura::Window* transient_child = transient_children[i];
    views::BubbleDelegateView* bubble_delegate =
        AsBubbleDelegate(transient_child);
    if (bubble_delegate &&
        bubble_delegate->GetAnchorView() &&
        top_container_->Contains(bubble_delegate->GetAnchorView())) {
      bubble_manager_->StartObserving(transient_child);
    }
  }
}

}  // namespace ash
