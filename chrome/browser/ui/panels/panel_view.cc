// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_view.h"

#include <map>
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_frame_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN) && !defined(USE_ASH) && !defined(USE_AURA)
#include "ui/base/win/shell.h"
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/panels/taskbar_window_thumbnailer_win.h"
#endif

namespace {

// Supported accelerators.
// Note: We can't use the acclerator table defined in chrome/browser/ui/views
// due to checkdeps violation.
struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};
const AcceleratorMapping kPanelAcceleratorMap[] = {
  { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_R, ui::EF_CONTROL_DOWN, IDC_RELOAD },
  { ui::VKEY_F5, ui::EF_NONE, IDC_RELOAD },
  { ui::VKEY_R, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
      IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_F5, ui::EF_CONTROL_DOWN, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_F5, ui::EF_SHIFT_DOWN, IDC_RELOAD_IGNORING_CACHE },
  { ui::VKEY_ESCAPE, ui::EF_NONE, IDC_STOP },
  { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
};

const std::map<ui::Accelerator, int>& GetAcceleratorTable() {
  static std::map<ui::Accelerator, int>* accelerators = NULL;
  if (!accelerators) {
    accelerators = new std::map<ui::Accelerator, int>();
    for (size_t i = 0; i < arraysize(kPanelAcceleratorMap); ++i) {
      ui::Accelerator accelerator(kPanelAcceleratorMap[i].keycode,
                                  kPanelAcceleratorMap[i].modifiers);
      (*accelerators)[accelerator] = kPanelAcceleratorMap[i].command_id;
    }
  }
  return *accelerators;
}

// NativePanelTesting implementation.
class NativePanelTestingWin : public NativePanelTesting {
 public:
  explicit NativePanelTestingWin(PanelView* panel_view);

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
  virtual bool IsButtonVisible(
      panel::TitlebarButtonType button_type) const OVERRIDE;

  PanelView* panel_view_;
};

NativePanelTestingWin::NativePanelTestingWin(PanelView* panel_view)
    : panel_view_(panel_view) {
}

void NativePanelTestingWin::PressLeftMouseButtonTitlebar(
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {
  panel_view_->OnTitlebarMousePressed(mouse_location);
}

void NativePanelTestingWin::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  panel_view_->OnTitlebarMouseReleased(modifier);
}

void NativePanelTestingWin::DragTitlebar(const gfx::Point& mouse_location) {
  panel_view_->OnTitlebarMouseDragged(mouse_location);
}

void NativePanelTestingWin::CancelDragTitlebar() {
  panel_view_->OnTitlebarMouseCaptureLost();
}

void NativePanelTestingWin::FinishDragTitlebar() {
  panel_view_->OnTitlebarMouseReleased(panel::NO_MODIFIER);
}

bool NativePanelTestingWin::VerifyDrawingAttention() const {
  return panel_view_->GetFrameView()->paint_state() ==
         PanelFrameView::PAINT_FOR_ATTENTION;
}

bool NativePanelTestingWin::VerifyActiveState(bool is_active) {
  return panel_view_->GetFrameView()->paint_state() ==
         (is_active ? PanelFrameView::PAINT_AS_ACTIVE
                    : PanelFrameView::PAINT_AS_INACTIVE);
}

bool NativePanelTestingWin::IsWindowSizeKnown() const {
  return true;
}

bool NativePanelTestingWin::IsAnimatingBounds() const {
  return panel_view_->IsAnimatingBounds();
}

bool NativePanelTestingWin::IsButtonVisible(
    panel::TitlebarButtonType button_type) const {
  PanelFrameView* frame_view = panel_view_->GetFrameView();

  switch (button_type) {
    case panel::CLOSE_BUTTON:
      return frame_view->close_button()->visible();
    case panel::MINIMIZE_BUTTON:
      return frame_view->minimize_button()->visible();
    case panel::RESTORE_BUTTON:
      return frame_view->restore_button()->visible();
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace

// static
NativePanel* Panel::CreateNativePanel(Panel* panel, const gfx::Rect& bounds) {
  return new PanelView(panel, bounds);
}

PanelView::PanelView(Panel* panel, const gfx::Rect& bounds)
    : panel_(panel),
      bounds_(bounds),
      window_(NULL),
      web_view_(NULL),
      focused_(false),
      mouse_pressed_(false),
      mouse_dragging_state_(NO_DRAGGING),
      is_drawing_attention_(false),
      force_to_paint_as_inactive_(false),
      old_focused_view_(NULL) {
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_PANEL);
  params.delegate = this;
  params.remove_standard_frame = true;
  params.keep_on_top = true;
  params.bounds = bounds;
  window_->Init(params);
  window_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
  window_->set_focus_on_creation(false);
  window_->AddObserver(this);

  web_view_ = new views::WebView(NULL);
  AddChildView(web_view_);

  OnViewWasResized();

  // Register accelarators supported by panels.
  views::FocusManager* focus_manager = GetFocusManager();
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  for (std::map<ui::Accelerator, int>::const_iterator iter =
           accelerator_table.begin();
       iter != accelerator_table.end(); ++iter) {
    focus_manager->RegisterAccelerator(
        iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }

#if defined(OS_WIN) && !defined(USE_ASH) && !defined(USE_AURA)
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppModelIdForProfile(UTF8ToWide(panel->app_name()),
                                                panel->profile()->GetPath()),
      window_->GetNativeWindow());
#endif
}

PanelView::~PanelView() {
  web_view_->SetWebContents(NULL);
}

void PanelView::ShowPanel() {
  ShowPanelInactive();
  ActivatePanel();
}

void PanelView::ShowPanelInactive() {
  if (window_->IsVisible())
    return;
  window_->ShowInactive();
  // No animation is used for initial creation of a panel on Win.
  // Signal immediately that pending actions can be performed.
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

gfx::Rect PanelView::GetPanelBounds() const {
  return bounds_;
}

void PanelView::SetPanelBounds(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, true);
}

void PanelView::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, false);
}

