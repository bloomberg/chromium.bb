// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_view.h"

#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

const char kViewClassName[] = "browser/chromeos/login/WebUILoginView";

}  // namespace

namespace chromeos {

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : profile_(NULL),
      status_area_(NULL),
      webui_login_(NULL) {
}

WebUILoginView::~WebUILoginView() {
}

void WebUILoginView::Init() {
  profile_ = ProfileManager::GetDefaultProfile();

  webui_login_ = new DOMView();
  AddChildView(webui_login_);
  webui_login_->Init(profile_, NULL);
  webui_login_->SetVisible(true);

  InitStatusArea();
}


std::string WebUILoginView::GetClassName() const {
  return kViewClassName;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::OnDialogClosed() {
}

void WebUILoginView::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
  SchedulePaint();
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
}

WebUI* WebUILoginView::GetWebUI() {
  return webui_login_->tab_contents()->web_ui();
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(webui_login_);
  DCHECK(status_area_);

  // Layout the Status Area up in the right corner. This should always be done.
  const int kCornerPadding = 5;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - kCornerPadding,
      kCornerPadding,
      status_area_size.width(),
      status_area_size.height());

  // Layout the DOMView for the login page
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

// WebUILoginView private: -----------------------------------------------------

void WebUILoginView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  AddChildView(status_area_);
}

}  // namespace chromeos
