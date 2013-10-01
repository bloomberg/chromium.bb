// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include <set>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using views::View;

namespace {

// The slide open/closed animation looks better if it starts and ends just a
// few pixels before the view goes completely off the screen, which reduces
// the visual "pop" as the 2-pixel tall immersive-style tabs become visible.
const int kAnimationOffsetY = 3;

// Duration for the reveal show/hide slide animation. The slower duration is
// used for the initial slide out to give the user more change to see what
// happened.
const int kRevealSlowAnimationDurationMs = 400;
const int kRevealFastAnimationDurationMs = 200;

// How many pixels a gesture can start away from |top_container_| when in
// closed state and still be considered near it. This is needed to overcome
// issues with poor location values near the edge of the display.
const int kNearTopContainerDistance = 8;

// Used to multiply x value of an update in check to determine if gesture is
// vertical. This is used to make sure that gesture is close to vertical instead
// of just more vertical then horizontal.
const int kSwipeVerticalThresholdMultiplier = 3;

// The height in pixels of the region above the top edge of the display which
// hosts the immersive fullscreen window in which mouse events are ignored
// (cannot reveal or unreveal the top-of-window views).
// See ShouldIgnoreMouseEventAtLocation() for more details.
const int kHeightOfDeadRegionAboveTopContainer = 10;

// The height in pixels of the region below the top edge of the display in which
// the mouse can trigger revealing the top-of-window views. The height must be
// greater than 1px because the top pixel is used to trigger moving the cursor
// between displays if the user has a vertical display layout (primary display
// above/below secondary display).
const int kMouseRevealBoundsHeight = 3;

// If |hovered| is true, moves the mouse above |view|. Moves it outside of
// |view| otherwise.
// Should not be called outside of tests.
void MoveMouse(views::View* view, bool hovered) {
  gfx::Point cursor_pos;
  if (!hovered) {
    int bottom_edge = view->bounds().bottom();
    cursor_pos = gfx::Point(0, bottom_edge + 100);
  }
  views::View::ConvertPointToScreen(view, &cursor_pos);
  aura::Env::GetInstance()->set_last_mouse_location(cursor_pos);
}

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
       window = window->transient_parent()) {
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

////////////////////////////////////////////////////////////////////////////////

class RevealedLockAsh : public ImmersiveRevealedLock {
 public:
  RevealedLockAsh(const base::WeakPtr<ImmersiveModeControllerAsh>& controller,
                  ImmersiveModeController::AnimateReveal animate_reveal)
      : controller_(controller) {
    DCHECK(controller_);
    controller_->LockRevealedState(animate_reveal);
  }

  virtual ~RevealedLockAsh() {
    if (controller_)
      controller_->UnlockRevealedState();
  }

 private:
  base::WeakPtr<ImmersiveModeControllerAsh> controller_;

  DISALLOW_COPY_AND_ASSIGN(RevealedLockAsh);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// Class which keeps the top-of-window views revealed as long as one of the
// bubbles it is observing is visible. The logic to keep the top-of-window
// views revealed based on the visibility of bubbles anchored to
// children of |ImmersiveModeController::top_container_| is separate from
// the logic related to |ImmersiveModeControllerAsh::focus_revealed_lock_|
// so that bubbles which are not activatable and bubbles which do not close
// upon deactivation also keep the top-of-window views revealed for the
// duration of their visibility.
class ImmersiveModeControllerAsh::BubbleManager : public aura::WindowObserver {
 public:
  explicit BubbleManager(ImmersiveModeControllerAsh* controller);
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

  ImmersiveModeControllerAsh* controller_;

  std::set<aura::Window*> bubbles_;

  // Lock which keeps the top-of-window views revealed based on whether any of
  // |bubbles_| is visible.
  scoped_ptr<ImmersiveRevealedLock> revealed_lock_;

  DISALLOW_COPY_AND_ASSIGN(BubbleManager);
};

ImmersiveModeControllerAsh::BubbleManager::BubbleManager(
    ImmersiveModeControllerAsh* controller)
    : controller_(controller) {
}

ImmersiveModeControllerAsh::BubbleManager::~BubbleManager() {
  for (std::set<aura::Window*>::const_iterator it = bubbles_.begin();
       it != bubbles_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
}

void ImmersiveModeControllerAsh::BubbleManager::StartObserving(
    aura::Window* bubble) {
  if (bubbles_.insert(bubble).second) {
    bubble->AddObserver(this);
    UpdateRevealedLock();
  }
}

void ImmersiveModeControllerAsh::BubbleManager::StopObserving(
    aura::Window* bubble) {
  if (bubbles_.erase(bubble)) {
    bubble->RemoveObserver(this);
    UpdateRevealedLock();
  }
}

void ImmersiveModeControllerAsh::BubbleManager::UpdateRevealedLock() {
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
          ImmersiveModeController::ANIMATE_REVEAL_NO));
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
      AsBubbleDelegate(*it)->OnAnchorViewBoundsChanged();
    }
  }
}