void PanelView::SetBoundsInternal(const gfx::Rect& new_bounds, bool animate) {
  if (bounds_ == new_bounds)
    return;

  bounds_ = new_bounds;

  if (!animate) {
    // If no animation is in progress, apply bounds change instantly. Otherwise,
    // continue the animation with new target bounds.
    if (!IsAnimatingBounds())
      GetWidget()->SetBounds(bounds_);
    return;
  }

  animation_start_bounds_ = window_->GetWindowBoundsInScreen();

  bounds_animator_.reset(new PanelBoundsAnimation(
      this, panel_.get(), animation_start_bounds_, new_bounds));
  bounds_animator_->Start();
}

void PanelView::AnimationEnded(const ui::Animation* animation) {
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelView::AnimationProgressed(const ui::Animation* animation) {
  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);
  GetWidget()->SetBounds(new_bounds);
}

void PanelView::ClosePanel() {
  // Cancel any currently running animation since we're closing down.
  if (bounds_animator_.get())
    bounds_animator_.reset();

  panel_->OnNativePanelClosed();
  window_->Close();
}

void PanelView::ActivatePanel() {
  window_->Activate();
}

void PanelView::DeactivatePanel() {
  window_->Deactivate();
}

bool PanelView::IsPanelActive() const {
  return focused_;
}

void PanelView::PreventActivationByOS(bool prevent_activation) {
#if defined(OS_WIN) && !defined(USE_ASH) && !defined(USE_AURA)
  // Set the flags "NoActivate" and "AppWindow" to make sure
  // the minimized panels do not get activated by the OS, but
  // do appear in the taskbar and Alt-Tab menu.
  UpdateWindowAttribute(GWL_EXSTYLE,
                        WS_EX_NOACTIVATE | WS_EX_APPWINDOW,
                        prevent_activation);
#endif
}

gfx::NativeWindow PanelView::GetNativePanelHandle() {
  return window_->GetNativeWindow();
}

void PanelView::UpdatePanelTitleBar() {
  UpdateWindowTitle();
  UpdateWindowIcon();
}

void PanelView::UpdatePanelLoadingAnimations(bool should_animate) {
  GetFrameView()->UpdateThrobber();
}

void PanelView::NotifyPanelOnUserChangedTheme() {
  GetFrameView()->SchedulePaint();
}

