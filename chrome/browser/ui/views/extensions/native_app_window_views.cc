// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/native_app_window_views.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/extensions/shell_window_frame_view.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/common/draggable_region.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "base/utf_string_conversions.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/base/win/shell.h"
#endif

#if defined(USE_ASH)
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/custom_frame_view_ash.h"
#include "ash/wm/panel_frame_view.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/aura/root_window.h"
#endif

namespace {
const int kMinPanelWidth = 100;
const int kMinPanelHeight = 100;
const int kDefaultPanelWidth = 200;
const int kDefaultPanelHeight = 300;
const int kResizeInsideBoundsSize = 5;
}

NativeAppWindowViews::NativeAppWindowViews(
    ShellWindow* shell_window,
    const ShellWindow::CreateParams& create_params)
    : shell_window_(shell_window),
      web_view_(NULL),
      window_(NULL),
      is_fullscreen_(false),
      frameless_(create_params.frame == ShellWindow::FRAME_NONE),
      transparent_background_(create_params.transparent_background) {
  Observe(web_contents());
  minimum_size_ = create_params.minimum_size;
  maximum_size_ = create_params.maximum_size;

  window_ = new views::Widget;
  if (create_params.window_type == ShellWindow::WINDOW_TYPE_PANEL ||
      create_params.window_type == ShellWindow::WINDOW_TYPE_V1_PANEL) {
    InitializePanelWindow(create_params);
  } else {
    InitializeDefaultWindow(create_params);
  }
  extension_keybinding_registry_.reset(
      new ExtensionKeybindingRegistryViews(
          profile(),
          window_->GetFocusManager(),
          extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY,
          shell_window_));

  OnViewWasResized();
  window_->AddObserver(this);
}

NativeAppWindowViews::~NativeAppWindowViews() {
  web_view_->SetWebContents(NULL);
}

void NativeAppWindowViews::InitializeDefaultWindow(
    const ShellWindow::CreateParams& create_params) {
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.remove_standard_frame = true;
  init_params.use_system_default_icon = true;
  // TODO(erg): Conceptually, these are toplevel windows, but we theoretically
  // could plumb context through to here in some cases.
  init_params.top_level = true;
  window_->Init(init_params);
  gfx::Rect window_bounds = create_params.bounds;
  window_bounds.Inset(-GetFrameInsets());
  // Center window if no position was specified.
  if (create_params.bounds.x() == INT_MIN ||
      create_params.bounds.y() == INT_MIN) {
    window_->CenterWindow(window_bounds.size());
  } else {
    window_->SetBounds(window_bounds);
  }

#if defined(OS_WIN) && !defined(USE_AURA)
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      extension()->id());
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppModelIdForProfile(
          UTF8ToWide(app_name), shell_window_->profile()->GetPath()),
      GetWidget()->GetTopLevelWidget()->GetNativeWindow());
#endif
}

void NativeAppWindowViews::InitializePanelWindow(
    const ShellWindow::CreateParams& create_params) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_PANEL);
  params.delegate = this;

  preferred_size_ = gfx::Size(create_params.bounds.width(),
                              create_params.bounds.height());
  if (preferred_size_.width() == 0)
    preferred_size_.set_width(kDefaultPanelWidth);
  else if (preferred_size_.width() < kMinPanelWidth)
    preferred_size_.set_width(kMinPanelWidth);

  if (preferred_size_.height() == 0)
    preferred_size_.set_height(kDefaultPanelHeight);
  else if (preferred_size_.height() < kMinPanelHeight)
    preferred_size_.set_height(kMinPanelHeight);
#if defined(USE_ASH)
  if (ash::Shell::HasInstance()) {
    // Open a new panel on the active root window where
    // a current active/focused window is on.
    aura::RootWindow* active = ash::Shell::GetActiveRootWindow();
    params.bounds = ash::ScreenAsh::ConvertRectToScreen(
        active, gfx::Rect(preferred_size_));
  } else {
    params.bounds = gfx::Rect(preferred_size_);
  }