void ImmersiveModeControllerAsh::BubbleManager::OnWindowVisibilityChanged(
    aura::Window*,
    bool visible) {
  UpdateRevealedLock();
}

void ImmersiveModeControllerAsh::BubbleManager::OnWindowDestroying(
    aura::Window* window) {
  StopObserving(window);
}

////////////////////////////////////////////////////////////////////////////////

ImmersiveModeControllerAsh::ImmersiveModeControllerAsh()
    : delegate_(NULL),
      widget_(NULL),
      top_container_(NULL),
      observers_enabled_(false),
      enabled_(false),
      reveal_state_(CLOSED),
      revealed_lock_count_(0),
      tab_indicator_visibility_(TAB_INDICATORS_HIDE),
      mouse_x_when_hit_top_in_screen_(-1),
      gesture_begun_(false),
      native_window_(NULL),
      animation_(new gfx::SlideAnimation(this)),
      animations_disabled_for_test_(false),
      weak_ptr_factory_(this) {
}

ImmersiveModeControllerAsh::~ImmersiveModeControllerAsh() {
  // The browser view is being destroyed so there's no need to update its
  // layout or layers, even if the top views are revealed. But the window
  // observers still need to be removed.
  EnableWindowObservers(false);
}

void ImmersiveModeControllerAsh::LockRevealedState(
      AnimateReveal animate_reveal) {
  ++revealed_lock_count_;
  Animate animate = (animate_reveal == ANIMATE_REVEAL_YES) ?
      ANIMATE_FAST : ANIMATE_NO;
  MaybeStartReveal(animate);
}

void ImmersiveModeControllerAsh::UnlockRevealedState() {
  --revealed_lock_count_;
  DCHECK_GE(revealed_lock_count_, 0);
  if (revealed_lock_count_ == 0) {
    // Always animate ending the reveal fast.
    MaybeEndReveal(ANIMATE_FAST);
  }
}

void ImmersiveModeControllerAsh::MaybeExitImmersiveFullscreen() {
  if (ShouldExitImmersiveFullscreen())
    delegate_->FullscreenStateChanged();
}

void ImmersiveModeControllerAsh::Init(
    Delegate* delegate,
    views::Widget* widget,
    views::View* top_container) {
  delegate_ = delegate;
  widget_ = widget;
  // Browser view is detached from its widget during destruction. Cache the
  // window pointer so |this| can stop observing during destruction.
  native_window_ = widget_->GetNativeWindow();
  top_container_ = top_container;

  // Optionally allow the tab indicators to be hidden.
  if (CommandLine::ForCurrentProcess()->
          HasSwitch(ash::switches::kAshImmersiveHideTabIndicators)) {
    tab_indicator_visibility_ = TAB_INDICATORS_FORCE_HIDE;
  }
}

