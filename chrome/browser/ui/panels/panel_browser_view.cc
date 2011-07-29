// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_view.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_win.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "grit/chromium_strings.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/widget/widget.h"

namespace {
// This value is experimental and subjective.
const int kSetBoundsAnimationMs = 200;

// The panel can be fully minimized to 3-pixel lines.
const int kFullyMinimizedHeight = 3;

// Delay before click-to-minimize is allowed after the attention has been
// cleared.
const base::TimeDelta kSuspendMinimizeOnClickIntervalMs =
    base::TimeDelta::FromMilliseconds(500);
}

NativePanel* Panel::CreateNativePanel(Browser* browser, Panel* panel,
                                      const gfx::Rect& bounds) {
  PanelBrowserView* view = new PanelBrowserView(browser, panel, bounds);
  (new BrowserFrame(view))->InitBrowserFrame();
  return view;
}

PanelBrowserView::PanelBrowserView(Browser* browser, Panel* panel,
                                   const gfx::Rect& bounds)
  : BrowserView(browser),
    panel_(panel),
    bounds_(bounds),
    original_height_(bounds.height()),
    closed_(false),
    focused_(false),
    mouse_pressed_(false),
    mouse_dragging_(false),
    is_drawing_attention_(false) {
}

PanelBrowserView::~PanelBrowserView() {
}

void PanelBrowserView::Init() {
  BrowserView::Init();

  GetWidget()->SetAlwaysOnTop(true);
  GetWidget()->non_client_view()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

void PanelBrowserView::Close() {
  GetWidget()->RemoveObserver(this);
  closed_ = true;

  // Cancel any currently running animation since we're closing down.
  if (bounds_animator_.get())
    bounds_animator_.reset();

  ::BrowserView::Close();

  // Stop the global mouse watcher only if we do not have any panels up.
#if defined(OS_WIN)
  if (panel_->manager()->num_panels() == 1)
    StopMouseWatcher();
#endif
}

bool PanelBrowserView::CanResize() const {
  return false;
}

bool PanelBrowserView::CanMaximize() const {
  return false;
}

void PanelBrowserView::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;

  //// No animation if the panel is being dragged.
  if (mouse_dragging_) {
    ::BrowserView::SetBounds(bounds);
    return;
  }

  animation_start_bounds_ = GetBounds();

  if (!bounds_animator_.get()) {
    bounds_animator_.reset(new ui::SlideAnimation(this));
    bounds_animator_->SetSlideDuration(kSetBoundsAnimationMs);
  }

  if (bounds_animator_->IsShowing())
    bounds_animator_->Reset();
  bounds_animator_->Show();
}

void PanelBrowserView::UpdateTitleBar() {
  ::BrowserView::UpdateTitleBar();
  GetFrameView()->UpdateTitleBar();
}

bool PanelBrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  *bounds = GetPanelBounds();
  return true;
}

void PanelBrowserView::OnWidgetActivationChanged(views::Widget* widget,
                                                 bool active) {
  ::BrowserView::OnWidgetActivationChanged(widget, active);

#if defined(OS_WIN)
  // The panel window is in focus (actually accepting keystrokes) if it is
  // active and belongs to a foreground application.
  bool focused = active &&
      GetFrameView()->GetWidget()->GetNativeView() == ::GetForegroundWindow();
#else
  // TODO(jianli): Investigate focus behavior for ChromeOS
  bool focused = active;
#endif

  if (focused_ == focused)
    return;
  focused_ = focused;

  GetFrameView()->OnFocusChanged(focused);

  // Clear the attention state if the panel is on focus.
  if (is_drawing_attention_ && focused_)
    StopDrawingAttention();
}

bool PanelBrowserView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (mouse_pressed_ && accelerator.key_code() == ui::VKEY_ESCAPE) {
    OnTitleBarMouseCaptureLost();
    return true;
  }
  return BrowserView::AcceleratorPressed(accelerator);
}

void PanelBrowserView::AnimationProgressed(const ui::Animation* animation) {
  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);
  ::BrowserView::SetBounds(new_bounds);
}

void PanelBrowserView::OnDisplayChanged() {
  BrowserView::OnDisplayChanged();
  panel_->manager()->OnDisplayChanged();
}

void PanelBrowserView::OnWorkAreaChanged() {
  BrowserView::OnWorkAreaChanged();
  panel_->manager()->OnDisplayChanged();
}

bool PanelBrowserView::WillProcessWorkAreaChange() const {
  return true;
}

void PanelBrowserView::ShowPanel() {
  Show();
}

void PanelBrowserView::ShowPanelInactive() {
  ShowInactive();
}

gfx::Rect PanelBrowserView::GetPanelBounds() const {
  return bounds_;
}

void PanelBrowserView::SetPanelBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void PanelBrowserView::OnPanelExpansionStateChanged(
    Panel::ExpansionState expansion_state) {
  int height;
  switch (expansion_state) {
    case Panel::EXPANDED:
      height = original_height_;
      break;
    case Panel::TITLE_ONLY:
      height = GetFrameView()->NonClientTopBorderHeight();
      break;
    case Panel::MINIMIZED:
      height = kFullyMinimizedHeight;

      // Start the mouse watcher so that we can bring up the minimized panels.
      // TODO(jianli): Need to support mouse watching in ChromeOS.
#if defined(OS_WIN)
      EnsureMouseWatcherStarted();
#endif
      break;
    default:
      NOTREACHED();
      height = original_height_;
      break;
  }

  gfx::Rect bounds = bounds_;
  bounds.set_y(bounds.y() + bounds.height() - height);
  bounds.set_height(height);
  SetBounds(bounds);
}

