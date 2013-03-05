// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/immersive_mode_controller.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"
#include "ui/views/view.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/command_line.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif

using ui::Compositor;
using views::View;

namespace {

// Time after which the edge trigger fires and top-chrome is revealed. This is
// after the mouse stops moving.
const int kTopEdgeRevealDelayMs = 200;

// Duration for the reveal show/hide slide animation. The slower duration is
// used for the initial slide out to give the user more change to see what
// happened.
const int kRevealSlowAnimationDurationMs = 400;
const int kRevealFastAnimationDurationMs = 200;

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// View to hold the tab strip, toolbar, and sometimes the bookmark bar during
// an immersive mode reveal. Paints on top of other layers in order to appear
// over the web contents. Immersive mode uses this view to avoid changing the
// BrowserView's view structure in the steady state. Informs the controller
// when the mouse leaves its bounds and when its children lose focus.
// TODO(jamescook): If immersive mode becomes non-experimental, use a permanent
// top-of-window container view in BrowserView instead of RevealView to avoid
// reparenting.
// TODO(jamescook): Bookmark bar does not yet work.
class ImmersiveModeController::RevealView : public views::View,
                                            public views::FocusChangeListener,
                                            public ui::CompositorObserver {
 public:
  RevealView(ImmersiveModeController* controller, BrowserView* browser_view);
  virtual ~RevealView();

  // Returns true if the mouse is in the bounds of this view.
  bool hovered() const { return hovered_; }

  // Trigger a paint, composite and then an EndReveal animation. Used for the
  // initial slide-out animation when enabling immersive mode.
  void EndRevealAfterPaint();

  // Returns true when this or any child view has focus.
  bool ContainsFocusedView() const;

  // Reparents the |browser_view_| tab strip, toolbar, and bookmark bar to
  // this view.
  void AcquireTopViews();

  // Reparents tab strip, toolbar, and bookmark bar back to |browser_view_|.
  void ReleaseTopViews();

  // views::View overrides:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

  // views::FocusChangeListener overrides:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE {}
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

  // ui::CompositorObserver overrides:
  virtual void OnCompositingDidCommit(Compositor* compositor) OVERRIDE {}
  virtual void OnCompositingStarted(Compositor* compositor,
                                    base::TimeTicks start_time) OVERRIDE {}
  virtual void OnCompositingEnded(Compositor* compositor) OVERRIDE;
  virtual void OnCompositingAborted(Compositor* compositor) OVERRIDE {}
  virtual void OnCompositingLockStateChanged(Compositor* compositor) OVERRIDE {}
  virtual void OnUpdateVSyncParameters(Compositor* compositor,
                                       base::TimeTicks timebase,
                                       base::TimeDelta interval) OVERRIDE {}

  // Called when painting is complete. Public for tests.
  void OnPainted();

 private:
  // Don't end the reveal until we have both painted and composited the layer.
  enum EndRevealState {
    END_REVEAL_NONE,
    END_REVEAL_NEEDS_PAINT,
    END_REVEAL_NEEDS_COMPOSITE,
  };

  // Returns true if the mouse cursor is inside this view.
  bool ContainsCursor() const;

  // The controller owns this view.
  ImmersiveModeController* controller_;

  // None of these views are owned.
  BrowserView* browser_view_;
  TabStrip* tabstrip_;
  ToolbarView* toolbar_view_;

  // True until the mouse leaves the view.
  bool hovered_;

  // State machine for PaintAndEndReveal().
  EndRevealState end_reveal_state_;

  // During widget destruction the views are disconnected from the widget and
  // GetFocusManager() and GetWidget() return NULL. Cache a pointer to the
  // focus manager so we can remove our listener.
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(RevealView);
};

ImmersiveModeController::RevealView::RevealView(
    ImmersiveModeController* controller,
    BrowserView* browser_view)
    : controller_(controller),
      browser_view_(browser_view),
      tabstrip_(NULL),
      toolbar_view_(NULL),
      hovered_(false),
      end_reveal_state_(END_REVEAL_NONE),
      focus_manager_(browser_view->GetFocusManager()) {
  set_notify_enter_exit_on_child(true);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);
  focus_manager_->AddFocusChangeListener(this);
}