void ImmersiveModeControllerAsh::SetEnabled(bool enabled) {
  DCHECK(native_window_) << "Must initialize before enabling";
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  EnableWindowObservers(enabled_);

  UpdateUseMinimalChrome(LAYOUT_NO);

  if (enabled_) {
    // Animate enabling immersive mode by sliding out the top-of-window views.
    // No animation occurs if a lock is holding the top-of-window views open.

    // Do a reveal to set the initial state for the animation. (And any
    // required state in case the animation cannot run because of a lock holding
    // the top-of-window views open.) This call has the side effect of relaying
    // out |browser_view_|'s root view.
    MaybeStartReveal(ANIMATE_NO);

    // Reset the located event and the focus revealed locks so that they do not
    // affect whether the top-of-window views are hidden.
    located_event_revealed_lock_.reset();
    focus_revealed_lock_.reset();

    // Try doing the animation.
    MaybeEndReveal(ANIMATE_SLOW);

    if (reveal_state_ == REVEALED) {
      // Reveal was unsuccessful. Reacquire the revealed locks if appropriate.
      UpdateLocatedEventRevealedLock(NULL, ALLOW_REVEAL_WHILE_CLOSING_NO);
      UpdateFocusRevealedLock();
    }
  } else {
    // Stop cursor-at-top tracking.
    top_edge_hover_timer_.Stop();
    // Snap immediately to the closed state.
    reveal_state_ = CLOSED;
    EnablePaintToLayer(false);
    delegate_->SetImmersiveStyle(false);
    SetRenderWindowTopInsetsForTouch(0);

    // Layout the root view so that incognito avatar icon, if any, gets laid
    // out.
    LayoutBrowserRootView();
  }
}

bool ImmersiveModeControllerAsh::IsEnabled() const {
  return enabled_;
}

bool ImmersiveModeControllerAsh::ShouldHideTabIndicators() const {
  return tab_indicator_visibility_ != TAB_INDICATORS_SHOW;
}

bool ImmersiveModeControllerAsh::ShouldHideTopViews() const {
  return enabled_ && reveal_state_ == CLOSED;
}

bool ImmersiveModeControllerAsh::IsRevealed() const {
  return enabled_ && reveal_state_ != CLOSED;
}

int ImmersiveModeControllerAsh::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  if (!enabled_ || reveal_state_ == REVEALED || reveal_state_ == CLOSED)
    return 0;

  return animation_->CurrentValueBetween(
      -top_container_size.height() + kAnimationOffsetY, 0);
}

ImmersiveRevealedLock* ImmersiveModeControllerAsh::GetRevealedLock(
    AnimateReveal animate_reveal) {
  return new RevealedLockAsh(weak_ptr_factory_.GetWeakPtr(), animate_reveal);
}

void ImmersiveModeControllerAsh::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {
  find_bar_visible_bounds_in_screen_ = new_visible_bounds_in_screen;
}

////////////////////////////////////////////////////////////////////////////////
// Observers:

void ImmersiveModeControllerAsh::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  if (enabled_)
    UpdateUseMinimalChrome(LAYOUT_YES);
}

void ImmersiveModeControllerAsh::OnMouseEvent(ui::MouseEvent* event) {
  if (!enabled_)
    return;

  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_PRESSED &&
      event->type() != ui::ET_MOUSE_RELEASED &&
      event->type() != ui::ET_MOUSE_CAPTURE_CHANGED) {
    return;
  }

  // Mouse hover should not initiate revealing the top-of-window views while
  // |native_window_| is inactive.
  if (!views::Widget::GetWidgetForNativeWindow(native_window_)->IsActive())
    return;

  // Mouse hover should not initiate revealing the top-of-window views while
  // a window has mouse capture.
  if (aura::client::GetCaptureWindow(native_window_))
    return;

  if (IsRevealed())
    UpdateLocatedEventRevealedLock(event, ALLOW_REVEAL_WHILE_CLOSING_NO);

  // Trigger a reveal if the cursor pauses at the top of the screen for a
  // while.
  if (event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)
    UpdateTopEdgeHoverTimer(event);
}

void ImmersiveModeControllerAsh::OnTouchEvent(ui::TouchEvent* event) {
  if (!enabled_ || event->type() != ui::ET_TOUCH_PRESSED)
    return;

  UpdateLocatedEventRevealedLock(event, ALLOW_REVEAL_WHILE_CLOSING_NO);
}