#else
  params.bounds = gfx::Rect(preferred_size_);
#endif
  // TODO(erg): Conceptually, these are toplevel windows, but we theoretically
  // could plumb context through to here in some cases.
  params.top_level = true;
  window_->Init(params);

#if !defined(USE_ASH)
  // TODO(oshima|stevenjb): Ideally, we should be able to just pre-determine
  // the exact location and size, but this doesn't work well
  // on non-ash environment where we don't have full control over
  // window management.
  gfx::Rect window_bounds =
      window_->non_client_view()->GetWindowBoundsForClientBounds(
          create_params.bounds);
  window_->SetBounds(window_bounds);
#endif
}

// BaseWindow implementation.

bool NativeAppWindowViews::IsActive() const {
  return window_->IsActive();
}

bool NativeAppWindowViews::IsMaximized() const {
  return window_->IsMaximized();
}

bool NativeAppWindowViews::IsMinimized() const {
  return window_->IsMinimized();
}

bool NativeAppWindowViews::IsFullscreen() const {
  return window_->IsFullscreen();
}

gfx::NativeWindow NativeAppWindowViews::GetNativeWindow() {
  return window_->GetNativeWindow();
}

gfx::Rect NativeAppWindowViews::GetRestoredBounds() const {
  return window_->GetRestoredBounds();
}

gfx::Rect NativeAppWindowViews::GetBounds() const {
  return window_->GetWindowBoundsInScreen();
}

void NativeAppWindowViews::Show() {
  if (window_->IsVisible()) {
    window_->Activate();
    return;
  }

  window_->Show();
}

void NativeAppWindowViews::ShowInactive() {
  if (window_->IsVisible())
    return;
  window_->ShowInactive();
}

void NativeAppWindowViews::Hide() {
  window_->Hide();
}

void NativeAppWindowViews::Close() {
  window_->Close();
}

void NativeAppWindowViews::Activate() {
  window_->Activate();
}

void NativeAppWindowViews::Deactivate() {
  window_->Deactivate();
}

void NativeAppWindowViews::Maximize() {
  window_->Maximize();
}

void NativeAppWindowViews::Minimize() {
  window_->Minimize();
}

void NativeAppWindowViews::Restore() {
  window_->Restore();
}

void NativeAppWindowViews::SetBounds(const gfx::Rect& bounds) {
  GetWidget()->SetBounds(bounds);
}

void NativeAppWindowViews::FlashFrame(bool flash) {
  window_->FlashFrame(flash);
}

bool NativeAppWindowViews::IsAlwaysOnTop() const {
  return shell_window_->window_type_is_panel();
}

gfx::Insets NativeAppWindowViews::GetFrameInsets() const {
  if (frameless())
    return gfx::Insets();

  // The pretend client_bounds passed in need to be large enough to ensure that
  // GetWindowBoundsForClientBounds() doesn't decide that it needs more than
  // the specified amount of space to fit the window controls in, and return a
  // number larger than the real frame insets. Most window controls are smaller
  // than 1000x1000px, so this should be big enough.
  gfx::Rect client_bounds = gfx::Rect(1000, 1000);
  gfx::Rect window_bounds =
      window_->non_client_view()->GetWindowBoundsForClientBounds(
          client_bounds);
  return window_bounds.InsetsFrom(client_bounds);
}

