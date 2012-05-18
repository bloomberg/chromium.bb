// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_view.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/native_window_notification_source.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;

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
  if (!panel_->manager()->display_settings_provider()->is_full_screen()) {
    // TODO(prasadt): Implement this code.
    // HideThePanel.
  }

  BrowserView::Init();

  GetWidget()->non_client_view()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  registrar_.Add(
      this,
      chrome::NOTIFICATION_WINDOW_CLOSED,
      content::Source<gfx::NativeWindow>(frame()->GetNativeWindow()));
}

void PanelBrowserView::Show() {
  BrowserView::Show();
}

void PanelBrowserView::ShowInactive() {
  BrowserView::ShowInactive();
}

void PanelBrowserView::Close() {
  GetWidget()->RemoveObserver(this);
  closed_ = true;

  // Cancel any currently running animation since we're closing down.
  if (bounds_animator_.get())
    bounds_animator_.reset();

  ::BrowserView::Close();
}

void PanelBrowserView::Deactivate() {
  if (!IsActive())
    return;

#if defined(OS_WIN) && !defined(USE_AURA)
  gfx::NativeWindow native_window = NULL;
  BrowserWindow* browser_window =
      panel_->manager()->GetNextBrowserWindowToActivate(panel_.get());
  if (browser_window)
    native_window = browser_window->GetNativeHandle();
  else
    native_window = ::GetDesktopWindow();
  if (native_window)
    ::SetForegroundWindow(native_window);
  else
    ::SetFocus(NULL);
#else
  NOTIMPLEMENTED();
  BrowserView::Deactivate();
#endif
}

bool PanelBrowserView::CanResize() const {
  return true;
}

bool PanelBrowserView::CanMaximize() const {
  return false;
}

void PanelBrowserView::SetBounds(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, true);
}

void PanelBrowserView::SetBoundsInternal(const gfx::Rect& new_bounds,
                                         bool animate) {
  if (bounds_ == new_bounds)
    return;

  bounds_ = new_bounds;

  if (!animate) {
    // If no animation is in progress, apply bounds change instantly. Otherwise,
    // continue the animation with new target bounds.
    if (!IsAnimatingBounds())
      ::BrowserView::SetBounds(new_bounds);
    return;
  }

  animation_start_bounds_ = GetBounds();

  bounds_animator_.reset(new PanelBoundsAnimation(
      this, panel(), animation_start_bounds_, new_bounds));
  bounds_animator_->Start();
}

void PanelBrowserView::UpdateTitleBar() {
  ::BrowserView::UpdateTitleBar();
  GetFrameView()->UpdateTitleBar();
}

bool PanelBrowserView::IsPanel() const {
  return true;
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

#if defined(OS_WIN) && !defined(USE_AURA)
  // The panel window is in focus (actually accepting keystrokes) if it is
  // active and belongs to a foreground application.
  bool focused = active &&
      GetFrameView()->GetWidget()->GetNativeView() == ::GetForegroundWindow();
#else
  NOTIMPLEMENTED();
  bool focused = active;
#endif

  if (focused_ == focused)
    return;
  focused_ = focused;

  panel()->OnActiveStateChanged(focused);
}

bool PanelBrowserView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (mouse_pressed_ && accelerator.key_code() == ui::VKEY_ESCAPE) {
    OnTitlebarMouseCaptureLost();
    return true;
  }

  // No other accelerator is allowed when the drag begins.
  if (mouse_dragging_state_ == DRAGGING_STARTED)
    return true;

  return BrowserView::AcceleratorPressed(accelerator);
}

void PanelBrowserView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_WINDOW_CLOSED:
      panel_->OnNativePanelClosed();
      break;
    default:
      NOTREACHED();
  }
}

void PanelBrowserView::AnimationEnded(const ui::Animation* animation) {
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelBrowserView::AnimationProgressed(const ui::Animation* animation) {
  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);

#if defined(OS_WIN) && !defined(USE_AURA)
  // When the panel's height falls below the titlebar height, do not show
  // thick frame since otherwise Windows will add extra size to the layout
  // computation.
  bool use_thick_frame = !GetFrameView()->IsShowingTitlebarOnly();
  UpdateWindowAttribute(GWL_STYLE, WS_THICKFRAME, use_thick_frame);
#endif

  ::BrowserView::SetBounds(new_bounds);
}

void PanelBrowserView::OnDisplayChanged() {
  BrowserView::OnDisplayChanged();
  panel_->manager()->display_settings_provider()->OnDisplaySettingsChanged();
}

void PanelBrowserView::OnWorkAreaChanged() {
  BrowserView::OnWorkAreaChanged();
  panel_->manager()->display_settings_provider()->OnDisplaySettingsChanged();
}

bool PanelBrowserView::WillProcessWorkAreaChange() const {
  return true;
}