void PanelView::PanelWebContentsFocused(content::WebContents* contents) {
  web_view_->OnWebContentsFocused(contents);
}

void PanelView::PanelCut() {
  // Nothing to do since we do not have panel-specific system menu.
  NOTREACHED();
}

void PanelView::PanelCopy() {
  // Nothing to do since we do not have panel-specific system menu.
  NOTREACHED();
}

void PanelView::PanelPaste() {
  // Nothing to do since we do not have panel-specific system menu.
  NOTREACHED();
}

void PanelView::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  if (is_drawing_attention_ == draw_attention)
    return;
  is_drawing_attention_ = draw_attention;
  GetFrameView()->SchedulePaint();

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0)
    window_->FlashFrame(draw_attention);
}

bool PanelView::IsDrawingAttention() const {
  return is_drawing_attention_;
}

void PanelView::HandlePanelKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager->shortcut_handling_suspended())
    return;

  ui::Accelerator accelerator(
      static_cast<ui::KeyboardCode>(event.windowsKeyCode),
      content::GetModifiersFromNativeWebKeyboardEvent(event));
  if (event.type == WebKit::WebInputEvent::KeyUp)
    accelerator.set_type(ui::ET_KEY_RELEASED);
  focus_manager->ProcessAccelerator(accelerator);
}

void PanelView::FullScreenModeChanged(bool is_full_screen) {
  if (is_full_screen) {
    if (window_->IsVisible())
      window_->Hide();
  } else {
    ShowPanelInactive();
  }
}

void PanelView::SetPanelAlwaysOnTop(bool on_top) {
  window_->SetAlwaysOnTop(on_top);
  window_->non_client_view()->Layout();
  window_->client_view()->Layout();
}

void PanelView::EnableResizeByMouse(bool enable) {
  // Nothing to do since we use system resizing.
}

void PanelView::UpdatePanelMinimizeRestoreButtonVisibility() {
  GetFrameView()->UpdateTitlebarMinimizeRestoreButtonVisibility();
}

void PanelView::PanelExpansionStateChanging(Panel::ExpansionState old_state,
                                            Panel::ExpansionState new_state) {
#if defined(OS_WIN) && !defined(USE_ASH) && !defined(USE_AURA)
  // Live preview is only available since Windows 7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  bool is_minimized = old_state != Panel::EXPANDED;
  bool will_be_minimized = new_state != Panel::EXPANDED;
  if (is_minimized == will_be_minimized)
    return;

  HWND native_window = window_->GetNativeWindow();

  if (!thumbnailer_.get()) {
    DCHECK(native_window);
    thumbnailer_.reset(new TaskbarWindowThumbnailerWin(native_window));
    ui::HWNDSubclass::AddFilterToTarget(native_window, thumbnailer_.get());
  }

  // Cache the image at this point.
  if (will_be_minimized) {
    // If the panel is still active (we will deactivate the minimizd panel at
    // later time), we need to paint it immediately as inactive so that we can
    // take a snapshot of inactive panel.
    if (focused_) {
      force_to_paint_as_inactive_ = true;
      ::RedrawWindow(native_window, NULL, NULL,
                     RDW_NOCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW);
    }

    thumbnailer_->Start();
  } else {
    force_to_paint_as_inactive_ = false;
    thumbnailer_->Stop();
  }

#endif
}

gfx::Size PanelView::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  gfx::Size frame = GetFrameView()->NonClientAreaSize();
  return gfx::Size(content_size.width() + frame.width(),
                   content_size.height() + frame.height());
}

gfx::Size PanelView::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  gfx::Size frame = GetFrameView()->NonClientAreaSize();
  return gfx::Size(window_size.width() - frame.width(),
                   window_size.height() - frame.height());
}

int PanelView::TitleOnlyHeight() const {
  return panel::kTitlebarHeight;
}

void PanelView::AttachWebContents(content::WebContents* contents) {
  web_view_->SetWebContents(contents);
}

void PanelView::DetachWebContents(content::WebContents* contents) {
  web_view_->SetWebContents(NULL);
}

NativePanelTesting* PanelView::CreateNativePanelTesting() {
  return new NativePanelTestingWin(this);
}

