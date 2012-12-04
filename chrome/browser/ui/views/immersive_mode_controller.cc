// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/immersive_mode_controller.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// Time after which the edge trigger fires and top-chrome is revealed.
const int kRevealDelayMs = 200;

// During an immersive-mode top-of-window reveal, how long to wait after the
// mouse exits the views before hiding them again.
const int kHideDelayMs = 200;

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// View to hold the tab strip, toolbar, and sometimes the bookmark bar during
// an immersive mode reveal. Paints on top of other layers in order to appear
// over the web contents. Immersive mode uses this view to avoid changing the
// BrowserView's view structure in the steady state.
// TODO(jamescook): If immersive mode becomes non-experimental, use a permanent
// top-of-window container view in BrowserView instead of RevealView to avoid
// reparenting.
// TODO(jamescook): Bookmark bar does not yet work.
class RevealView : public views::View {
 public:
  explicit RevealView(BrowserView* browser_view);
  virtual ~RevealView();

  // Reparents the |browser_view_| tab strip, toolbar, and bookmark bar to
  // this view.
  void AcquireTopViews();

  // Reparents tab strip, toolbar, and bookmark bar back to |browser_view_|.
  void ReleaseTopViews();

  // views::View overrides:
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

 private:
  // None of these views are owned.
  BrowserView* browser_view_;
  TabStrip* tabstrip_;
  ToolbarView* toolbar_view_;

  DISALLOW_COPY_AND_ASSIGN(RevealView);
};

RevealView::RevealView(BrowserView* browser_view)
    : browser_view_(browser_view),
      tabstrip_(NULL),
      toolbar_view_(NULL) {
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);
}

RevealView::~RevealView() {}

void RevealView::AcquireTopViews() {
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

  // Set our initial bounds, which triggers a Layout().
  int width = parent()->width();
  int height = toolbar_view_->bounds().bottom();
  SetBounds(0, 0, width, height);
}

void RevealView::ReleaseTopViews() {
  // Reparenting causes hit tests that require a parent for |this|.
  DCHECK(parent());

  browser_view_->AddChildViewAt(tabstrip_, BrowserView::kTabstripIndex);
  browser_view_->AddChildViewAt(toolbar_view_, BrowserView::kToolbarIndex);

  // Ensure the newly restored views get painted.
  tabstrip_->SchedulePaint();
  toolbar_view_->SchedulePaint();

  tabstrip_ = NULL;
  toolbar_view_ = NULL;
}

void RevealView::PaintChildren(gfx::Canvas* canvas) {
  // Top-views depend on parts of the frame (themes, window buttons) being
  // painted underneath them. Clip rect has already been set to the bounds
  // of this view, so just paint the frame.
  views::View* frame = browser_view_->frame()->GetFrameView();
  frame->Paint(canvas);

  views::View::PaintChildren(canvas);
}

////////////////////////////////////////////////////////////////////////////////

ImmersiveModeController::ImmersiveModeController(BrowserView* browser_view)
    : browser_view_(browser_view),
      enabled_(false),
      revealed_(false) {
}

ImmersiveModeController::~ImmersiveModeController() {
  // Ensure views are reparented if we are deleted mid-reveal.
  if (reveal_view_.get()) {
    reveal_view_->ReleaseTopViews();
    ResetRevealView();
  }
}

void ImmersiveModeController::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;

  if (enabled_) {
    browser_view_->tabstrip()->SetImmersiveStyle(true);
    browser_view_->Layout();
  } else {
    // Don't Layout() browser_view_ because EndReveal() does so.
    EndReveal(false);
    top_timer_.Stop();
  }

#if defined(USE_AURA)
  // TODO(jamescook): If we want to port this feature to non-Aura views we'll
  // need a method to monitor incoming mouse move events without handling them.
  // Currently views uses GetEventHandlerForPoint() to route events directly
  // to either a tab or the caption area, bypassing pre-target handlers and
  // intermediate views.
  if (enabled_)
    browser_view_->GetNativeWindow()->AddPreTargetHandler(this);
  else
    browser_view_->GetNativeWindow()->RemovePreTargetHandler(this);