ImmersiveModeController::RevealView::~RevealView() {
  // |this| could be deleted while waiting for a composite.
  if (layer()->GetCompositor())
    layer()->GetCompositor()->RemoveObserver(this);
  focus_manager_->RemoveFocusChangeListener(this);
}

void ImmersiveModeController::RevealView::EndRevealAfterPaint() {
  end_reveal_state_ = END_REVEAL_NEEDS_PAINT;
}

bool ImmersiveModeController::RevealView::ContainsFocusedView() const {
  // Views are considered to contain themselves and their children.
  return Contains(focus_manager_->GetFocusedView());
}

void ImmersiveModeController::RevealView::AcquireTopViews() {
  // Reparenting causes hit tests that require a parent for |this|.
  DCHECK(parent());

  tabstrip_ = browser_view_->tabstrip();
  toolbar_view_ = browser_view_->GetToolbarView();

  // Ensure the indices are what we expect before we start moving the views.
  DCHECK_EQ(browser_view_->GetIndexOf(tabstrip_), BrowserView::kTabstripIndex);
  DCHECK_EQ(browser_view_->GetIndexOf(toolbar_view_),
            BrowserView::kToolbarIndex);

  AddChildView(tabstrip_);
  AddChildView(toolbar_view_);

  // Set our initial bounds, which triggers a Layout() and SchedulePaint().
  int width = parent()->width();
  int height = toolbar_view_->bounds().bottom();
  SetBounds(0, 0, width, height);
}

void ImmersiveModeController::RevealView::ReleaseTopViews() {
  // Reparenting causes hit tests that require a parent for |this|.
  DCHECK(parent());
  // Check that the top views have not already been released.
  DCHECK(tabstrip_);
  DCHECK(toolbar_view_);

  browser_view_->AddChildViewAt(tabstrip_, BrowserView::kTabstripIndex);
  browser_view_->AddChildViewAt(toolbar_view_, BrowserView::kToolbarIndex);

  // Ensure the newly restored views get painted.
  tabstrip_->SchedulePaint();
  toolbar_view_->SchedulePaint();

  tabstrip_ = NULL;
  toolbar_view_ = NULL;
}

std::string ImmersiveModeController::RevealView::GetClassName() const {
  return "RevealView";
}

void ImmersiveModeController::RevealView::OnMouseEntered(
    const ui::MouseEvent& event) {
  // Entering this view or a child view always means we are hovered.
  hovered_ = true;
}

void ImmersiveModeController::RevealView::OnMouseExited(
    const ui::MouseEvent& event) {
  // TODO(jamescook):  Currently Ash does not differentiate between disabling
  // and hiding the mouse. When the mouse is "hidden" by typing, it actually
  // moves to -10000, -10000 and generates mouse moved events. For now, ignore
  // mouse exit events caused by a move to that location. Remove this code
  // when crbug.com/153703 is fixed.
  if (event.location().x() == -10000 && event.location().y() == -10000)
    return;

  // This view may still be hovered if the mouse exit was on a child view.
  bool was_hovered = hovered_;
  hovered_ = ContainsCursor();

  if (was_hovered && !hovered_)
    controller_->OnRevealViewLostMouse();
}

void ImmersiveModeController::RevealView::PaintChildren(gfx::Canvas* canvas) {
  // Top-views depend on parts of the frame (themes, window buttons) being
  // painted underneath them. Clip rect has already been set to the bounds
  // of this view, so just paint the frame.
  views::View* frame = browser_view_->frame()->GetFrameView();
  frame->Paint(canvas);

  views::View::PaintChildren(canvas);

  OnPainted();
}

void ImmersiveModeController::RevealView::OnDidChangeFocus(View* focused_before,
                                                           View* focused_now) {
  // If one of this view's children had focus before, but doesn't have focus
  // now, inform the controller.
  if (Contains(focused_before) && !Contains(focused_now))
    controller_->OnRevealViewLostFocus();
  // |this| may be deleted.
}