void PanelView::OnDisplayChanged() {
  panel_->manager()->display_settings_provider()->OnDisplaySettingsChanged();
}

void PanelView::OnWorkAreaChanged() {
  panel_->manager()->display_settings_provider()->OnDisplaySettingsChanged();
}

bool PanelView::WillProcessWorkAreaChange() const {
  return true;
}

views::View* PanelView::GetContentsView() {
  return this;
}

views::NonClientFrameView* PanelView::CreateNonClientFrameView(
    views::Widget* widget) {
  PanelFrameView* frame_view = new PanelFrameView(this);
  frame_view->Init();
  return frame_view;
}

bool PanelView::CanResize() const {
  return true;
}

bool PanelView::CanMaximize() const {
  return false;
}

string16 PanelView::GetWindowTitle() const {
  return panel_->GetWindowTitle();
}

gfx::ImageSkia PanelView::GetWindowIcon() {
  gfx::Image icon = panel_->GetCurrentPageIcon();
  return icon.IsEmpty() ? gfx::ImageSkia() : *icon.ToImageSkia();
}

void PanelView::DeleteDelegate() {
  delete this;
}

void PanelView::OnWindowBeginUserBoundsChange() {
  panel_->OnPanelStartUserResizing();
}

void PanelView::OnWindowEndUserBoundsChange() {
  panel_->OnPanelEndUserResizing();

  // No need to proceed with post-resizing update when there is no size change.
  gfx::Rect new_bounds = window_->GetWindowBoundsInScreen();
  if (bounds_ == new_bounds)
    return;
  bounds_ = new_bounds;

  panel_->IncreaseMaxSize(bounds_.size());
  panel_->set_full_size(bounds_.size());

  panel_->panel_strip()->RefreshLayout();
}

views::Widget* PanelView::GetWidget() {
  return window_;
}

const views::Widget* PanelView::GetWidget() const {
  return window_;
}

void PanelView::UpdateLoadingAnimations(bool should_animate) {
  GetFrameView()->UpdateThrobber();
}

void PanelView::UpdateWindowTitle() {
  window_->UpdateWindowTitle();
  GetFrameView()->UpdateTitle();
}

void PanelView::UpdateWindowIcon() {
  window_->UpdateWindowIcon();
  GetFrameView()->UpdateIcon();
}

void PanelView::Layout() {
  // |web_view_| might not be created yet when the window is first created.
  if (web_view_)
    web_view_->SetBounds(0, 0, width(), height());
  OnViewWasResized();
}

gfx::Size PanelView::GetMinimumSize() {
  return gfx::Size();
}

gfx::Size PanelView::GetMaximumSize() {
  return gfx::Size();
}

bool PanelView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (mouse_pressed_ && accelerator.key_code() == ui::VKEY_ESCAPE) {
    OnTitlebarMouseCaptureLost();
    return true;
  }

  // No other accelerator is allowed when the drag begins.
  if (mouse_dragging_state_ == DRAGGING_STARTED)
    return true;

  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table.find(accelerator);
  DCHECK(iter != accelerator_table.end());
  return panel_->ExecuteCommandIfEnabled(iter->second);
}

void PanelView::OnWidgetActivationChanged(views::Widget* widget, bool active) {
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

  // Expand the panel if the minimized panel is activated by means other than
  // clicking on its titlebar. This is the workaround to support restoring the
  // minimized panel by other means, like alt-tabbing, win-tabbing, or clicking
  // the taskbar icon. Note that this workaround does not work for one edge
  // case: the mouse happens to be at the minimized panel when the user tries to
  // bring up the panel with the above alternatives.
  // When the user clicks on the minimized panel, the panel expansion will be
  // done when we process the mouse button pressed message.
  if (focused_ && panel_->IsMinimized() &&
      gfx::Screen::GetWindowAtCursorScreenPoint() !=
          widget->GetNativeWindow()) {
    panel_->Restore();
  }

  panel()->OnActiveStateChanged(focused);
}

bool PanelView::OnTitlebarMousePressed(const gfx::Point& mouse_location) {
  mouse_pressed_ = true;
  mouse_dragging_state_ = NO_DRAGGING;
  last_mouse_location_ = mouse_location;
  return true;
}

