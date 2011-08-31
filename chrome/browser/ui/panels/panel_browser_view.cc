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
const int kSetBoundsAnimationMs = 180;

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

  // No animation if the panel is being dragged.
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

bool PanelBrowserView::GetSavedWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetPanelBounds();
  *show_state = ui::SHOW_STATE_NORMAL;
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
    OnTitlebarMouseCaptureLost();
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
      height = PanelBrowserFrameView::MinimizedPanelHeight();

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

  int bottom = panel_->manager()->GetBottomPositionForExpansionState(
      expansion_state);
  gfx::Rect bounds = bounds_;
  bounds.set_y(bottom - height);
  bounds.set_height(height);
  SetBounds(bounds);
}

bool PanelBrowserView::ShouldBringUpPanelTitlebar(int mouse_x,
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

FindBar* PanelBrowserView::CreatePanelFindBar() {
  return CreateFindBar();
}

void PanelBrowserView::NotifyPanelOnUserChangedTheme() {
  UserChangedTheme();
}

void PanelBrowserView::DrawAttention() {
  // Don't draw attention for active panel.
  if (is_drawing_attention_ || focused_)
    return;
  is_drawing_attention_ = true;

  // Bring up the titlebar to get people's attention.
  if (panel_->expansion_state() == Panel::MINIMIZED)
    panel_->SetExpansionState(Panel::TITLE_ONLY);

  GetFrameView()->SchedulePaint();
}

bool PanelBrowserView::IsDrawingAttention() const {
  return is_drawing_attention_;
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

  // Bring up the titlebar.
  if (panel_->expansion_state() == Panel::TITLE_ONLY)
    panel_->SetExpansionState(Panel::EXPANDED);

  GetFrameView()->SchedulePaint();
}

bool PanelBrowserView::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void PanelBrowserView::HandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  HandleKeyboardEvent(event);
}

Browser* PanelBrowserView::GetPanelBrowser() const {
  return browser();
}

void PanelBrowserView::DestroyPanelBrowser() {
  DestroyBrowser();
}

PanelBrowserFrameView* PanelBrowserView::GetFrameView() const {
  return static_cast<PanelBrowserFrameView*>(frame()->GetFrameView());
}

bool PanelBrowserView::OnTitlebarMousePressed(const gfx::Point& location) {
  mouse_pressed_ = true;
  mouse_pressed_point_ = location;
  mouse_dragging_ = false;
  return true;
}

bool PanelBrowserView::OnTitlebarMouseDragged(const gfx::Point& location) {
  if (!mouse_pressed_)
    return false;

  int delta_x = location.x() - mouse_pressed_point_.x();
  int delta_y = location.y() - mouse_pressed_point_.y();
  if (!mouse_dragging_ && ExceededDragThreshold(delta_x, delta_y)) {
    panel_->manager()->StartDragging(panel_.get());
    mouse_dragging_ = true;
  }
  if (mouse_dragging_)
    panel_->manager()->Drag(delta_x);
  return true;
}

bool PanelBrowserView::OnTitlebarMouseReleased() {
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

bool PanelBrowserView::OnTitlebarMouseCaptureLost() {
  if (mouse_dragging_)
    return EndDragging(true);
  return true;
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

// NativePanelTesting implementation.
class NativePanelTestingWin : public NativePanelTesting {
 public:
  explicit NativePanelTestingWin(PanelBrowserView* panel_browser_view);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& point) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar() OVERRIDE;
  virtual void DragTitlebar(int delta_x, int delta_y) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;

  PanelBrowserView* panel_browser_view_;
};

// static
NativePanelTesting* NativePanelTesting::Create(NativePanel* native_panel) {
  return new NativePanelTestingWin(static_cast<PanelBrowserView*>(
      native_panel));
}

NativePanelTestingWin::NativePanelTestingWin(
    PanelBrowserView* panel_browser_view) :
    panel_browser_view_(panel_browser_view) {
}

void NativePanelTestingWin::PressLeftMouseButtonTitlebar(
    const gfx::Point& point) {
  panel_browser_view_->OnTitlebarMousePressed(point);
}

void NativePanelTestingWin::ReleaseMouseButtonTitlebar() {
  panel_browser_view_->OnTitlebarMouseReleased();
}

void NativePanelTestingWin::DragTitlebar(int delta_x, int delta_y) {
  // TODO(jianli): Need a comment here that explains why we use
  // mouse_pressed_point_ and not current bounds as obtained by
  // GetRestoredBounds().
  panel_browser_view_->OnTitlebarMouseDragged(gfx::Point(
      panel_browser_view_->mouse_pressed_point_.x() + delta_x,
      panel_browser_view_->mouse_pressed_point_.y() + delta_y));
}

void NativePanelTestingWin::CancelDragTitlebar() {
  panel_browser_view_->OnTitlebarMouseCaptureLost();
}

void NativePanelTestingWin::FinishDragTitlebar() {
  panel_browser_view_->OnTitlebarMouseReleased();
}