void ImmersiveModeController::RevealView::OnCompositingEnded(
    Compositor* compositor) {
  if (end_reveal_state_ == END_REVEAL_NEEDS_COMPOSITE) {
    // Pixels should be on the screen, so slide out the layer.
    end_reveal_state_ = END_REVEAL_NONE;
    layer()->GetCompositor()->RemoveObserver(this);
    controller_->EndReveal(ANIMATE_SLOW, LAYOUT_YES);
  }
}

void ImmersiveModeController::RevealView::OnPainted() {
  if (end_reveal_state_ == END_REVEAL_NEEDS_PAINT) {
    end_reveal_state_ = END_REVEAL_NEEDS_COMPOSITE;
    layer()->GetCompositor()->AddObserver(this);
  }
}

bool ImmersiveModeController::RevealView::ContainsCursor() const {
  gfx::Point cursor_point(gfx::Screen::GetScreenFor(
      GetWidget()->GetNativeView())->GetCursorScreenPoint());
  ConvertPointToTarget(NULL, this, &cursor_point);
  return GetLocalBounds().Contains(cursor_point);
}

////////////////////////////////////////////////////////////////////////////////

#if defined(USE_AURA)
// Observer to watch for window restore. views::Widget does not provide a hook
// to observe for window restore, so do this at the Aura level.
class ImmersiveModeController::WindowObserver : public aura::WindowObserver {
 public:
  explicit WindowObserver(ImmersiveModeController* controller)
      : controller_(controller) {
    controller_->native_window_->AddObserver(this);
  }

  virtual ~WindowObserver() {
    controller_->native_window_->RemoveObserver(this);
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    using aura::client::kShowStateKey;
    if (key == kShowStateKey) {
      // Disable immersive mode when leaving the fullscreen state.
      if (window->GetProperty(kShowStateKey) != ui::SHOW_STATE_FULLSCREEN)
        controller_->SetEnabled(false);
      return;
    }
#if defined(USE_ASH)
    using ash::internal::kImmersiveModeKey;
    if (key == kImmersiveModeKey) {
      // Another component has toggled immersive mode.
      controller_->SetEnabled(window->GetProperty(kImmersiveModeKey));
      return;
    }
#endif
  }

 private:
  ImmersiveModeController* controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};
#endif  // defined(USE_AURA)

////////////////////////////////////////////////////////////////////////////////

ImmersiveModeController::ImmersiveModeController(BrowserView* browser_view)
    : browser_view_(browser_view),
      enabled_(false),
      revealed_(false),
      hide_tab_indicators_(false),
      native_window_(NULL) {
}

ImmersiveModeController::~ImmersiveModeController() {
  // Ensure views are reparented if we are deleted while revealing.
  EndReveal(ANIMATE_NO, LAYOUT_NO);
  // Clean up our window observers.
  EnableWindowObservers(false);
}

void ImmersiveModeController::Init() {
  // Browser view is detached from its widget during destruction. Cache the
  // window pointer so |this| can stop observing during destruction.
  native_window_ = browser_view_->GetNativeWindow();
  DCHECK(native_window_);
  EnableWindowObservers(true);

#if defined(USE_ASH)
  // Optionally allow the tab indicators to be hidden.
  hide_tab_indicators_ = CommandLine::ForCurrentProcess()->
      HasSwitch(ash::switches::kAshImmersiveHideTabIndicators);
#endif
}

void ImmersiveModeController::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  if (enabled_) {
    // When UI is enabled slide-out the reveal views by slamming it to open then
    // triggering an end-reveal animation.
    StartReveal(ANIMATE_NO);
    reveal_view_->EndRevealAfterPaint();
  } else {
    // Layout occurs below because EndReveal() only performs layout if the view
    // is already revealed.
    EndReveal(ANIMATE_NO, LAYOUT_NO);
    LayoutBrowserView(false);
    // Stop cursor-at-top tracking.
    top_timer_.Stop();
  }