// Private method. TODO(stevenjb): Move this below InitializePanelWindow()
// to match declaration order.
void NativeAppWindowViews::OnViewWasResized() {
  // TODO(jeremya): this doesn't seem like a terribly elegant way to keep the
  // window shape in sync.
#if defined(OS_WIN) && !defined(USE_AURA)
  // Set the window shape of the RWHV.
  DCHECK(window_);
  DCHECK(web_view_);
  gfx::Size sz = web_view_->size();
  int height = sz.height(), width = sz.width();
  int radius = 1;
  gfx::Path path;
  if (window_->IsMaximized() || window_->IsFullscreen()) {
    // Don't round the corners when the window is maximized or fullscreen.
    path.addRect(0, 0, width, height);
  } else {
    if (frameless_) {
      path.moveTo(0, radius);
      path.lineTo(radius, 0);
      path.lineTo(width - radius, 0);
      path.lineTo(width, radius);
    } else {
      // Don't round the top corners in chrome-style frame mode.
      path.moveTo(0, 0);
      path.lineTo(width, 0);
    }
    path.lineTo(width, height - radius - 1);
    path.lineTo(width - radius - 1, height);
    path.lineTo(radius + 1, height);
    path.lineTo(0, height - radius - 1);
    path.close();
  }
  SetWindowRgn(web_contents()->GetNativeView(), path.CreateNativeRegion(), 1);

  SkRegion* rgn = new SkRegion;
  if (!window_->IsFullscreen()) {
    if (draggable_region())
      rgn->op(*draggable_region(), SkRegion::kUnion_Op);
    if (!window_->IsMaximized()) {
      if (frameless_)
        rgn->op(0, 0, width, kResizeInsideBoundsSize, SkRegion::kUnion_Op);
      rgn->op(0, 0, kResizeInsideBoundsSize, height, SkRegion::kUnion_Op);
      rgn->op(width - kResizeInsideBoundsSize, 0, width, height,
          SkRegion::kUnion_Op);
      rgn->op(0, height - kResizeInsideBoundsSize, width, height,
          SkRegion::kUnion_Op);
    }
  }
  if (web_contents()->GetRenderViewHost()->GetView())
    web_contents()->GetRenderViewHost()->GetView()->SetClickthroughRegion(rgn);
#endif
}

// WidgetDelegate implementation.

void NativeAppWindowViews::OnWidgetMove() {
  shell_window_->OnNativeWindowChanged();
}

views::View* NativeAppWindowViews::GetInitiallyFocusedView() {
  return web_view_;
}

bool NativeAppWindowViews::CanResize() const {
  return maximum_size_.IsEmpty() || minimum_size_ != maximum_size_;
}

bool NativeAppWindowViews::CanMaximize() const {
  return maximum_size_.IsEmpty();
}

string16 NativeAppWindowViews::GetWindowTitle() const {
  return shell_window_->GetTitle();
}

bool NativeAppWindowViews::ShouldShowWindowTitle() const {
  return shell_window_->window_type() == ShellWindow::WINDOW_TYPE_V1_PANEL;
}

gfx::ImageSkia NativeAppWindowViews::GetWindowAppIcon() {
  gfx::Image app_icon = shell_window_->app_icon();
  if (app_icon.IsEmpty())
    return GetWindowIcon();
  else
    return *app_icon.ToImageSkia();
}

gfx::ImageSkia NativeAppWindowViews::GetWindowIcon() {
  content::WebContents* web_contents = shell_window_->web_contents();
  if (web_contents) {
    FaviconTabHelper* favicon_tab_helper =
        FaviconTabHelper::FromWebContents(web_contents);
    gfx::Image app_icon = favicon_tab_helper->GetFavicon();
    if (!app_icon.IsEmpty())
      return *app_icon.ToImageSkia();
  }
  return gfx::ImageSkia();
}

bool NativeAppWindowViews::ShouldShowWindowIcon() const {
  return shell_window_->window_type() == ShellWindow::WINDOW_TYPE_V1_PANEL;
}

void NativeAppWindowViews::SaveWindowPlacement(const gfx::Rect& bounds,
                                               ui::WindowShowState show_state) {
  views::WidgetDelegate::SaveWindowPlacement(bounds, show_state);
  shell_window_->OnNativeWindowChanged();
}

void NativeAppWindowViews::DeleteDelegate() {
  window_->RemoveObserver(this);
  shell_window_->OnNativeClose();
}

views::Widget* NativeAppWindowViews::GetWidget() {
  return window_;
}

const views::Widget* NativeAppWindowViews::GetWidget() const {
  return window_;
}