void ImmersiveModeControllerAsh::OnGestureEvent(ui::GestureEvent* event) {
  if (!enabled_)
    return;

  // Touch gestures should not initiate revealing the top-of-window views while
  // |native_window_| is inactive.
  if (!views::Widget::GetWidgetForNativeWindow(native_window_)->IsActive())
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (ShouldHandleGestureEvent(GetEventLocationInScreen(*event))) {
        gesture_begun_ = true;
        event->SetHandled();
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

void ImmersiveModeControllerAsh::OnWillChangeFocus(views::View* focused_before,
                                                   views::View* focused_now) {
}

void ImmersiveModeControllerAsh::OnDidChangeFocus(views::View* focused_before,
                                                  views::View* focused_now) {
  UpdateFocusRevealedLock();
}

void ImmersiveModeControllerAsh::OnWidgetDestroying(views::Widget* widget) {
  EnableWindowObservers(false);
  native_window_ = NULL;

  // Set |enabled_| to false such that any calls to MaybeStartReveal() and
  // MaybeEndReveal() have no effect.
  enabled_ = false;
}

void ImmersiveModeControllerAsh::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  // Mouse hover should not initiate revealing the top-of-window views while
  // |native_window_| is inactive.
  top_edge_hover_timer_.Stop();

  UpdateFocusRevealedLock();

  // Allow the top-of-window views to stay revealed if all of the revealed locks
  // were released in the process of activating |widget| but the mouse is still
  // hovered above the top-of-window views. For instance, if the bubble which
  // has been keeping the top-of-window views revealed is hidden but the mouse
  // is hovered above the top-of-window views, the top-of-window views should
  // stay revealed. We cannot call UpdateLocatedEventRevealedLock() from
  // BubbleManager::UpdateRevealedLock() because |widget| is not yet active
  // at that time.
  UpdateLocatedEventRevealedLock(NULL, ALLOW_REVEAL_WHILE_CLOSING_YES);
}

////////////////////////////////////////////////////////////////////////////////
// Animation delegate:

void ImmersiveModeControllerAsh::AnimationEnded(
    const gfx::Animation* animation) {
  if (reveal_state_ == SLIDING_OPEN) {
    // AnimationProgressed() is called immediately before AnimationEnded()
    // and does a layout.
    OnSlideOpenAnimationCompleted(LAYOUT_NO);
  } else if (reveal_state_ == SLIDING_CLOSED) {
    OnSlideClosedAnimationCompleted();
  }
}

void ImmersiveModeControllerAsh::AnimationProgressed(
    const gfx::Animation* animation) {
  // Relayout. This will also move any views whose position depends on the
  // top container position such as the find bar.
  // We do not call LayoutBrowserRootView() here because we are not toggling
  // the tab strip's immersive style so relaying out the non client view is not
  // necessary.
  top_container_->parent()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:
void ImmersiveModeControllerAsh::OnWindowPropertyChanged(aura::Window* window,
                                                         const void* key,
                                                         intptr_t old) {
  if (!enabled_)
    return;

  if (key == aura::client::kShowStateKey) {
    // Disable immersive mode when the user exits fullscreen without going
    // through FullscreenController::ToggleFullscreenMode(). This is the case
    // if the user exits fullscreen via the restore button.
    if (ShouldExitImmersiveFullscreen()) {
      // Other "property change" observers may want to animate between the
      // current visuals and the new window state. Do not alter the current
      // visuals yet and post a task to exit immersive fullscreen instead.
      base::MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          base::Bind(&ImmersiveModeControllerAsh::MaybeExitImmersiveFullscreen,
                     weak_ptr_factory_.GetWeakPtr()));
    }

    ui::WindowShowState show_state = native_window_->GetProperty(
        aura::client::kShowStateKey);
    if (show_state == ui::SHOW_STATE_FULLSCREEN &&
        old == ui::SHOW_STATE_MINIMIZED) {
      // Relayout in case there was a layout while the window show state was
      // ui::SHOW_STATE_MINIMIZED.
      LayoutBrowserRootView();
    }
  }
}

void ImmersiveModeControllerAsh::OnAddTransientChild(aura::Window* window,
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

void ImmersiveModeControllerAsh::OnRemoveTransientChild(
    aura::Window* window,
    aura::Window* transient) {
  bubble_manager_->StopObserving(transient);
}

////////////////////////////////////////////////////////////////////////////////
// Testing interface:

void ImmersiveModeControllerAsh::SetForceHideTabIndicatorsForTest(bool force) {
  if (force)
    tab_indicator_visibility_ = TAB_INDICATORS_FORCE_HIDE;
  else if (tab_indicator_visibility_ == TAB_INDICATORS_FORCE_HIDE)
    tab_indicator_visibility_ = TAB_INDICATORS_HIDE;
  UpdateUseMinimalChrome(LAYOUT_YES);
}

void ImmersiveModeControllerAsh::StartRevealForTest(bool hovered) {
  MaybeStartReveal(ANIMATE_NO);
  MoveMouse(top_container_, hovered);
  UpdateLocatedEventRevealedLock(NULL, ALLOW_REVEAL_WHILE_CLOSING_NO);
}

void ImmersiveModeControllerAsh::SetMouseHoveredForTest(bool hovered) {
  MoveMouse(top_container_, hovered);
  UpdateLocatedEventRevealedLock(NULL, ALLOW_REVEAL_WHILE_CLOSING_NO);
}

void ImmersiveModeControllerAsh::DisableAnimationsForTest() {
  animations_disabled_for_test_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// private:

void ImmersiveModeControllerAsh::EnableWindowObservers(bool enable) {
  if (observers_enabled_ == enable)
    return;
  observers_enabled_ = enable;

  if (!native_window_) {
    NOTREACHED() << "ImmersiveModeControllerAsh not initialized";
    return;
  }

  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window_);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  if (enable) {
    widget->AddObserver(this);
    focus_manager->AddFocusChangeListener(this);
  } else {
    widget->RemoveObserver(this);
    focus_manager->RemoveFocusChangeListener(this);
  }

  if (enable)
    ash::Shell::GetInstance()->AddPreTargetHandler(this);
  else
    ash::Shell::GetInstance()->RemovePreTargetHandler(this);

  if (enable) {
    native_window_->AddObserver(this);
  } else {
    native_window_->RemoveObserver(this);
  }

  if (enable) {
    RecreateBubbleManager();
  } else {
    // We have stopped observing whether transient children are added or removed
    // to |native_window_|. The set of bubbles that BubbleManager is observing
    // will become stale really quickly. Destroy BubbleManager and recreate it
    // when we start observing |native_window_| again.
    bubble_manager_.reset();
  }

  if (enable) {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::Source<FullscreenController>(
            delegate_->GetFullscreenController()));
  } else {
    registrar_.Remove(
        this,
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::Source<FullscreenController>(
            delegate_->GetFullscreenController()));
  }

  if (!enable)
    animation_->Stop();
}

void ImmersiveModeControllerAsh::UpdateTopEdgeHoverTimer(
    ui::MouseEvent* event) {
  DCHECK(enabled_);
  // Stop the timer if the top-of-window views are already revealed.
  if (reveal_state_ == SLIDING_OPEN || reveal_state_ == REVEALED) {
    top_edge_hover_timer_.Stop();
    return;
  }

  gfx::Point location_in_screen = GetEventLocationInScreen(*event);
  if (ShouldIgnoreMouseEventAtLocation(location_in_screen))
    return;

  // Stop the timer if the cursor left the top edge or is on a different
  // display. The bounds of |top_container_|'s parent are used to infer the hit
  // bounds because |top_container_| will be partially offscreen if it is
  // animating closed.
  gfx::Rect hit_bounds_in_screen =
      top_container_->parent()->GetBoundsInScreen();
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
          ImmersiveFullscreenConfiguration::
              immersive_mode_reveal_x_threshold_pixels())
    return;

  // Start the reveal if the cursor doesn't move for some amount of time.
  mouse_x_when_hit_top_in_screen_ = location_in_screen.x();
  top_edge_hover_timer_.Stop();
  // Timer is stopped when |this| is destroyed, hence Unretained() is safe.
  top_edge_hover_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          ImmersiveFullscreenConfiguration::immersive_mode_reveal_delay_ms()),
      base::Bind(&ImmersiveModeControllerAsh::AcquireLocatedEventRevealedLock,
                 base::Unretained(this)));
}