void PanelBrowserView::OnWindowBeginUserBoundsChange() {
  panel_->OnPanelStartUserResizing();
}

void PanelBrowserView::OnWindowEndUserBoundsChange() {
  bounds_ = GetBounds();
  panel_->IncreaseMaxSize(bounds_.size());
  panel_->set_full_size(bounds_.size());

  panel_->OnPanelEndUserResizing();
  panel_->panel_strip()->RefreshLayout();
}

void PanelBrowserView::ShowPanel() {
  Show();
  // No animation is used for initial creation of a panel on Win.
  // Signal immediately that pending actions can be performed.
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelBrowserView::ShowPanelInactive() {
  ShowInactive();
  // No animation is used for initial creation of a panel on Win.
  // Signal immediately that pending actions can be performed.
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

gfx::Rect PanelBrowserView::GetPanelBounds() const {
  return bounds_;
}

void PanelBrowserView::SetPanelBounds(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, true);
}

void PanelBrowserView::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, false);
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

void PanelBrowserView::PreventActivationByOS(bool prevent_activation) {
#if defined(OS_WIN) && !defined(USE_AURA)
  // Set the flags "NoActivate" and "AppWindow" to make sure
  // the minimized panels do not get activated by the OS, but
  // do appear in the taskbar and Alt-Tab menu.
  UpdateWindowAttribute(GWL_EXSTYLE,
                        WS_EX_NOACTIVATE | WS_EX_APPWINDOW,
                        prevent_activation);
#endif
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

void PanelBrowserView::PanelWebContentsFocused(WebContents* contents) {
  WebContentsFocused(contents);
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

void PanelBrowserView::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  if (is_drawing_attention_ == draw_attention)
    return;
  is_drawing_attention_ = draw_attention;
  GetFrameView()->SchedulePaint();

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0)
    ::BrowserView::FlashFrame(draw_attention);
}

bool PanelBrowserView::IsDrawingAttention() const {
  return is_drawing_attention_;
}