#endif  // defined(USE_AURA)
}

// ui::EventHandler overrides:
ui::EventResult ImmersiveModeController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED)
    return ui::ER_UNHANDLED;
  if (event->location().y() == 0) {
    // Use a timer to detect if the cursor stays at the top past a delay.
    if (!top_timer_.IsRunning()) {
      top_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kRevealDelayMs),
                       this, &ImmersiveModeController::StartReveal);
    }
  } else {
    // Cursor left the top edge.
    top_timer_.Stop();
  }
  // Pass along event for further handling.
  return ui::ER_UNHANDLED;
}

// views::MouseWatcherListener overrides:
void ImmersiveModeController::MouseMovedOutOfHost() {
  EndReveal(true);
}

// ui::ImplicitAnimationObserver overrides:
void ImmersiveModeController::OnImplicitAnimationsCompleted() {
  // Fired when the slide-out animation completes.
  ResetRevealView();
}

// Testing interface:
void ImmersiveModeController::StartRevealForTest() {
  StartReveal();
}

void ImmersiveModeController::EndRevealForTest() {
  EndReveal(false);
}

////////////////////////////////////////////////////////////////////////////////
// private:

void ImmersiveModeController::StartReveal() {
  if (revealed_)
    return;
  revealed_ = true;

  // Recompute the bounds of the views when painted normally.
  browser_view_->tabstrip()->SetImmersiveStyle(false);
  browser_view_->Layout();

  // Place tabstrip, toolbar, and bookmarks bar in a new view at the end of
  // the BrowserView hierarchy so it paints over the web contents.
  reveal_view_.reset(new RevealView(browser_view_));
  browser_view_->AddChildView(reveal_view_.get());
  reveal_view_->AcquireTopViews();

  // Slide in the reveal view.
  AnimateShowRevealView();

  // Stop the immersive reveal when the mouse leaves the top-of-window area.
  StartMouseWatcher();
}

void ImmersiveModeController::AnimateShowRevealView() {
  DCHECK(reveal_view_.get());
  gfx::Transform transform;
  transform.Translate(0, -reveal_view_->height());
  reveal_view_->SetTransform(transform);

  ui::ScopedLayerAnimationSettings settings(
      reveal_view_->layer()->GetAnimator());
  settings.SetTweenType(ui::Tween::EASE_OUT);
  reveal_view_->SetTransform(gfx::Transform());
}

void ImmersiveModeController::StartMouseWatcher() {
  DCHECK(reveal_view_.get());
  views::MouseWatcherViewHost* host =
      new views::MouseWatcherViewHost(reveal_view_.get(), gfx::Insets());
  // MouseWatcher takes ownership of |host|.
  mouse_watcher_.reset(new views::MouseWatcher(host, this));
  mouse_watcher_->set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kHideDelayMs));
  mouse_watcher_->Start();
}

void ImmersiveModeController::EndReveal(bool animate) {
  revealed_ = false;
  mouse_watcher_.reset();

  if (reveal_view_.get()) {
    reveal_view_->ReleaseTopViews();
    if (animate) {
      // Animation resets the reveal view when complete.
      AnimateHideRevealView();
    } else {
      ResetRevealView();
    }
  }

  browser_view_->tabstrip()->SetImmersiveStyle(enabled_);
  browser_view_->Layout();
}

void ImmersiveModeController::AnimateHideRevealView() {
  ui::Layer* layer = reveal_view_->layer();
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTweenType(ui::Tween::EASE_OUT);
  settings.AddObserver(this);  // Resets |reveal_view_| on completion.
  gfx::Transform transform;
  transform.Translate(0, -layer->bounds().height());
  layer->SetTransform(transform);
}

void ImmersiveModeController::ResetRevealView() {
  browser_view_->RemoveChildView(reveal_view_.get());
  reveal_view_.reset();
}