void ImmersiveModeControllerAsh::UpdateLocatedEventRevealedLock(
    ui::LocatedEvent* event,
    AllowRevealWhileClosing allow_reveal_while_closing) {
  if (!enabled_)
    return;
  DCHECK(!event || event->IsMouseEvent() || event->IsTouchEvent());

  // Neither the mouse nor touch can initiate a reveal when the top-of-window
  // views are sliding closed or are closed with the following exceptions:
  // - Hovering at y = 0 which is handled in OnMouseEvent().
  // - Doing a SWIPE_OPEN edge gesture which is handled in OnGestureEvent().
  if (reveal_state_ == CLOSED ||
      (reveal_state_ == SLIDING_CLOSED &&
       allow_reveal_while_closing == ALLOW_REVEAL_WHILE_CLOSING_NO)) {
    return;
  }

  // Neither the mouse nor touch should keep the top-of-window views revealed if
  // |native_window_| is not active.
  if (!views::Widget::GetWidgetForNativeWindow(native_window_)->IsActive()) {
    located_event_revealed_lock_.reset();
    return;
  }

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

  gfx::Rect hit_bounds_in_top_container = top_container_->GetVisibleBounds();
  // TODO(tdanderson): Implement View::ConvertRectToScreen();
  gfx::Point hit_bounds_in_screen_origin = hit_bounds_in_top_container.origin();
  views::View::ConvertPointToScreen(top_container_,
      &hit_bounds_in_screen_origin);
  gfx::Rect hit_bounds_in_screen(hit_bounds_in_screen_origin,
      hit_bounds_in_top_container.size());

  gfx::Rect find_bar_hit_bounds_in_screen = find_bar_visible_bounds_in_screen_;

  // Allow the cursor to move slightly off the top-of-window views before
  // sliding closed. This helps when the user is attempting to click on the
  // bookmark bar and overshoots slightly.
  if (event && event->type() == ui::ET_MOUSE_MOVED) {
    const int kBoundsOffsetY = 8;
    hit_bounds_in_screen.Inset(0, 0, 0, -kBoundsOffsetY);
    find_bar_hit_bounds_in_screen.Inset(0, 0, 0, -kBoundsOffsetY);
  }

  if (hit_bounds_in_screen.Contains(location_in_screen) ||
      find_bar_hit_bounds_in_screen.Contains(location_in_screen)) {
    AcquireLocatedEventRevealedLock();
  } else {
    located_event_revealed_lock_.reset();
  }
}