bool PanelBrowserView::ShouldBringUpPanelTitleBar(int mouse_x,
                                                  int mouse_y) const {
  // We do not want to bring up other minimized panels if the mouse is over the
  // panel that pops up the title-bar to attract attention.
  if (is_drawing_attention_)
    return false;

  return bounds_.x() <= mouse_x && mouse_x <= bounds_.right() &&
         mouse_y >= bounds_.y();
}

void PanelBrowserView::ClosePanel() {
  Close();
}

void PanelBrowserView::ActivatePanel() {
  Activate();
}

void PanelBrowserView::DeactivatePanel() {
  Deactivate();
}

bool PanelBrowserView::IsPanelActive() const {
  return IsActive();
}

gfx::NativeWindow PanelBrowserView::GetNativePanelHandle() {
  return GetNativeHandle();
}

void PanelBrowserView::UpdatePanelTitleBar() {
  UpdateTitleBar();
}

void PanelBrowserView::ShowTaskManagerForPanel() {
  ShowTaskManager();
}

void PanelBrowserView::NotifyPanelOnUserChangedTheme() {
  UserChangedTheme();
}

void PanelBrowserView::DrawAttention() {
  // Don't draw attention for active panel.
  if (is_drawing_attention_ || focused_)
    return;
  is_drawing_attention_ = true;

  // Bring up the title bar to get people's attention.
  if (panel_->expansion_state() == Panel::MINIMIZED)
    panel_->SetExpansionState(Panel::TITLE_ONLY);

  GetFrameView()->SchedulePaint();
}

bool PanelBrowserView::IsDrawingAttention() const {
  return is_drawing_attention_;
}

void PanelBrowserView::DestroyPanelBrowser() {
  DestroyBrowser();
}

void PanelBrowserView::StopDrawingAttention() {
  if (!is_drawing_attention_)
    return;
  is_drawing_attention_ = false;

  // This function is called from OnWidgetActivationChanged to clear the
  // attention, per one of the following user interactions:
  // 1) clicking on the title-bar
  // 2) clicking on the client area
  // 3) switching to the panel via keyboard
  // For case 1, we do not want the expanded panel to be minimized since the
  // user clicks on it to mean to clear the attention.
  attention_cleared_time_ = base::TimeTicks::Now();

  // Bring up the title bar.
  if (panel_->expansion_state() == Panel::TITLE_ONLY)
    panel_->SetExpansionState(Panel::EXPANDED);

  GetFrameView()->SchedulePaint();
}

NativePanelTesting* PanelBrowserView::GetNativePanelTesting() {
  return this;
}

PanelBrowserFrameView* PanelBrowserView::GetFrameView() const {
  return static_cast<PanelBrowserFrameView*>(frame()->GetFrameView());
}

bool PanelBrowserView::OnTitleBarMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;
  mouse_pressed_ = true;
  mouse_pressed_point_ = event.location();
  mouse_dragging_ = false;
  return true;
}

bool PanelBrowserView::OnTitleBarMouseDragged(const views::MouseEvent& event) {
  if (!mouse_pressed_)
    return false;

  // We do not allow dragging vertically.
  int delta_x = event.location().x() - mouse_pressed_point_.x();
  if (!mouse_dragging_ && ExceededDragThreshold(delta_x, 0)) {
    panel_->manager()->StartDragging(panel_.get());
    mouse_dragging_ = true;
  }
  if (mouse_dragging_)
    panel_->manager()->Drag(delta_x);
  return true;
}

bool PanelBrowserView::OnTitleBarMouseReleased(const views::MouseEvent& event) {
  if (mouse_dragging_)
    return EndDragging(false);

  // Do not minimize the panel when we just clear the attention state. This is
  // a hack to prevent the panel from being minimized when the user clicks on
  // the title-bar to clear the attention.
  if (panel_->expansion_state() == Panel::EXPANDED &&
      base::TimeTicks::Now() < attention_cleared_time_ +
                               kSuspendMinimizeOnClickIntervalMs) {
    return true;
  }

  Panel::ExpansionState new_expansion_state =
    (panel_->expansion_state() != Panel::EXPANDED) ? Panel::EXPANDED
                                                   : Panel::MINIMIZED;
  panel_->SetExpansionState(new_expansion_state);
  return true;
}

bool PanelBrowserView::OnTitleBarMouseCaptureLost() {
  return EndDragging(true);
}

bool PanelBrowserView::EndDragging(bool cancelled) {
  // Only handle clicks that started in our window.
  if (!mouse_pressed_)
    return false;
  mouse_pressed_ = false;

  if (!mouse_dragging_)
    cancelled = true;
  mouse_dragging_ = false;
  panel_->manager()->EndDragging(cancelled);
  return true;
}