views::NonClientFrameView* NativeAppWindowViews::CreateNonClientFrameView(
    views::Widget* widget) {
#if defined(USE_ASH)
  if (chrome::IsNativeViewInAsh(widget->GetNativeView())) {
    if (shell_window_->window_type_is_panel()) {
      ash::PanelFrameView::FrameType frame_type = frameless_ ?
          ash::PanelFrameView::FRAME_NONE : ash::PanelFrameView::FRAME_ASH;
      return new ash::PanelFrameView(widget, frame_type);
    }
    if (!frameless_) {
      ash::CustomFrameViewAsh* frame = new ash::CustomFrameViewAsh();
      frame->Init(widget);
      return frame;
    }
  }
#endif
  ShellWindowFrameView* frame_view = new ShellWindowFrameView(this);
  frame_view->Init(window_);
  return frame_view;
}

bool NativeAppWindowViews::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
#if defined(USE_AURA)
  DCHECK_EQ(child, web_view_->web_contents()->GetView()->GetNativeView());
  // Shell window should claim mouse events that fall within the draggable
  // region.
  return !draggable_region_.get() ||
         !draggable_region_->contains(location.x(), location.y());
#else
  return true;
#endif
}

// WidgetObserver implementation.

void NativeAppWindowViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  shell_window_->OnNativeWindowChanged();
}

void NativeAppWindowViews::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  shell_window_->OnNativeWindowChanged();
}

// WebContentsObserver implementation.

void NativeAppWindowViews::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (transparent_background_) {
    // Use a background with transparency to trigger transparency in Webkit.
    SkBitmap background;
    background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    background.allocPixels();
    background.eraseARGB(0x00, 0x00, 0x00, 0x00);

    content::RenderWidgetHostView* view = render_view_host->GetView();
    DCHECK(view);
    view->SetBackground(background);
  }
}

// views::View implementation.

void NativeAppWindowViews::Layout() {
  DCHECK(web_view_);
  web_view_->SetBounds(0, 0, width(), height());
  OnViewWasResized();
}

void NativeAppWindowViews::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this) {
    web_view_ = new views::WebView(NULL);
    AddChildView(web_view_);
    web_view_->SetWebContents(web_contents());
  }
}

gfx::Size NativeAppWindowViews::GetPreferredSize() {
  if (!preferred_size_.IsEmpty())
    return preferred_size_;
  return views::View::GetPreferredSize();
}

gfx::Size NativeAppWindowViews::GetMinimumSize() {
  return minimum_size_;
}

gfx::Size NativeAppWindowViews::GetMaximumSize() {
  return maximum_size_;
}

void NativeAppWindowViews::OnFocus() {
  web_view_->RequestFocus();
}

// NativeAppWindow implementation.

void NativeAppWindowViews::SetFullscreen(bool fullscreen) {
  // Fullscreen not supported by panels.
  if (shell_window_->window_type_is_panel())
    return;
  is_fullscreen_ = fullscreen;
  window_->SetFullscreen(fullscreen);
  // TODO(jeremya) we need to call RenderViewHost::ExitFullscreen() if we
  // ever drop the window out of fullscreen in response to something that
  // wasn't the app calling webkitCancelFullScreen().
}

bool NativeAppWindowViews::IsFullscreenOrPending() const {
  return is_fullscreen_;
}

views::View* NativeAppWindowViews::GetContentsView() {
  return this;
}

void NativeAppWindowViews::UpdateWindowIcon() {
  window_->UpdateWindowIcon();
}

void NativeAppWindowViews::UpdateWindowTitle() {
  window_->UpdateWindowTitle();
}

void NativeAppWindowViews::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (!frameless_)
    return;

  draggable_region_.reset(ShellWindow::RawDraggableRegionsToSkRegion(regions));
  OnViewWasResized();
}

void NativeAppWindowViews::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
}

void NativeAppWindowViews::RenderViewHostChanged() {
  OnViewWasResized();
}

//------------------------------------------------------------------------------
// NativeAppWindow::Create

// static
NativeAppWindow* NativeAppWindow::Create(
    ShellWindow* shell_window, const ShellWindow::CreateParams& params) {
  return new NativeAppWindowViews(shell_window, params);
}
