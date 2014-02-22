// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/views/base_native_app_window_views.h"

#include "apps/app_window.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/common/draggable_region.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using apps::AppWindow;

BaseNativeAppWindowViews::BaseNativeAppWindowViews()
    : app_window_(NULL),
      web_view_(NULL),
      window_(NULL),
      frameless_(false),
      transparent_background_(false),
      resizable_(false) {}

void BaseNativeAppWindowViews::Init(
    apps::AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  app_window_ = app_window;
  frameless_ = create_params.frame == AppWindow::FRAME_NONE;
  transparent_background_ = create_params.transparent_background;
  resizable_ = create_params.resizable;
  Observe(app_window_->web_contents());

  window_ = new views::Widget;
  InitializeWindow(app_window, create_params);

  OnViewWasResized();
  window_->AddObserver(this);
}

BaseNativeAppWindowViews::~BaseNativeAppWindowViews() {
  web_view_->SetWebContents(NULL);
}

void BaseNativeAppWindowViews::InitializeWindow(
    apps::AppWindow* app_window,
    const apps::AppWindow::CreateParams& create_params) {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.top_level = true;
  init_params.keep_on_top = create_params.always_on_top;
  window_->Init(init_params);
  window_->CenterWindow(create_params.bounds.size());
}

// ui::BaseWindow implementation.

bool BaseNativeAppWindowViews::IsActive() const {
  return window_->IsActive();
}

bool BaseNativeAppWindowViews::IsMaximized() const {
  return window_->IsMaximized();
}

bool BaseNativeAppWindowViews::IsMinimized() const {
  return window_->IsMinimized();
}

bool BaseNativeAppWindowViews::IsFullscreen() const {
  return window_->IsFullscreen();
}

gfx::NativeWindow BaseNativeAppWindowViews::GetNativeWindow() {
  return window_->GetNativeWindow();
}

gfx::Rect BaseNativeAppWindowViews::GetRestoredBounds() const {
  return window_->GetRestoredBounds();
}

ui::WindowShowState BaseNativeAppWindowViews::GetRestoredState() const {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect BaseNativeAppWindowViews::GetBounds() const {
  return window_->GetWindowBoundsInScreen();
}

void BaseNativeAppWindowViews::Show() {
  if (window_->IsVisible()) {
    window_->Activate();
    return;
  }
  window_->Show();
}

void BaseNativeAppWindowViews::ShowInactive() {
  if (window_->IsVisible())
    return;

  window_->ShowInactive();
}

void BaseNativeAppWindowViews::Hide() {
  window_->Hide();
}

void BaseNativeAppWindowViews::Close() {
  window_->Close();
}

void BaseNativeAppWindowViews::Activate() {
  window_->Activate();
}

void BaseNativeAppWindowViews::Deactivate() {
  window_->Deactivate();
}

void BaseNativeAppWindowViews::Maximize() {
  window_->Maximize();
}

void BaseNativeAppWindowViews::Minimize() {
  window_->Minimize();
}

void BaseNativeAppWindowViews::Restore() {
  window_->Restore();
}

void BaseNativeAppWindowViews::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void BaseNativeAppWindowViews::FlashFrame(bool flash) {
  window_->FlashFrame(flash);
}

bool BaseNativeAppWindowViews::IsAlwaysOnTop() const {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
  return window_->IsAlwaysOnTop();
}

void BaseNativeAppWindowViews::SetAlwaysOnTop(bool always_on_top) {
  window_->SetAlwaysOnTop(always_on_top);
}

gfx::NativeView BaseNativeAppWindowViews::GetHostView() const {
  return window_->GetNativeView();
}

gfx::Point BaseNativeAppWindowViews::GetDialogPosition(const gfx::Size& size) {
  gfx::Size app_window_size = window_->GetWindowBoundsInScreen().size();
  return gfx::Point(app_window_size.width() / 2 - size.width() / 2,
                    app_window_size.height() / 2 - size.height() / 2);
}

gfx::Size BaseNativeAppWindowViews::GetMaximumDialogSize() {
  return window_->GetWindowBoundsInScreen().size();
}

void BaseNativeAppWindowViews::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.AddObserver(observer);
}
void BaseNativeAppWindowViews::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void BaseNativeAppWindowViews::OnViewWasResized() {
  FOR_EACH_OBSERVER(web_modal::ModalDialogHostObserver,
                    observer_list_,
                    OnPositionRequiresUpdate());
}

// WidgetDelegate implementation.

void BaseNativeAppWindowViews::OnWidgetMove() {
  app_window_->OnNativeWindowChanged();
}

views::View* BaseNativeAppWindowViews::GetInitiallyFocusedView() {
  return web_view_;
}

bool BaseNativeAppWindowViews::CanResize() const {
  return resizable_ && !app_window_->size_constraints().HasFixedSize();
}