#if defined(USE_ASH)
  // This causes a no-op call to SetEnabled() since enabled_ is already set.
  native_window_->SetProperty(ash::internal::kImmersiveModeKey, enabled_);
  // Ash on Windows may not have a shell.
  if (ash::Shell::HasInstance()) {
    // Shelf auto-hides in immersive mode.
    ash::Shell::GetInstance()->UpdateShelfVisibility();
  }
#endif
}

views::View* ImmersiveModeController::reveal_view() {
  return reveal_view_.get();
}

void ImmersiveModeController::MaybeStackViewAtTop() {
#if defined(USE_AURA)
  if (enabled_ && revealed_ && reveal_view_.get()) {
    ui::Layer* reveal_layer = reveal_view_->layer();
    reveal_layer->parent()->StackAtTop(reveal_layer);
  }
#endif
}

void ImmersiveModeController::MaybeStartReveal() {
  if (enabled_ && !revealed_)
    StartReveal(ANIMATE_FAST);
}

void ImmersiveModeController::CancelReveal() {
  EndReveal(ANIMATE_NO, LAYOUT_YES);
}

////////////////////////////////////////////////////////////////////////////////

// ui::EventHandler overrides:
void ImmersiveModeController::OnMouseEvent(ui::MouseEvent* event) {
  if (!enabled_ || event->type() != ui::ET_MOUSE_MOVED)
    return;
  if (event->location().y() == 0) {
    // Start a reveal if the mouse touches the top of the screen and then stops
    // moving for a little while. This mirrors the Ash launcher behavior.
    top_timer_.Stop();
    // Timer is stopped when |this| is destroyed, hence Unretained() is safe.
    top_timer_.Start(FROM_HERE,
                     base::TimeDelta::FromMilliseconds(kTopEdgeRevealDelayMs),
                     base::Bind(&ImmersiveModeController::StartReveal,
                                base::Unretained(this),
                                ANIMATE_FAST));
  } else {
    // Cursor left the top edge.
    top_timer_.Stop();
  }
  // Pass along event for further handling.
}

// ui::ImplicitAnimationObserver overrides:
void ImmersiveModeController::OnImplicitAnimationsCompleted() {
  OnHideAnimationCompleted();
}

////////////////////////////////////////////////////////////////////////////////
// Testing interface:

void ImmersiveModeController::SetHideTabIndicatorsForTest(bool hide) {
  hide_tab_indicators_ = hide;
}

void ImmersiveModeController::SetEnabledForTest(bool enabled) {
  bool was_enabled = enabled_;
  SetEnabled(enabled);
  if (enabled && !was_enabled) {
    // Simulate the reveal view being painted and composited.
    reveal_view_->OnPainted();
    reveal_view_->OnCompositingEnded(NULL);
  }
}

void ImmersiveModeController::StartRevealForTest() {
  StartReveal(ANIMATE_NO);
}

void ImmersiveModeController::OnRevealViewLostMouseForTest() {
  OnRevealViewLostMouse();
}

////////////////////////////////////////////////////////////////////////////////
// private:

void ImmersiveModeController::EnableWindowObservers(bool enable) {
  if (!native_window_) {
    NOTREACHED() << "ImmersiveModeController not initialized";
    return;
  }
#if defined(USE_AURA)
  // TODO(jamescook): Porting immersive mode to non-Aura views will require
  // a method to monitor incoming mouse move events without handling them.
  // Currently views uses GetEventHandlerForPoint() to route events directly
  // to either a tab or the caption area, bypassing pre-target handlers and
  // intermediate views.
  if (enable)
    native_window_->AddPreTargetHandler(this);
  else
    native_window_->RemovePreTargetHandler(this);

  // The window observer adds and removes itself from the native window.
  // TODO(jamescook): Porting to non-Aura will also require a method to monitor
  // for window restore, which is not provided by views Widget.
  window_observer_.reset(enable ? new WindowObserver(this) : NULL);
#endif  // defined(USE_AURA)
}