void ImmersiveModeControllerAsh::AcquireLocatedEventRevealedLock() {
  // CAUTION: Acquiring the lock results in a reentrant call to
  // AcquireLocatedEventRevealedLock() when
  // |ImmersiveModeControllerAsh::animations_disabled_for_test_| is true.
  if (!located_event_revealed_lock_.get())
    located_event_revealed_lock_.reset(GetRevealedLock(ANIMATE_REVEAL_YES));
}

void ImmersiveModeControllerAsh::UpdateFocusRevealedLock() {
  if (!enabled_)
    return;

  bool hold_lock = false;
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window_);
  if (widget->IsActive()) {
    views::View* focused_view = widget->GetFocusManager()->GetFocusedView();
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

bool ImmersiveModeControllerAsh::UpdateRevealedLocksForSwipe(
    SwipeType swipe_type) {
  if (!enabled_ || swipe_type == SWIPE_NONE)
    return false;

  // Swipes while |native_window_| is inactive should have been filtered out in
  // OnGestureEvent().
  DCHECK(views::Widget::GetWidgetForNativeWindow(native_window_)->IsActive());

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

      if (reveal_state_ == SLIDING_CLOSED || reveal_state_ == CLOSED)
        return true;

      // Ending the reveal was unsuccessful. Reaquire the locks if appropriate.
      UpdateLocatedEventRevealedLock(NULL, ALLOW_REVEAL_WHILE_CLOSING_NO);
      UpdateFocusRevealedLock();
    }
  }
  return false;
}