bool BaseNativeAppWindowViews::CanMaximize() const {
  return resizable_ && !app_window_->size_constraints().HasMaximumSize() &&
         !app_window_->window_type_is_panel();
}

base::string16 BaseNativeAppWindowViews::GetWindowTitle() const {
  return app_window_->GetTitle();
}

bool BaseNativeAppWindowViews::ShouldShowWindowTitle() const {
  return app_window_->window_type() == AppWindow::WINDOW_TYPE_V1_PANEL;
}

bool BaseNativeAppWindowViews::ShouldShowWindowIcon() const {
  return app_window_->window_type() == AppWindow::WINDOW_TYPE_V1_PANEL;
}

void BaseNativeAppWindowViews::SaveWindowPlacement(
    const gfx::Rect& bounds,
    ui::WindowShowState show_state) {
  views::WidgetDelegate::SaveWindowPlacement(bounds, show_state);
  app_window_->OnNativeWindowChanged();
}

void BaseNativeAppWindowViews::DeleteDelegate() {
  window_->RemoveObserver(this);
  app_window_->OnNativeClose();
}

views::Widget* BaseNativeAppWindowViews::GetWidget() {
  return window_;
}

const views::Widget* BaseNativeAppWindowViews::GetWidget() const {
  return window_;
}

views::View* BaseNativeAppWindowViews::GetContentsView() {
  return this;
}

bool BaseNativeAppWindowViews::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
#if defined(USE_AURA)
  if (child->Contains(web_view_->web_contents()->GetView()->GetNativeView())) {
    // App window should claim mouse events that fall within the draggable
    // region.
    return !draggable_region_.get() ||
           !draggable_region_->contains(location.x(), location.y());
  }
#endif

  return true;
}

// WidgetObserver implementation.

void BaseNativeAppWindowViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                                         bool visible) {
  app_window_->OnNativeWindowChanged();
}

void BaseNativeAppWindowViews::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  app_window_->OnNativeWindowChanged();
  if (active)
    app_window_->OnNativeWindowActivated();
}

// WebContentsObserver implementation.

void BaseNativeAppWindowViews::RenderViewCreated(
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

void BaseNativeAppWindowViews::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  OnViewWasResized();
}

// views::View implementation.

void BaseNativeAppWindowViews::Layout() {
  DCHECK(web_view_);
  web_view_->SetBounds(0, 0, width(), height());
  OnViewWasResized();
}

void BaseNativeAppWindowViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    web_view_ = new views::WebView(NULL);
    AddChildView(web_view_);
    web_view_->SetWebContents(app_window_->web_contents());
  }
}

gfx::Size BaseNativeAppWindowViews::GetMinimumSize() {
  return app_window_->size_constraints().GetMinimumSize();
}

gfx::Size BaseNativeAppWindowViews::GetMaximumSize() {
  return app_window_->size_constraints().GetMaximumSize();
}

void BaseNativeAppWindowViews::OnFocus() {
  web_view_->RequestFocus();
}

// NativeAppWindow implementation.

void BaseNativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
  window_->SetFullscreen(fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
}

bool BaseNativeAppWindowViews::IsFullscreenOrPending() const {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
  return window_->IsFullscreen();
}

bool BaseNativeAppWindowViews::IsDetached() const {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
  return false;
}

void BaseNativeAppWindowViews::UpdateWindowIcon() {
  window_->UpdateWindowIcon();
}

void BaseNativeAppWindowViews::UpdateWindowTitle() {
  window_->UpdateWindowTitle();
}

void BaseNativeAppWindowViews::UpdateBadgeIcon() {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
}

void BaseNativeAppWindowViews::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (!frameless_)
    return;

  draggable_region_.reset(AppWindow::RawDraggableRegionsToSkRegion(regions));
  OnViewWasResized();
}

SkRegion* BaseNativeAppWindowViews::GetDraggableRegion() {
  return draggable_region_.get();
}

void BaseNativeAppWindowViews::UpdateShape(scoped_ptr<SkRegion> region) {
  // Stub implementation. See also NativeAppWindowViews in Chrome.
}

void BaseNativeAppWindowViews::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
}

bool BaseNativeAppWindowViews::IsFrameless() const {
  return frameless_;
}

bool BaseNativeAppWindowViews::HasFrameColor() const { return false; }

SkColor BaseNativeAppWindowViews::FrameColor() const { return SK_ColorBLACK; }

gfx::Insets BaseNativeAppWindowViews::GetFrameInsets() const {
  if (frameless_)
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

void BaseNativeAppWindowViews::HideWithApp() {}

void BaseNativeAppWindowViews::ShowWithApp() {}

void BaseNativeAppWindowViews::UpdateWindowMinMaxSize() {}