void ImmersiveModeController::StartReveal(Animate animate) {
  if (revealed_)
    return;
  revealed_ = true;

  // Ensure window caption buttons are updated and the view bounds are computed
  // at normal (non-immersive-style) size.
  LayoutBrowserView(false);

  // Place tabstrip, toolbar, and bookmarks bar in a new view at the end of
  // the BrowserView hierarchy so it paints over the web contents.
  reveal_view_.reset(new RevealView(this, browser_view_));
  browser_view_->AddChildView(reveal_view_.get());
  reveal_view_->AcquireTopViews();

  // Slide in the reveal view.
  if (animate != ANIMATE_NO)
    AnimateShowRevealView();  // Show is always fast.
}

void ImmersiveModeController::AnimateShowRevealView() {
  DCHECK(reveal_view_.get());
  gfx::Transform transform;
  transform.Translate(0, -reveal_view_->height());
  reveal_view_->SetTransform(transform);

  ui::ScopedLayerAnimationSettings settings(
      reveal_view_->layer()->GetAnimator());
  settings.SetTweenType(ui::Tween::EASE_OUT);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kRevealFastAnimationDurationMs));
  reveal_view_->SetTransform(gfx::Transform());
}

void ImmersiveModeController::OnRevealViewLostMouse() {
  // Stop the reveal if the view's children don't have focus.
  // TODO(jamescook): Consider stopping the reveal after a delay. This code
  // isn't using a MouseWatcher because it needs to know if the mouse re-enters
  // the RevealView before focus is lost.
  if (!reveal_view_->ContainsFocusedView())
    EndReveal(ANIMATE_FAST, LAYOUT_YES);
}

void ImmersiveModeController::OnRevealViewLostFocus() {
  // Stop the reveal if the mouse is outside the reveal view.
  if (!reveal_view_->hovered())
    EndReveal(ANIMATE_FAST, LAYOUT_YES);
}

void ImmersiveModeController::EndReveal(Animate animate, Layout layout) {
  if (!revealed_)
    return;
  revealed_ = false;

  if (reveal_view_.get()) {
    // Always reparent the views when triggering a hide, otherwise tooltip
    // position lookup during the slide-out will get confused about view
    // parenting and crash.
    reveal_view_->ReleaseTopViews();
    // Animations reset the reveal view when complete, which also removes
    // it from its parent BrowserView.
    if (animate == ANIMATE_FAST)
      AnimateHideRevealView(kRevealFastAnimationDurationMs);
    else if (animate == ANIMATE_SLOW)
      AnimateHideRevealView(kRevealSlowAnimationDurationMs);
    else
      reveal_view_.reset();
  }

  if (layout == LAYOUT_YES)
    LayoutBrowserView(enabled_);
}

void ImmersiveModeController::LayoutBrowserView(bool immersive_style) {
  // Update the window caption buttons.
  browser_view_->frame()->non_client_view()->frame_view()->
      ResetWindowControls();
  browser_view_->tabstrip()->SetImmersiveStyle(immersive_style);
  browser_view_->Layout();
}

void ImmersiveModeController::AnimateHideRevealView(int duration_ms) {
  ui::Layer* layer = reveal_view_->layer();
  // Stop any show animation in progress, but don't skip to the end. This
  // avoids a visual "pop" when starting a hide in the middle of a show.
  layer->GetAnimator()->AbortAllAnimations();
  // Detach the layer from its delegate to stop updating it. This prevents
  // graphical glitches due to hover events causing repaints during the hide.
  layer->set_delegate(NULL);

  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTweenType(ui::Tween::EASE_OUT);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(duration_ms));
  settings.AddObserver(this);  // Resets |reveal_view_| on completion.
  gfx::Transform transform;
  transform.Translate(0, -layer->bounds().height());
  layer->SetTransform(transform);
}

void ImmersiveModeController::OnHideAnimationCompleted() {
  reveal_view_.reset();  // Also removes from parent.
}
