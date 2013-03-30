// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/panel_view.h"

#include <map>
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/views/panels/panel_frame_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/panels/taskbar_window_thumbnailer_win.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/icon_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

// If the height of a stacked panel shrinks below this threshold during the
// user resizing, it will be treated as minimized.
const int kStackedPanelHeightShrinkThresholdToBecomeMinimized =
    panel::kTitlebarHeight + 20;

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
  { ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_DEV_TOOLS },
  { ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_DEV_TOOLS_CONSOLE },
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
  virtual bool VerifyAppIcon() const OVERRIDE;
  virtual bool VerifySystemMinimizeState() const OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;
  virtual bool IsButtonVisible(
      panel::TitlebarButtonType button_type) const OVERRIDE;
  virtual panel::CornerStyle GetWindowCornerStyle() const OVERRIDE;

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
  MessageLoop::current()->RunUntilIdle();
  return panel_view_->GetFrameView()->GetPaintState() ==
         PanelFrameView::PAINT_FOR_ATTENTION;
}

bool NativePanelTestingWin::VerifyActiveState(bool is_active) {
  return panel_view_->GetFrameView()->GetPaintState() ==
         (is_active ? PanelFrameView::PAINT_AS_ACTIVE
                    : PanelFrameView::PAINT_AS_INACTIVE);
}

bool NativePanelTestingWin::VerifyAppIcon() const {
#if defined(OS_WIN)
  // We only care about Windows 7 and later.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return true;

  HWND native_window = views::HWNDForWidget(panel_view_->window());
  HICON app_icon = reinterpret_cast<HICON>(
      ::SendMessage(native_window, WM_GETICON, ICON_BIG, 0L));
  if (!app_icon)
    return false;
  scoped_ptr<SkBitmap> bitmap(IconUtil::CreateSkBitmapFromHICON(app_icon));
  return bitmap.get() &&
         bitmap->width() == panel::kPanelAppIconSize &&
         bitmap->height() == panel::kPanelAppIconSize;
#else
  return true;
#endif
}

bool NativePanelTestingWin::VerifySystemMinimizeState() const {
#if defined(OS_WIN)
  HWND native_window = views::HWNDForWidget(panel_view_->window());
  WINDOWPLACEMENT placement;
  if (!::GetWindowPlacement(native_window, &placement))
    return false;
  if (placement.showCmd == SW_MINIMIZE || placement.showCmd == SW_SHOWMINIMIZED)
    return true;

  // If the panel window has owner window, as in stacked mode, check its owner
  // window. Note that owner window, instead of parent window, is returned
  // though GWL_HWNDPARENT contains 'parent'.
  HWND owner_window =
      reinterpret_cast<HWND>(::GetWindowLongPtr(native_window,
                                                GWLP_HWNDPARENT));
  if (!owner_window || !::GetWindowPlacement(owner_window, &placement))
    return false;
  return placement.showCmd == SW_MINIMIZE ||
         placement.showCmd == SW_SHOWMINIMIZED;
#else
  return true;
#endif
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

panel::CornerStyle NativePanelTestingWin::GetWindowCornerStyle() const {
  return panel_view_->GetFrameView()->corner_style();
}

}  // namespace

// static
NativePanel* Panel::CreateNativePanel(Panel* panel, const gfx::Rect& bounds) {
  return new PanelView(panel, bounds);
}

// The panel window has to be created as always-on-top. We cannot create it
// as non-always-on-top and then change it to always-on-top because Windows
// system might deny making a window always-on-top if the application is not
// a foreground application. In addition, we do not know if the panel should
// be created as always-on-top at its creation time. To solve this issue,
// always_on_top_ is default to true because we can always change from
// always-on-top to not always-on-top but not the other way around.
PanelView::PanelView(Panel* panel, const gfx::Rect& bounds)
    : panel_(panel),
      bounds_(bounds),
      window_(NULL),
      window_closed_(false),
      web_view_(NULL),
      always_on_top_(true),
      focused_(false),
      user_resizing_(false),
