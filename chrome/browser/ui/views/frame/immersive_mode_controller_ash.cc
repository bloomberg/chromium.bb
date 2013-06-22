// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/command_line.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/gfx/transform.h"
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
const int kNearTopContainerDistance = 5;

// Used to multiply x value of an update in check to determine if gesture is
// vertical. This is used to make sure that gesture is close to vertical instead
// of just more vertical then horizontal.
const int kSwipeVerticalThresholdMultiplier = 3;

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

// Returns true if the currently active window is a transient child of
// |toplevel|.
bool IsActiveWindowTransientChildOf(aura::Window* toplevel) {
  if (!toplevel)
    return false;

  aura::Window* active_window = aura::client::GetActivationClient(
      toplevel->GetRootWindow())->GetActiveWindow();

  if (!active_window)
    return false;

  for (aura::Window* window = active_window; window;
       window = window->transient_parent()) {
    if (window == toplevel)
      return true;
  }
  return false;
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

// Manages widgets which should move in sync with the top-of-window views.
class ImmersiveModeControllerAsh::AnchoredWidgetManager
    : public views::WidgetObserver {
 public:
  explicit AnchoredWidgetManager(ImmersiveModeControllerAsh* controller);
  virtual ~AnchoredWidgetManager();

  // Anchors |widget| such that it stays |y_offset| below the top-of-window
  // views. |widget| will be repositioned whenever the top-of-window views are
  // animated (top-of-window views revealing / unrevealing) or the top-of-window
  // bounds change (eg the bookmark bar is shown).
  // If the top-of-window views are revealed (or become revealed), |widget| will
  // keep the top-of-window views revealed till |widget| is hidden or
  // RemoveAnchoredWidget() is called.
  void AddAnchoredWidget(views::Widget* widget, int y_offset);

  // Stops managing |widget|'s y position.
  // Closes the top-of-window views if no locks or other anchored widgets are
  // keeping the top-of-window views revealed.
  void RemoveAnchoredWidget(views::Widget* widget);

  // Repositions the anchored widgets for the current top container bounds if
  // immersive mode is enabled.
  void MaybeRepositionAnchoredWidgets();

  // Called when immersive mode has been enabled.
  void OnImmersiveModeEnabled();

 private:
  // Updates |revealed_lock_| based on the visible anchored widgets.
  void UpdateRevealedLock();

  // Updates the y position of |widget| given |y_offset| and the top
  // container's target bounds.
  void UpdateWidgetBounds(views::Widget* widget, int y_offset);

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;

  ImmersiveModeControllerAsh* controller_;

  // Mapping of anchored widgets to the y offset below the top-of-window views
  // that they should be positioned at.
  std::map<views::Widget*, int> widgets_;

  // The subset of |widgets_| which are visible.
  std::set<views::Widget*> visible_;

  // Lock which keeps the top-of-window views revealed based on the visible
  // anchored widgets.
  scoped_ptr<ImmersiveRevealedLock> revealed_lock_;

  DISALLOW_COPY_AND_ASSIGN(AnchoredWidgetManager);
};

ImmersiveModeControllerAsh::AnchoredWidgetManager::AnchoredWidgetManager(
    ImmersiveModeControllerAsh* controller)
    : controller_(controller) {
}

ImmersiveModeControllerAsh::AnchoredWidgetManager::~AnchoredWidgetManager() {
  while (!widgets_.empty())
    RemoveAnchoredWidget(widgets_.begin()->first);
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::AddAnchoredWidget(
    views::Widget* widget,
    int y_offset) {
  DCHECK(widget);
  bool already_added = widgets_.count(widget) > 0;
  widgets_[widget] = y_offset;

  if (already_added)
    return;

  widget->AddObserver(this);

  if (widget->IsVisible())
    visible_.insert(widget);

  UpdateRevealedLock();
  UpdateWidgetBounds(widget, y_offset);
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::RemoveAnchoredWidget(
    views::Widget* widget) {
  if (!widgets_.count(widget))
    return;

  widget->RemoveObserver(this);
  widgets_.erase(widget);
  visible_.erase(widget);

  UpdateRevealedLock();
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::
    MaybeRepositionAnchoredWidgets() {
  for (std::map<views::Widget*, int>::iterator it = widgets_.begin();
       it != widgets_.end(); ++it) {
    UpdateWidgetBounds(it->first, it->second);
  }

  UpdateRevealedLock();
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::
    OnImmersiveModeEnabled() {
  UpdateRevealedLock();
  // The top container bounds may have changed while immersive mode was
  // disabled.
  MaybeRepositionAnchoredWidgets();
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::UpdateRevealedLock() {
  if (visible_.empty()) {
    revealed_lock_.reset();
  } else if (controller_->IsRevealed()) {
    if (!revealed_lock_.get()) {
      // CAUTION: Acquiring the lock results in a reentrant call to
      // UpdateRevealedLock() when
      // |ImmersiveModeControllerAsh::animations_disabled_for_test_| is true.
      revealed_lock_.reset(controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES));
    }
  }
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::UpdateWidgetBounds(
    views::Widget* widget,
    int y_offset) {
  if (!controller_->IsEnabled() || !widget->IsVisible())
    return;

  gfx::Rect top_container_bounds =
      controller_->top_container_->GetBoundsInScreen();
  gfx::Rect bounds(widget->GetWindowBoundsInScreen());
  bounds.set_y(top_container_bounds.bottom() + y_offset);
  widget->SetBounds(bounds);
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::OnWidgetDestroying(
    views::Widget* widget) {
  RemoveAnchoredWidget(widget);
}

void ImmersiveModeControllerAsh::AnchoredWidgetManager::
    OnWidgetVisibilityChanged(
        views::Widget* widget,
        bool visible) {
  if (visible)
    visible_.insert(widget);
  else
    visible_.erase(widget);

  UpdateRevealedLock();

  std::map<views::Widget*, int>::iterator it = widgets_.find(widget);
  DCHECK(it != widgets_.end());
  UpdateWidgetBounds(it->first, it->second);
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
      mouse_x_when_hit_top_(-1),
      gesture_begun_(false),
      native_window_(NULL),
      animation_(new ui::SlideAnimation(this)),
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

  anchored_widget_manager_.reset(new AnchoredWidgetManager(this));
}

void ImmersiveModeControllerAsh::SetEnabled(bool enabled) {
  DCHECK(native_window_) << "Must initialize before enabling";
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  // Delay the initialization of the window observers till the first call to
  // SetEnabled(true) because FullscreenController is not yet initialized when
  // Init() is called.
  EnableWindowObservers(true);

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
      UpdateLocatedEventRevealedLock(NULL);
      UpdateFocusRevealedLock();
    }
    anchored_widget_manager_->OnImmersiveModeEnabled();
  } else {
    // Stop cursor-at-top tracking.
    top_edge_hover_timer_.Stop();
    // Snap immediately to the closed state.
    reveal_state_ = CLOSED;
    EnablePaintToLayer(false);
    delegate_->SetImmersiveStyle(false);

    // Relayout the root view because disabling immersive fullscreen may have
    // changed the result of NonClientFrameView::GetBoundsForClientView().
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

void ImmersiveModeControllerAsh::AnchorWidgetToTopContainer(
    views::Widget* widget,
    int y_offset) {
  anchored_widget_manager_->AddAnchoredWidget(widget, y_offset);
}

void ImmersiveModeControllerAsh::UnanchorWidgetFromTopContainer(
    views::Widget* widget) {
  anchored_widget_manager_->RemoveAnchoredWidget(widget);
}

void ImmersiveModeControllerAsh::OnTopContainerBoundsChanged() {
  anchored_widget_manager_->MaybeRepositionAnchoredWidgets();
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

  // Counterintuitively, we can still get synthesized mouse moves when
  // aura::client::CursorClient::IsMouseEventsEnabled() == false.
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
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
    UpdateLocatedEventRevealedLock(event);

  // Trigger a reveal if the cursor pauses at the top of the screen for a
  // while.
  if (event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)
    UpdateTopEdgeHoverTimer(event);
}

void ImmersiveModeControllerAsh::OnTouchEvent(ui::TouchEvent* event) {
  if (!enabled_ || event->type() != ui::ET_TOUCH_PRESSED)
    return;

  UpdateLocatedEventRevealedLock(event);
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
      if (ShouldHandleEvent(event->location())) {
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
  UpdateLocatedEventRevealedLock(NULL);
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

  UpdateLocatedEventRevealedLock(NULL);
  UpdateFocusRevealedLock();
}

////////////////////////////////////////////////////////////////////////////////
// Animation delegate:

void ImmersiveModeControllerAsh::AnimationEnded(
    const ui::Animation* animation) {
  if (reveal_state_ == SLIDING_OPEN) {
    // AnimationProgressed() is called immediately before AnimationEnded()
    // and does a layout.
    OnSlideOpenAnimationCompleted(LAYOUT_NO);
  } else if (reveal_state_ == SLIDING_CLOSED) {
    OnSlideClosedAnimationCompleted();
  }
}

void ImmersiveModeControllerAsh::AnimationProgressed(
    const ui::Animation* animation) {
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

void ImmersiveModeControllerAsh::OnWindowAddedToRootWindow(
    aura::Window* window) {
  DCHECK_EQ(window, native_window_);
  UpdatePreTargetHandler();
}

void ImmersiveModeControllerAsh::OnWindowRemovingFromRootWindow(
    aura::Window* window) {
  DCHECK_EQ(window, native_window_);
  UpdatePreTargetHandler();
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
  UpdateLocatedEventRevealedLock(NULL);
}

void ImmersiveModeControllerAsh::SetMouseHoveredForTest(bool hovered) {
  MoveMouse(top_container_, hovered);
  UpdateLocatedEventRevealedLock(NULL);
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

  UpdatePreTargetHandler();

  if (enable) {
    native_window_->AddObserver(this);
  } else {
    native_window_->RemoveObserver(this);
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
  // If the top-of-window views are already revealed or the cursor left the
  // top edge we don't need to trigger based on a timer anymore.
  if (reveal_state_ == SLIDING_OPEN ||
      reveal_state_ == REVEALED ||
      event->root_location().y() != 0) {
    top_edge_hover_timer_.Stop();
    return;
  }
  // The cursor is now at the top of the screen. Consider the cursor "not
  // moving" even if it moves a little bit in x, because users don't have
  // perfect pointing precision.
  int mouse_x = event->root_location().x();
  if (top_edge_hover_timer_.IsRunning() &&
      abs(mouse_x - mouse_x_when_hit_top_) <=
          ImmersiveFullscreenConfiguration::
              immersive_mode_reveal_x_threshold_pixels())
    return;

  // Start the reveal if the cursor doesn't move for some amount of time.
  mouse_x_when_hit_top_ = mouse_x;
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
    ui::LocatedEvent* event) {
  if (!enabled_)
    return;
  DCHECK(!event || event->IsMouseEvent() || event->IsTouchEvent());

  // Neither the mouse nor touch can initiate a reveal when the top-of-window
  // views are sliding closed or are closed with the following exceptions:
  // - Hovering at y = 0 which is handled in OnMouseEvent().
  // - Doing a SWIPE_OPEN edge gesture which is handled in OnGestureEvent().
  if (reveal_state_ == SLIDING_CLOSED || reveal_state_ == CLOSED)
    return;

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
    location_in_screen = event->location();
    aura::Window* target = static_cast<aura::Window*>(event->target());
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(target->GetRootWindow());
    screen_position_client->ConvertPointToScreen(target, &location_in_screen);
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

  gfx::Rect hit_bounds_in_screen = top_container_->GetBoundsInScreen();
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
    // If the currently active window is not |native_window_|, the top-of-window
    // views should be revealed if:
    // 1) The newly active window is a transient child of |native_window_|.
    // 2) The top-of-window views are already revealed. This restriction
    //    prevents a transient window opened by the web contents while the
    //    top-of-window views are hidden from from initiating a reveal.
    // The top-of-window views will stay revealed till |native_window_| is
    // reactivated.
    if (IsRevealed() && IsActiveWindowTransientChildOf(native_window_))
      hold_lock = true;
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
      UpdateLocatedEventRevealedLock(NULL);
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
  UpdateLocatedEventRevealedLock(NULL);
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

bool ImmersiveModeControllerAsh::ShouldHandleEvent(
    const gfx::Point& location) const {
  // All of the gestures that are of interest start in a region with left &
  // right edges agreeing with |top_container_|. When CLOSED it is difficult to
  // hit the bounds due to small size of the tab strip, so the hit target needs
  // to be extended on the bottom, thus the inset call. Finally there may be a
  // bezel sensor off screen logically above |top_container_| thus the test
  // needs to include gestures starting above.
  gfx::Rect near_bounds = top_container_->GetBoundsInScreen();
  if (reveal_state_ == CLOSED)
    near_bounds.Inset(gfx::Insets(0, 0, -kNearTopContainerDistance, 0));
  return near_bounds.Contains(location) ||
      ((location.y() < near_bounds.y()) &&
       (location.x() >= near_bounds.x()) &&
       (location.x() <= near_bounds.right()));
}

void ImmersiveModeControllerAsh::UpdatePreTargetHandler() {
  if (!native_window_)
    return;
  aura::RootWindow* root_window = native_window_->GetRootWindow();
  if (!root_window)
    return;
  if (observers_enabled_)
    root_window->AddPreTargetHandler(this);
  else
    root_window->RemovePreTargetHandler(this);
}