void ImmersiveModeControllerAsh::UpdateUseMinimalChrome(Layout layout) {
  // May be NULL in tests.
  FullscreenController* fullscreen_controller =
      delegate_->GetFullscreenController();
  bool in_tab_fullscreen = fullscreen_controller ?
      fullscreen_controller->IsFullscreenForTabOrPending() : false;
  bool use_minimal_chrome = !in_tab_fullscreen && enabled_;
  native_window_->SetProperty(ash::internal::kFullscreenUsesMinimalChromeKey,
                              use_minimal_chrome);

  TabIndicatorVisibility previous_tab_indicator_visibility =
      tab_indicator_visibility_;
  if (tab_indicator_visibility_ != TAB_INDICATORS_FORCE_HIDE) {
    tab_indicator_visibility_ = use_minimal_chrome ?
        TAB_INDICATORS_SHOW : TAB_INDICATORS_HIDE;
  }

  // Ash on Windows may not have a shell.
  if (ash::Shell::HasInstance()) {
    // When using minimal chrome, the shelf is auto-hidden. The auto-hidden
    // shelf displays a 3px 'light bar' when it is closed.
    ash::Shell::GetInstance()->UpdateShelfVisibility();
  }

  if (tab_indicator_visibility_ != previous_tab_indicator_visibility) {
    // If the top-of-window views are revealed or animating, the change will
    // take effect with the layout once the top-of-window views are closed.
    if (layout == LAYOUT_YES && reveal_state_ == CLOSED)
      LayoutBrowserRootView();
  }
}

int ImmersiveModeControllerAsh::GetAnimationDuration(Animate animate) const {
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

void ImmersiveModeControllerAsh::MaybeStartReveal(Animate animate) {
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
    // Turn on layer painting so that we can overlap the web contents.
    EnablePaintToLayer(true);

    // Ensure window caption buttons are updated and the view bounds are
    // computed at normal (non-immersive-style) size. The layout call moves the
    // top-of-window views to their initial offscreen position for the
    // animation.
    delegate_->SetImmersiveStyle(false);
    SetRenderWindowTopInsetsForTouch(0);
    LayoutBrowserRootView();

    // Do not do any more processing if LayoutBrowserView() changed
    // |reveal_state_|.
    if (reveal_state_ != SLIDING_OPEN) {
      if (reveal_state_ == REVEALED)
        FOR_EACH_OBSERVER(Observer, observers_, OnImmersiveRevealStarted());
      return;
    }
  }
  // Slide in the reveal view.
  if (animate == ANIMATE_NO) {
    animation_->Reset(1);
    OnSlideOpenAnimationCompleted(LAYOUT_YES);
  } else {
    animation_->SetSlideDuration(GetAnimationDuration(animate));
    animation_->Show();
  }

  if (previous_reveal_state == CLOSED)
     FOR_EACH_OBSERVER(Observer, observers_, OnImmersiveRevealStarted());
}

void ImmersiveModeControllerAsh::EnablePaintToLayer(bool enable) {
  top_container_->SetPaintToLayer(enable);

  // Views software compositing is not fully layer aware. If the bookmark bar
  // is detached while the top container layer slides on or off the screen,
  // the pixels that become exposed are the remnants of the last software
  // composite of the BrowserView, not the freshly-exposed bookmark bar.
  // Force the bookmark bar to paint to a layer so the views composite
  // properly. The infobar container does not need this treatment because
  // BrowserView::PaintChildren() always draws it last when it is visible.
  BookmarkBarView* bookmark_bar = delegate_->GetBookmarkBar();
  if (!bookmark_bar)
    return;
  if (enable && bookmark_bar->IsDetached())
    bookmark_bar->SetPaintToLayer(true);
  else
    bookmark_bar->SetPaintToLayer(false);
}

void ImmersiveModeControllerAsh::LayoutBrowserRootView() {
  // Update the window caption buttons.
  widget_->non_client_view()->frame_view()->ResetWindowControls();
  // Layout all views, including BrowserView.
  widget_->GetRootView()->Layout();
}

void ImmersiveModeControllerAsh::OnSlideOpenAnimationCompleted(Layout layout) {
  DCHECK_EQ(SLIDING_OPEN, reveal_state_);
  reveal_state_ = REVEALED;

  if (layout == LAYOUT_YES)
    top_container_->parent()->Layout();

  // The user may not have moved the mouse since the reveal was initiated.
  // Update the revealed lock to reflect the mouse's current state.
  UpdateLocatedEventRevealedLock(NULL, ALLOW_REVEAL_WHILE_CLOSING_NO);
}

void ImmersiveModeControllerAsh::MaybeEndReveal(Animate animate) {
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
    // The bookmark bar may have become detached during the reveal so ensure
    // layers are available. This is a no-op for the top container.
    EnablePaintToLayer(true);

    animation_->SetSlideDuration(duration_ms);
    animation_->Hide();
  } else {
    animation_->Reset(0);
    OnSlideClosedAnimationCompleted();
  }
}