bool PanelView::OnTitlebarMouseDragged(const gfx::Point& mouse_location) {
  if (!mouse_pressed_)
    return false;

  int delta_x = mouse_location.x() - last_mouse_location_.x();
  int delta_y = mouse_location.y() - last_mouse_location_.y();
  if (mouse_dragging_state_ == NO_DRAGGING &&
      ExceededDragThreshold(delta_x, delta_y)) {
    // When a drag begins, we do not want to the client area to still receive
    // the focus. We do not need to do this for the unfocused minimized panel.
    if (!panel_->IsMinimized()) {
      old_focused_view_ = GetFocusManager()->GetFocusedView();
      GetFocusManager()->SetFocusedView(GetFrameView());
    }

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

bool PanelView::OnTitlebarMouseReleased(panel::ClickModifier modifier) {
  if (mouse_dragging_state_ != NO_DRAGGING) {
    // Ensure dragging a minimized panel does not leave it activated.
    // Windows activates a panel on mouse-down, regardless of our attempts
    // to prevent activation of a minimized panel. Now that we know mouse-down
    // resulted in a mouse-drag, we need to ensure the minimized panel is
    // deactivated.
    if (panel_->IsMinimized() && focused_)
      panel_->Deactivate();

    if (mouse_dragging_state_ == DRAGGING_STARTED) {
      // When a drag ends, restore the focus.
      if (old_focused_view_) {
        GetFocusManager()->SetFocusedView(old_focused_view_);
        old_focused_view_ = NULL;
      }
      return EndDragging(false);
    }

    // The panel drag was cancelled before the mouse is released. Do not
    // treat this as a click.
    return true;
  }

  panel_->OnTitlebarClicked(modifier);
  return true;
}

bool PanelView::OnTitlebarMouseCaptureLost() {
  if (mouse_dragging_state_ == DRAGGING_STARTED)
    return EndDragging(true);
  return true;
}

bool PanelView::EndDragging(bool cancelled) {
  // Only handle clicks that started in our window.
  if (!mouse_pressed_)
    return false;
  mouse_pressed_ = false;

  mouse_dragging_state_ = DRAGGING_ENDED;
  panel_->manager()->EndDragging(cancelled);
  return true;
}

PanelFrameView* PanelView::GetFrameView() const {
  return static_cast<PanelFrameView*>(window_->non_client_view()->frame_view());
}

bool PanelView::IsAnimatingBounds() const {
  return bounds_animator_.get() && bounds_animator_->is_animating();
}

bool PanelView::IsWithinResizingArea(const gfx::Point& mouse_location) const {
  gfx::Rect bounds = window_->GetWindowBoundsInScreen();
  DCHECK(bounds.Contains(mouse_location));
  return mouse_location.x() < bounds.x() + kResizeInsideBoundsSize ||
         mouse_location.x() >= bounds.right() - kResizeInsideBoundsSize ||
         mouse_location.y() < bounds.y() + kResizeInsideBoundsSize ||
         mouse_location.y() >= bounds.bottom() - kResizeInsideBoundsSize;
}

#if defined(OS_WIN) && !defined(USE_ASH) && !defined(USE_AURA)
void PanelView::UpdateWindowAttribute(int attribute_index,
                                      int attribute_value,
                                      bool to_set) {
  gfx::NativeWindow native_window = window_->GetNativeWindow();
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

void PanelView::OnViewWasResized() {
#if defined(OS_WIN) && !defined(USE_ASH) && !defined(USE_AURA)
  content::WebContents* web_contents = panel_->GetWebContents();
  if (!web_view_ || !web_contents)
    return;

  // Make part of the inner area be used for mouse resizing.
  int width = web_view_->size().width();
  int height = web_view_->size().height();
  SkRegion* region = new SkRegion;
  region->op(0, 0, kResizeInsideBoundsSize, height, SkRegion::kUnion_Op);
  region->op(width - kResizeInsideBoundsSize, 0, width, height,
      SkRegion::kUnion_Op);
  region->op(0, height - kResizeInsideBoundsSize, width, height,
      SkRegion::kUnion_Op);
  web_contents->GetRenderViewHost()->GetView()->SetClickthroughRegion(region);
#endif
}
