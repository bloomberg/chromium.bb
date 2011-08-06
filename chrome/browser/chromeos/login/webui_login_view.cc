// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_view.h"

#include "chrome/browser/chromeos/accessibility_util.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/widget/widget.h"

namespace {

const char kViewClassName[] = "browser/chromeos/login/WebUILoginView";

}  // namespace

namespace chromeos {

// static
const int WebUILoginView::kStatusAreaCornerPadding = 5;

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : status_area_(NULL),
      profile_(NULL),
      webui_login_(NULL),
      status_window_(NULL),
      accel_toggle_accessibility_(
          views::Accelerator(ui::VKEY_Z, false, true, true)) {
  // Accelerator events will be sent to this window until the WebUI dialog gains
  // focus via keyboard or mouse input, so we have to watch for the
  // accessibility hotkey here as well.
  AddAccelerator(accel_toggle_accessibility_);
}

WebUILoginView::~WebUILoginView() {
  if (status_window_)
    status_window_->Close();
  status_window_ = NULL;
}

void WebUILoginView::Init() {
  profile_ = ProfileManager::GetDefaultProfile();

  webui_login_ = new DOMView();
  AddChildView(webui_login_);
  webui_login_->Init(profile_, NULL);
  webui_login_->SetVisible(true);
  webui_login_->tab_contents()->set_delegate(this);
}

std::string WebUILoginView::GetClassName() const {
  return kViewClassName;
}

bool WebUILoginView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator == accel_toggle_accessibility_) {
    accessibility::ToggleAccessibility();
  } else {
    return false;
  }
  return true;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::OnWindowCreated() {
  InitStatusArea();
}

void WebUILoginView::UpdateWindowType() {
  std::vector<int> params;
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(GetNativeWindow()),
      WM_IPC_WINDOW_LOGIN_WEBUI,
      &params);
}

void WebUILoginView::LoadURL(const GURL & url) {
  webui_login_->LoadURL(url);
  webui_login_->RequestFocus();
}

WebUI* WebUILoginView::GetWebUI() {
  return webui_login_->tab_contents()->web_ui();
}

void WebUILoginView::SetStatusAreaEnabled(bool enable) {
  if (status_area_)
    status_area_->MakeButtonsActive(enable);
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
  if (status_area_)
    status_area_->SetVisible(visible);
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(webui_login_);
  webui_login_->SetBoundsRect(bounds());
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

Profile* WebUILoginView::GetProfile() const {
  return profile_;
}

void WebUILoginView::ExecuteBrowserCommand(int id) const {
}

bool WebUILoginView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->network_view())
    return true;

  if (button_view == status_area_->clock_view() ||
      button_view == status_area_->input_method_view())
    return false;

  return true;
}

void WebUILoginView::OpenButtonOptions(const views::View* button_view) {
  if (button_view == status_area_->network_view()) {
    if (proxy_settings_dialog_.get() == NULL) {
      proxy_settings_dialog_.reset(new ProxySettingsDialog(
          this, GetNativeWindow()));
    }
    proxy_settings_dialog_->Show();
  }
}

StatusAreaHost::ScreenMode WebUILoginView::GetScreenMode() const {
  return kLoginMode;
}

StatusAreaHost::TextStyle WebUILoginView::GetTextStyle() const {
  return kWhitePlain;
}

void WebUILoginView::ButtonVisibilityChanged(views::View* button_view) {
  status_area_->ButtonVisibilityChanged(button_view);
}

void WebUILoginView::OnDialogClosed() {
}

void WebUILoginView::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
  SchedulePaint();
}

void WebUILoginView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  DCHECK(status_window_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();

  views::Widget* login_window = WebUILoginDisplay::GetLoginWindow();
  gfx::Size size = status_area_->GetPreferredSize();
  gfx::Rect bounds(width() - size.width() - kStatusAreaCornerPadding,
                   kStatusAreaCornerPadding,
                   size.width(),
                   size.height());

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.bounds = bounds;
  widget_params.double_buffer = true;
  widget_params.transparent = true;
  widget_params.parent = login_window->GetNativeView();
  status_window_ = new views::Widget;
  status_window_->Init(widget_params);
  chromeos::WmIpc::instance()->SetWindowType(
      status_window_->GetNativeView(),
      chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
      NULL);
  status_window_->SetContentsView(status_area_);
  status_window_->Show();
}

// WebUILoginView private: -----------------------------------------------------

bool WebUILoginView::HandleContextMenu(const ContextMenuParams& params) {
  // Do not show the context menu.
#ifndef NDEBUG
  return false;
#else
  return true;
#endif
}

bool WebUILoginView::TakeFocus(bool reverse) {
  // Forward the focus back to web contents.
  webui_login_->tab_contents()->FocusThroughTabTraversal(reverse);
  return true;
}

}  // namespace chromeos