bool PanelBrowserView::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void PanelBrowserView::FullScreenModeChanged(bool is_full_screen) {
  if (is_full_screen) {
    if (frame()->IsVisible()) {
        frame()->Hide();
    }
  } else {
    ShowInactive();
  }
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

void PanelBrowserView::EnsurePanelFullyVisible() {
#if defined(OS_WIN) && !defined(USE_AURA)
  ::SetWindowPos(GetNativeHandle(), HWND_TOP, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
#else
  NOTIMPLEMENTED();
#endif
}

PanelBrowserFrameView* PanelBrowserView::GetFrameView() const {
  return static_cast<PanelBrowserFrameView*>(frame()->GetFrameView());
}

bool PanelBrowserView::OnTitlebarMousePressed(
    const gfx::Point& mouse_location) {
  mouse_pressed_ = true;
  mouse_dragging_state_ = NO_DRAGGING;
  last_mouse_location_ = mouse_location;
  return true;
}

bool PanelBrowserView::OnTitlebarMouseDragged(
    const gfx::Point& mouse_location) {
  if (!mouse_pressed_)
    return false;

  int delta_x = mouse_location.x() - last_mouse_location_.x();
  int delta_y = mouse_location.y() - last_mouse_location_.y();
  if (mouse_dragging_state_ == NO_DRAGGING &&
      ExceededDragThreshold(delta_x, delta_y)) {
    // When a drag begins, we do not want to the client area to still receive
    // the focus.
    old_focused_view_ = GetFocusManager()->GetFocusedView();
    GetFocusManager()->SetFocusedView(GetFrameView());

    panel_->manager()->StartDragging(panel_.get(), last_mouse_location_);
    mouse_dragging_state_ = DRAGGING_STARTED;
  }
  if (mouse_dragging_state_ == DRAGGING_STARTED) {
    panel_->manager()->Drag(mouse_location);

    // Once in drag, update |last_mouse_location_| on each drag fragment, since
    // we already dragged the panel up to the current mouse location.
    last_mouse_location_ = mouse_location;
  }
  return true;
}

bool PanelBrowserView::OnTitlebarMouseReleased(panel::ClickModifier modifier) {
  if (mouse_dragging_state_ != NO_DRAGGING) {
    // Ensure dragging a minimized panel does not leave it activated.
    // Windows activates a panel on mouse-down, regardless of our attempts
    // to prevent activation of a minimized panel. Now that we know mouse-down
    // resulted in a mouse-drag, we need to ensure the minimized panel is
    // deactivated.
    if (panel_->IsMinimized() && panel_->IsActive())
      panel_->Deactivate();

    if (mouse_dragging_state_ == DRAGGING_STARTED) {
      // When a drag ends, restore the focus.
      if (old_focused_view_) {
        GetFocusManager()->SetFocusedView(old_focused_view_);
        old_focused_view_ = NULL;
      }
      return EndDragging(false);
    }

    // Else, the panel drag was cancelled before the mouse is released. Do not
    // treat this as a click.
    if (mouse_dragging_state_ != NO_DRAGGING)
      return true;
  }

  panel_->OnTitlebarClicked(modifier);
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

void PanelBrowserView::SetPanelAlwaysOnTop(bool on_top) {
  GetWidget()->SetAlwaysOnTop(on_top);
  GetWidget()->non_client_view()->Layout();
  GetWidget()->client_view()->Layout();
}

bool PanelBrowserView::IsAnimatingBounds() const {
  return bounds_animator_.get() && bounds_animator_->is_animating();
}

void PanelBrowserView::EnableResizeByMouse(bool enable) {
}

void PanelBrowserView::UpdatePanelMinimizeRestoreButtonVisibility() {
  GetFrameView()->UpdateTitleBarMinimizeRestoreButtonVisibility();
}


#if defined(OS_WIN) && !defined(USE_AURA)
void PanelBrowserView::UpdateWindowAttribute(int attribute_index,
                                             int attribute_value,
                                             bool to_set) {
  gfx::NativeWindow native_window = GetNativePanelHandle();
  int value = ::GetWindowLong(native_window, attribute_index);
  int expected_value;
  if (to_set)
    expected_value = value |  attribute_value;
  else
    expected_value = value &  ~attribute_value;
  if (value != expected_value)
    ::SetWindowLong(native_window, attribute_index, expected_value);
}
#endif

// NativePanelTesting implementation.
class NativePanelTestingWin : public NativePanelTesting {
 public:
  explicit NativePanelTestingWin(PanelBrowserView* panel_browser_view);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& mouse_location, panel::ClickModifier modifier) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar(
      panel::ClickModifier modifier) OVERRIDE;
  virtual void DragTitlebar(const gfx::Point& mouse_location) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;
  virtual bool VerifyDrawingAttention() const OVERRIDE;
  virtual bool VerifyActiveState(bool is_active) OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;
  virtual bool IsButtonVisible(TitlebarButtonType button_type) const OVERRIDE;

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
  PanelBrowserFrameView* frame_view = panel_browser_view_->GetFrameView();
  frame_view->title_label_->SetAutoColorReadabilityEnabled(false);
}

void NativePanelTestingWin::PressLeftMouseButtonTitlebar(
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {
  panel_browser_view_->OnTitlebarMousePressed(mouse_location);
}

void NativePanelTestingWin::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  panel_browser_view_->OnTitlebarMouseReleased(modifier);
}

void NativePanelTestingWin::DragTitlebar(const gfx::Point& mouse_location) {
  panel_browser_view_->OnTitlebarMouseDragged(mouse_location);
}

void NativePanelTestingWin::CancelDragTitlebar() {
  panel_browser_view_->OnTitlebarMouseCaptureLost();
}

void NativePanelTestingWin::FinishDragTitlebar() {
  panel_browser_view_->OnTitlebarMouseReleased(panel::NO_MODIFIER);
}

bool NativePanelTestingWin::VerifyDrawingAttention() const {
  PanelBrowserFrameView* frame_view = panel_browser_view_->GetFrameView();
  SkColor attention_color = frame_view->GetTitleColor(
      PanelBrowserFrameView::PAINT_FOR_ATTENTION);
  return attention_color == frame_view->title_label_->enabled_color();
}

bool NativePanelTestingWin::VerifyActiveState(bool is_active) {
  PanelBrowserFrameView* frame_view = panel_browser_view_->GetFrameView();

  PanelBrowserFrameView::PaintState expected_paint_state =
      is_active ? PanelBrowserFrameView::PAINT_AS_ACTIVE
                : PanelBrowserFrameView::PAINT_AS_INACTIVE;
  if (frame_view->paint_state_ != expected_paint_state)
    return false;

  SkColor expected_color = frame_view->GetTitleColor(expected_paint_state);
  return expected_color == frame_view->title_label_->enabled_color();
}

bool NativePanelTestingWin::IsWindowSizeKnown() const {
  return true;
}

bool NativePanelTestingWin::IsAnimatingBounds() const {
  return panel_browser_view_->IsAnimatingBounds();
}

bool NativePanelTestingWin::IsButtonVisible(
    TitlebarButtonType button_type) const {
  PanelBrowserFrameView* frame_view = panel_browser_view_->GetFrameView();

  switch (button_type) {
    case CLOSE_BUTTON:
      return frame_view->close_button_->visible();
    case MINIMIZE_BUTTON:
      return frame_view->minimize_button_->visible();
    case RESTORE_BUTTON:
      return frame_view->restore_button_->visible();
    default:
      NOTREACHED();
  }
  return false;
}