#if defined(OS_WIN)
      user_resizing_interior_stacked_panel_edge_(false),
#endif
      mouse_pressed_(false),
      mouse_dragging_state_(NO_DRAGGING),
      is_drawing_attention_(false),
      force_to_paint_as_inactive_(false),
      old_focused_view_(NULL) {
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
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

#if defined(OS_WIN)
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppModelIdForProfile(UTF8ToWide(panel->app_name()),
                                                panel->profile()->GetPath()),
      views::HWNDForWidget(window_));
#endif

  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

PanelView::~PanelView() {
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
      SetWidgetBounds(bounds_);
    return;
  }

  animation_start_bounds_ = window_->GetWindowBoundsInScreen();

  bounds_animator_.reset(new PanelBoundsAnimation(
      this, panel_.get(), animation_start_bounds_, new_bounds));
  bounds_animator_->Start();
}

#if defined(OS_WIN)
bool PanelView::FilterMessage(HWND hwnd,
                              UINT message,
                              WPARAM w_param,
                              LPARAM l_param,
                              LRESULT* l_result) {
  switch (message) {
    case WM_SIZING:
      if (w_param == WMSZ_BOTTOM)
        user_resizing_interior_stacked_panel_edge_ = true;
      break;
  }
  return false;
}
#endif

void PanelView::AnimationEnded(const ui::Animation* animation) {
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelView::AnimationProgressed(const ui::Animation* animation) {
  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);
  SetWidgetBounds(new_bounds);
}

void PanelView::SetWidgetBounds(const gfx::Rect& new_bounds) {
#if defined(OS_WIN)
  // An overlapped window is a top-level window that has a titlebar, border,
  // and client area. The Windows system will automatically put the shadow
  // around the whole window. Also the system will enforce the minimum height
  // (38 pixels based on observation) for the overlapped window such that it
  // will always has the space for the titlebar.
  //
  // On contrast, a popup window is a bare minimum window without border and
  // titlebar by default. It is often used for the popup menu and the window
  // with short life. The Windows system does not add the shadow around the
  // whole window though CS_DROPSHADOW class style could be passed to add the
  // drop shadow which is only around the right and bottom edges.
  //
  // The height of the title-only or minimized panel is smaller than the minimum
  // overlapped window height. If the panel still uses the overlapped window
  // style, Windows system will automatically increase the window height. To
  // work around this limitation, we temporarily change the window style to
  // popup when the height to set is smaller than the minimum overlapped window
  // height and then restore the window style to overlapped when the height
  // grows.
  static const int kMinimumOverlappedWindowHeight = 38;
  gfx::Rect old_bounds = GetWidget()->GetRestoredBounds();
  if (old_bounds.height() > kMinimumOverlappedWindowHeight &&
      new_bounds.height() <= kMinimumOverlappedWindowHeight) {
    // When the panel height shrinks below the minimum overlapped window height,
    // change the window style to popup such that we can show the title-only
    // and minimized panel without additional height being added by the system.
    UpdateWindowAttribute(GWL_STYLE,
                          WS_POPUP,
                          WS_OVERLAPPED | WS_THICKFRAME | WS_SYSMENU,
                          true);
  } else if (old_bounds.height() <= kMinimumOverlappedWindowHeight &&
             new_bounds.height() > kMinimumOverlappedWindowHeight) {
    // Change the window style back to overlappped when the panel height grow
    // taller than the minimum overlapped window height.
    UpdateWindowAttribute(GWL_STYLE,
                          WS_OVERLAPPED | WS_THICKFRAME | WS_SYSMENU,
                          WS_POPUP,
                          true);
  }
#endif

  GetWidget()->SetBounds(new_bounds);
}

void PanelView::ClosePanel() {
  // We're already closing. Do nothing.
  if (window_closed_)
    return;

  if (!panel_->ShouldCloseWindow())
    return;

  // Cancel any currently running animation since we're closing down.
  if (bounds_animator_.get())
    bounds_animator_.reset();

  if (panel_->GetWebContents()) {
    // Still have web contents. Allow renderer to shut down.
    // When web contents are destroyed, we will be called back again.
    panel_->OnWindowClosing();
    return;
  }

  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);

  panel_->OnNativePanelClosed();
  if (window_)
    window_->Close();
  window_closed_ = true;
}

