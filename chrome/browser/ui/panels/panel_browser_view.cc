// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_view.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"

namespace {
// This value is experimental and subjective.
const int kSetBoundsAnimationMs = 180;

// The threshold to differentiate the short click and long click.
const base::TimeDelta kShortClickThresholdMs =
    base::TimeDelta::FromMilliseconds(200);

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
    closed_(false),
    focused_(false),
    mouse_pressed_(false),
    mouse_dragging_state_(NO_DRAGGING),
    is_drawing_attention_(false),
    old_focused_view_(NULL) {
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
}

bool PanelBrowserView::CanResize() const {
  return false;
}

bool PanelBrowserView::CanMaximize() const {
  return false;
}

void PanelBrowserView::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;
  bounds_ = bounds;

  // No animation if the panel is being dragged.
  if (mouse_dragging_state_ == DRAGGING_STARTED) {
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
  *bounds = bounds_;
  *show_state = ui::SHOW_STATE_NORMAL;
  return true;
}

void PanelBrowserView::OnWidgetActivationChanged(views::Widget* widget,
                                                 bool active) {
  ::BrowserView::OnWidgetActivationChanged(widget, active);

#if defined(USE_AURA)
  // TODO(beng):
  NOTIMPLEMENTED();
  bool focused = active;
#elif defined(OS_WIN)
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

  if (focused_) {
    // Expand the panel if needed.
    if (panel_->expansion_state() == Panel::MINIMIZED)
      panel_->SetExpansionState(Panel::EXPANDED);

    // Clear the attention state if needed.
    if (is_drawing_attention_)
      StopDrawingAttention();
  }

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      Source<Panel>(panel()),
      NotificationService::NoDetails());
}

bool PanelBrowserView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (mouse_pressed_ && accelerator.key_code() == ui::VKEY_ESCAPE) {
    OnTitlebarMouseCaptureLost();
    return true;
  }

  // No other accelerator is allowed when the drag begins.
  if (mouse_dragging_state_ == DRAGGING_STARTED)
    return true;

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

void PanelBrowserView::UpdatePanelLoadingAnimations(bool should_animate) {
  UpdateLoadingAnimations(should_animate);
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

void PanelBrowserView::PanelTabContentsFocused(TabContents* tab_contents) {
  TabContentsFocused(tab_contents);
}

void PanelBrowserView::PanelCut() {
  Cut();
}

void PanelBrowserView::PanelCopy() {
  Copy();
}

void PanelBrowserView::PanelPaste() {
  Paste();
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

  // Restore the panel.
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

gfx::Size PanelBrowserView::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  gfx::Size frame = GetFrameView()->NonClientAreaSize();
  return gfx::Size(content_size.width() + frame.width(),
                   content_size.height() + frame.height());
}

gfx::Size PanelBrowserView::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  gfx::Size frame = GetFrameView()->NonClientAreaSize();
  return gfx::Size(window_size.width() - frame.width(),
                   window_size.height() - frame.height());
}

int PanelBrowserView::TitleOnlyHeight() const {
  return GetFrameView()->NonClientTopBorderHeight();
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
  mouse_pressed_time_ = base::TimeTicks::Now();
  mouse_dragging_state_ = NO_DRAGGING;
  return true;
}

bool PanelBrowserView::OnTitlebarMouseDragged(const gfx::Point& location) {
  if (!mouse_pressed_)
    return false;

  int delta_x = location.x() - mouse_pressed_point_.x();
  int delta_y = location.y() - mouse_pressed_point_.y();
  if (mouse_dragging_state_ == NO_DRAGGING &&
      ExceededDragThreshold(delta_x, delta_y)) {
    // When a drag begins, we do not want to the client area to still receive
    // the focus.
    old_focused_view_ = GetFocusManager()->GetFocusedView();
    GetFocusManager()->SetFocusedView(GetFrameView());

    panel_->manager()->StartDragging(panel_.get());
    mouse_dragging_state_ = DRAGGING_STARTED;
  }
  if (mouse_dragging_state_ == DRAGGING_STARTED)
    panel_->manager()->Drag(delta_x);
  return true;
}

bool PanelBrowserView::OnTitlebarMouseReleased() {
  if (mouse_dragging_state_ == DRAGGING_STARTED) {
    // When a drag ends, restore the focus.
    if (old_focused_view_) {
      GetFocusManager()->SetFocusedView(old_focused_view_);
      old_focused_view_ = NULL;
    }

    return EndDragging(false);
  }

  // If the panel drag was cancelled before the mouse is released, do not treat
  // this as a click.
  if (mouse_dragging_state_ != NO_DRAGGING)
    return true;

  // Do not minimize the panel when we just clear the attention state. This is
  // a hack to prevent the panel from being minimized when the user clicks on
  // the title-bar to clear the attention.
  if (panel_->expansion_state() == Panel::EXPANDED &&
      base::TimeTicks::Now() < attention_cleared_time_ +
                               kSuspendMinimizeOnClickIntervalMs) {
    return true;
  }

  // Do not minimize the panel if it is long click.
  if (base::TimeTicks::Now() - mouse_pressed_time_ > kShortClickThresholdMs)
    return true;

  Panel::ExpansionState new_expansion_state =
    (panel_->expansion_state() != Panel::EXPANDED) ? Panel::EXPANDED
                                                   : Panel::MINIMIZED;
  panel_->SetExpansionState(new_expansion_state);
  return true;
}

bool PanelBrowserView::OnTitlebarMouseCaptureLost() {
  if (mouse_dragging_state_ == DRAGGING_STARTED)
    return EndDragging(true);
  return true;
}

bool PanelBrowserView::EndDragging(bool cancelled) {
  // Only handle clicks that started in our window.
  if (!mouse_pressed_)
    return false;
  mouse_pressed_ = false;

  mouse_dragging_state_ = DRAGGING_ENDED;
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
  virtual bool VerifyDrawingAttention() const OVERRIDE;


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

bool NativePanelTestingWin::VerifyDrawingAttention() const {
  PanelBrowserFrameView* frame_view = panel_browser_view_->GetFrameView();
  SkColor attention_color = frame_view->GetTitleColor(
      PanelBrowserFrameView::PAINT_FOR_ATTENTION);
  return attention_color == frame_view->title_label_->GetColor();
}