void ImmersiveModeControllerAsh::OnSlideClosedAnimationCompleted() {
  DCHECK_EQ(SLIDING_CLOSED, reveal_state_);
  reveal_state_ = CLOSED;
  // Layers aren't needed after animation completes.
  EnablePaintToLayer(false);
  // Update tabstrip for closed state.
  delegate_->SetImmersiveStyle(true);
  SetRenderWindowTopInsetsForTouch(kNearTopContainerDistance);
  LayoutBrowserRootView();
}

bool ImmersiveModeControllerAsh::ShouldExitImmersiveFullscreen() const {
  if (!native_window_)
    return false;

  ui::WindowShowState show_state = static_cast<ui::WindowShowState>(
      native_window_->GetProperty(aura::client::kShowStateKey));
  return IsEnabled() &&
      show_state != ui::SHOW_STATE_FULLSCREEN &&
      show_state != ui::SHOW_STATE_MINIMIZED;
}

ImmersiveModeControllerAsh::SwipeType ImmersiveModeControllerAsh::GetSwipeType(
    ui::GestureEvent* event) const {
  if (event->type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return SWIPE_NONE;
  // Make sure that it is a clear vertical gesture.
  if (abs(event->details().scroll_y()) <=
      kSwipeVerticalThresholdMultiplier * abs(event->details().scroll_x()))
    return SWIPE_NONE;
  if (event->details().scroll_y() < 0)
    return SWIPE_CLOSE;
  else if (event->details().scroll_y() > 0)
    return SWIPE_OPEN;
  return SWIPE_NONE;
}

bool ImmersiveModeControllerAsh::ShouldIgnoreMouseEventAtLocation(
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
  gfx::Rect dead_region = top_container_->parent()->GetBoundsInScreen();
  dead_region.set_y(dead_region.y() - kHeightOfDeadRegionAboveTopContainer);
  dead_region.set_height(kHeightOfDeadRegionAboveTopContainer);
  return dead_region.Contains(location);
}

bool ImmersiveModeControllerAsh::ShouldHandleGestureEvent(
    const gfx::Point& location) const {
  gfx::Rect top_container_bounds_in_screen =
      top_container_->GetBoundsInScreen();

  // All of the gestures that are of interest start in a region with left &
  // right edges agreeing with |top_container_|. When CLOSED it is difficult to
  // hit the bounds due to small size of the tab strip, so the hit target needs
  // to be extended on the bottom, thus the inset call.
  gfx::Rect near_bounds = top_container_bounds_in_screen;
  if (reveal_state_ == CLOSED)
    near_bounds.Inset(gfx::Insets(0, 0, -kNearTopContainerDistance, 0));
  if (near_bounds.Contains(location))
    return true;

  // There may be a bezel sensor off screen logically above |top_container_|
  // thus the test needs to include gestures starting above, but this needs to
  // be distinguished from events originating on another screen from
  // (potentially) an extended desktop. The check for the event not contained by
  // the closest screen ensures that the event is from a valid bezel and can be
  // interpreted as such.
  gfx::Rect screen_bounds =
      ash::Shell::GetScreen()->GetDisplayNearestPoint(location).bounds();
  return (!screen_bounds.Contains(location) &&
          location.y() < top_container_bounds_in_screen.y() &&
          location.x() >= top_container_bounds_in_screen.x() &&
          location.x() < top_container_bounds_in_screen.right());
}

void ImmersiveModeControllerAsh::SetRenderWindowTopInsetsForTouch(
    int top_inset) {
  content::WebContents* contents = delegate_->GetWebContents();
  if (contents) {
    aura::Window* window = contents->GetView()->GetContentNativeView();
    // |window| is NULL if the renderer crashed.
    if (window) {
      gfx::Insets inset(top_inset, 0, 0, 0);
      window->SetHitTestBoundsOverrideOuter(
          window->hit_test_bounds_override_outer_mouse(),
          inset);
    }
  }
}

void ImmersiveModeControllerAsh::RecreateBubbleManager() {
  bubble_manager_.reset(new BubbleManager(this));
  const std::vector<aura::Window*> transient_children =
      native_window_->transient_children();
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