void PanelView::ActivatePanel() {
  window_->Activate();
}

void PanelView::DeactivatePanel() {
  if (!focused_)
    return;

#if defined(OS_WIN)
  // Need custom behavior for always-on-top panels to avoid
  // the OS activating a minimized panel when this one is
  // deactivated.
  if (always_on_top_) {
    ::SetForegroundWindow(::GetDesktopWindow());
    return;
  }
#endif

  window_->Deactivate();
}

bool PanelView::IsPanelActive() const {
  return focused_;
}

void PanelView::PreventActivationByOS(bool prevent_activation) {
#if defined(OS_WIN)
  // Set the flags "NoActivate" to make sure the minimized panels do not get
  // activated by the OS. In addition, set "AppWindow" to make sure the
  // minimized panels do appear in the taskbar and Alt-Tab menu if it is not
  // in a stack.
  int value_to_change = WS_EX_NOACTIVATE;
  if (!panel_->stack())
    value_to_change |= WS_EX_APPWINDOW;
  if (prevent_activation)
    UpdateWindowAttribute(GWL_EXSTYLE, value_to_change, 0, false);
  else
    UpdateWindowAttribute(GWL_EXSTYLE, 0, value_to_change, false);
#endif
}

gfx::NativeWindow PanelView::GetNativePanelWindow() {
  return window_->GetNativeWindow();
}

void PanelView::UpdatePanelTitleBar() {
  UpdateWindowTitle();
  UpdateWindowIcon();
}

void PanelView::UpdatePanelLoadingAnimations(bool should_animate) {
  GetFrameView()->UpdateThrobber();
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

bool PanelView::IsPanelAlwaysOnTop() const {
  return always_on_top_;
}

void PanelView::SetPanelAlwaysOnTop(bool on_top) {
  if (always_on_top_ == on_top)
    return;
  always_on_top_ = on_top;

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

void PanelView::SetWindowCornerStyle(panel::CornerStyle corner_style) {
  GetFrameView()->SetWindowCornerStyle(corner_style);
}

void PanelView::PanelExpansionStateChanging(Panel::ExpansionState old_state,
                                            Panel::ExpansionState new_state) {
#if defined(OS_WIN)
  // Live preview is only available since Windows 7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  bool is_minimized = old_state != Panel::EXPANDED;
  bool will_be_minimized = new_state != Panel::EXPANDED;
  if (is_minimized == will_be_minimized)
    return;

  HWND native_window = views::HWNDForWidget(window_);

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

    std::vector<HWND> snapshot_hwnds;
    thumbnailer_->Start(snapshot_hwnds);
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

void PanelView::MinimizePanelBySystem() {
  window_->Minimize();
}

bool PanelView::IsPanelMinimizedBySystem() const {
  return window_->IsMinimized();
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

gfx::ImageSkia PanelView::GetWindowAppIcon() {
  gfx::Image app_icon = panel_->app_icon();
  if (app_icon.IsEmpty())
    return GetWindowIcon();
  else
    return *app_icon.ToImageSkia();
}

gfx::ImageSkia PanelView::GetWindowIcon() {
  gfx::Image icon = panel_->GetCurrentPageIcon();
  return icon.IsEmpty() ? gfx::ImageSkia() : *icon.ToImageSkia();
}

void PanelView::WindowClosing() {
  // When closing a panel via window.close, API or the close button,
  // ClosePanel() is called first, destroying the native |window_|
  // which results in this method being called. ClosePanel() sets
  // |window_closed_| to NULL.
  // If we still have a |window_closed_| here, the close was triggered by the
  // OS, (e.g. clicking on taskbar menu), which destroys the native |window_|
  // without invoking ClosePanel() beforehand.
  if (!window_closed_) {
    panel_->OnWindowClosing();
    ClosePanel();
    DCHECK(window_closed_);
  }
}

void PanelView::DeleteDelegate() {
  delete this;
}

void PanelView::OnWindowBeginUserBoundsChange() {
  user_resizing_ = true;
  panel_->OnPanelStartUserResizing();

#if defined(OS_WIN)
  StackedPanelCollection* stack = panel_->stack();
  if (stack) {
    // Listen to WM_SIZING message in order to find out whether the interior
    // edge is being resized such that the specific maximum size could be
    // passed to the system.
    if (panel_->stack()->GetPanelBelow(panel_.get())) {
      ui::HWNDSubclass::AddFilterToTarget(views::HWNDForWidget(window_), this);
      user_resizing_interior_stacked_panel_edge_ = false;
    }

    // Keep track of the original full size of the resizing panel such that it
    // can be restored to this size once it is shrunk to minimized state.
    original_full_size_of_resizing_panel_ = panel_->full_size();

    // Keep track of the original full size of the panel below the resizing
    // panel such that it can be restored to this size once it is shrunk to
    // minimized state.
    Panel* below_panel = stack->GetPanelBelow(panel_.get());
    if (below_panel && !below_panel->IsMinimized()) {
      original_full_size_of_panel_below_resizing_panel_ =
          below_panel->full_size();
    }
  }
#endif
}

void PanelView::OnWindowEndUserBoundsChange() {
  user_resizing_ = false;
  panel_->OnPanelEndUserResizing();

  // No need to proceed with post-resizing update when there is no size change.
  gfx::Rect new_bounds = window_->GetWindowBoundsInScreen();
  if (bounds_ == new_bounds)
    return;
  bounds_ = new_bounds;

  panel_->IncreaseMaxSize(bounds_.size());
  panel_->set_full_size(bounds_.size());

#if defined(OS_WIN)
  StackedPanelCollection* stack = panel_->stack();
  if (stack) {
    // No need to listen to WM_SIZING message any more.
    ui::HWNDSubclass::RemoveFilterFromAllTargets(this);

    // If the height of resizing panel shrinks close to the titlebar height,
    // treate it as minimized. This could occur when the user is dragging
    // 1) the top edge of the top panel downward to shrink it; or
    // 2) the bottom edge of any panel upward to shrink it.
    if (panel_->GetBounds().height() <
            kStackedPanelHeightShrinkThresholdToBecomeMinimized) {
      stack->MinimizePanel(panel_.get());
      panel_->set_full_size(original_full_size_of_resizing_panel_);
    }

    // If the height of panel below the resizing panel shrinks close to the
    // titlebar height, treat it as minimized. This could occur when the user
    // is dragging the bottom edge of non-bottom panel downward to expand it
    // and also shrink the panel below.
    Panel* below_panel = stack->GetPanelBelow(panel_.get());
    if (below_panel && !below_panel->IsMinimized() &&
        below_panel->GetBounds().height() <
            kStackedPanelHeightShrinkThresholdToBecomeMinimized) {
      stack->MinimizePanel(below_panel);
      below_panel->set_full_size(
          original_full_size_of_panel_below_resizing_panel_);
    }
  }
#endif

  panel_->collection()->RefreshLayout();
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
  // If the panel is minimized, it can be rendered to very small size, like
  // 4-pixel lines when it is docked. Otherwise, its size should not be less
  // than its minimum size.
  return panel_->IsMinimized() ? gfx::Size() :
      gfx::Size(panel::kPanelMinWidth, panel::kPanelMinHeight);
}

gfx::Size PanelView::GetMaximumSize() {
  // If the user is resizing a stacked panel by its bottom edge, make sure its
  // height cannot grow more than what the panel below it could offer. This is
  // because growing a stacked panel by y amount will shrink the panel below it
  // by same amount and we do not want the panel below it being shrunk to be
  // smaller than the titlebar.
#if defined(OS_WIN)
  if (panel_->stack() && user_resizing_interior_stacked_panel_edge_) {
    Panel* below_panel = panel_->stack()->GetPanelBelow(panel_.get());
    if (below_panel && !below_panel->IsMinimized()) {
      return gfx::Size(0, below_panel->GetBounds().bottom() -
          panel_->GetBounds().y() - panel::kTitlebarHeight);
    }
  }
#endif
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

void PanelView::OnWidgetDestroying(views::Widget* widget) {
  window_ = NULL;
}

void PanelView::OnWidgetActivationChanged(views::Widget* widget, bool active) {
#if defined(OS_WIN)
  // The panel window is in focus (actually accepting keystrokes) if it is
  // active and belongs to a foreground application.
  bool focused = active &&
      views::HWNDForWidget(widget) == ::GetForegroundWindow();
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
      panel_->collection()->type() == PanelCollection::DOCKED &&
      gfx::Screen::GetScreenFor(widget->GetNativeWindow())->
          GetWindowAtCursorScreenPoint() != widget->GetNativeWindow()) {
    panel_->Restore();
  }

  panel()->OnActiveStateChanged(focused);
}

void PanelView::OnWidgetBoundsChanged(views::Widget* widget,
                                      const gfx::Rect& new_bounds) {
  if (user_resizing_)
    panel()->collection()->OnPanelResizedByMouse(panel(), new_bounds);
}

void PanelView::OnNativeFocusChange(gfx::NativeView focused_before,
                                    gfx::NativeView focused_now) {
  if (focused_now != window_->GetNativeView())
    return;

  // Give web contents view a chance to set focus to the appropriate element.
  content::WebContents* web_contents = panel_->GetWebContents();
  if (web_contents)
    web_contents->GetView()->RestoreFocus();
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

  if (mouse_dragging_state_ == NO_DRAGGING &&
      ExceededDragThreshold(mouse_location - last_mouse_location_)) {
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

#if defined(OS_WIN)
void PanelView::UpdateWindowAttribute(int attribute_index,
                                      int attribute_value_to_set,
                                      int attribute_value_to_reset,
                                      bool update_frame) {
  HWND native_window = views::HWNDForWidget(window_);
  int value = ::GetWindowLong(native_window, attribute_index);
  int expected_value = value;
  if (attribute_value_to_set)
    expected_value |=  attribute_value_to_set;
  if (attribute_value_to_reset)
    expected_value &=  ~attribute_value_to_reset;
  if (value != expected_value)
    ::SetWindowLong(native_window, attribute_index, expected_value);

  // Per MSDN, if any of the frame styles is changed, SetWindowPos with the
  // SWP_FRAMECHANGED flag must be called in order for the cached window data
  // to be updated properly.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms633591(v=vs.85).aspx
  if (update_frame) {
    ::SetWindowPos(native_window, NULL, 0, 0, 0, 0,
                   SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                       SWP_NOZORDER | SWP_NOACTIVATE);
  }
}
#endif

void PanelView::OnViewWasResized() {
#if defined(OS_WIN) && !defined(USE_AURA)
  content::WebContents* web_contents = panel_->GetWebContents();
  if (!web_view_ || !web_contents)
    return;

  // When the panel is frameless or has thin frame, the mouse resizing should
  // also be triggered from the part of client area that is close to the window
  // frame.
  int width = web_view_->size().width();
  int height = web_view_->size().height();
  // Compute the thickness of the client area that needs to be counted towards
  // mouse resizing.
  int thickness_for_mouse_resizing =
      kResizeInsideBoundsSize - GetFrameView()->BorderThickness();
  DCHECK(thickness_for_mouse_resizing > 0);
  SkRegion* region = new SkRegion;
  region->op(0, 0, thickness_for_mouse_resizing, height, SkRegion::kUnion_Op);
  region->op(width - thickness_for_mouse_resizing, 0, width, height,
      SkRegion::kUnion_Op);
  region->op(0, height - thickness_for_mouse_resizing, width, height,
      SkRegion::kUnion_Op);
  web_contents->GetRenderViewHost()->GetView()->SetClickthroughRegion(region);
#endif
}
