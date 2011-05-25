// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_view.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_touch.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/transform.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/widget.h"

// TODO(rharrison): Modify this class to support both touch and non-touch

namespace {

const int kKeyboardHeight = 300;
const int kKeyboardSlideDuration = 500;  // In milliseconds

PropertyAccessor<bool>* GetFocusedStateAccessor() {
  static PropertyAccessor<bool> state;
  return &state;
}

bool TabContentsHasFocus(const TabContents* contents) {
  views::View* view = static_cast<TabContentsViewTouch*>(contents->view());
  return view->Contains(view->GetFocusManager()->GetFocusedView());
}

}  // namespace

namespace chromeos {

// static
const char WebUILoginView::kViewClassName[] =
    "browser/chromeos/login/WebUILoginView";

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : profile_(NULL),
      status_area_(NULL),
      webui_login_(NULL),
      keyboard_showing_(false),
      focus_listener_added_(false),
      keyboard_(NULL) {
}

void WebUILoginView::Init(const GURL& login_url) {
  CHECK(!login_url.is_empty());

  profile_ = ProfileManager::GetDefaultProfile();

  webui_login_ = new DOMView();
  AddChildView(webui_login_);
  webui_login_->Init(profile_, NULL);
  webui_login_->LoadURL(login_url);
  webui_login_->SetVisible(true);

  InitStatusArea();

  registrar_.Add(this,
                 NotificationType::FOCUS_CHANGED_IN_PAGE,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::TAB_CONTENTS_DESTROYED,
                 NotificationService::AllSources());
}

// static
views::Widget* WebUILoginView::CreateWindowContainingView(
    const gfx::Rect& bounds,
    const GURL& login_url,
    WebUILoginView** view) {
  views::Widget* window = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  window->Init(params);
  *view = new WebUILoginView();
  (*view)->Init(login_url);

  window->SetContentsView(*view);

  (*view)->UpdateWindowType();

  return window;
}

std::string WebUILoginView::GetClassName() const {
  return kViewClassName;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::FocusWillChange(views::View* focused_before,
                                     views::View* focused_now) {
  VirtualKeyboardType before = DecideKeyboardStateForView(focused_before);
  VirtualKeyboardType now = DecideKeyboardStateForView(focused_now);
  if (before != now) {
    // TODO(varunjain): support other types of keyboard.
    UpdateKeyboardAndLayout(now == GENERIC);
  }
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  const int kCornerPadding = 5;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - kCornerPadding,
      kCornerPadding,
      status_area_size.width(),
      status_area_size.height());

  if (webui_login_)
    webui_login_->SetBoundsRect(bounds());

  // TODO(rharrison): Hide touch specific code behind TOUCH_UI defines
  if (!keyboard_)
    return;

  keyboard_->SetVisible(keyboard_showing_);
  gfx::Rect keyboard_bounds = bounds();
  keyboard_bounds.set_y(keyboard_bounds.height() - kKeyboardHeight);
  keyboard_bounds.set_height(kKeyboardHeight);
  keyboard_->SetBoundsRect(keyboard_bounds);
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

Profile* WebUILoginView::GetProfile() const {
  return NULL;
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

void WebUILoginView::OnDialogClosed() {
}

void WebUILoginView::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
  SchedulePaint();
}

// WebUILoginView private: -----------------------------------------------------

void WebUILoginView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  AddChildView(status_area_);
}

void WebUILoginView::UpdateWindowType() {
  std::vector<int> params;
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(GetNativeWindow()),
      WM_IPC_WINDOW_LOGIN_WEBUI,
      &params);
}

void WebUILoginView::InitVirtualKeyboard() {
  if (keyboard_)
    return;

  keyboard_ = new KeyboardContainerView(profile_, NULL);
  keyboard_->SetVisible(false);
  AddChildView(keyboard_);
}

void WebUILoginView::UpdateKeyboardAndLayout(bool should_show_keyboard) {
  if (should_show_keyboard)
    InitVirtualKeyboard();

  if (should_show_keyboard == keyboard_showing_)
    return;

  DCHECK(keyboard_);

  keyboard_showing_ = should_show_keyboard;
  Layout();
}

WebUILoginView::VirtualKeyboardType
    WebUILoginView::DecideKeyboardStateForView(views::View* view) {
  if (!view)
    return NONE;

  std::string cname = view->GetClassName();
  if (cname == views::Textfield::kViewClassName) {
    return GENERIC;
  } else if (cname == RenderWidgetHostViewViews::kViewClassName) {
    TabContents* contents = webui_login_->tab_contents();
    bool* editable = contents ? GetFocusedStateAccessor()->GetProperty(
        contents->property_bag()) : NULL;
    if (editable && *editable)
      return GENERIC;
  }
  return NONE;
}

void WebUILoginView::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::FOCUS_CHANGED_IN_PAGE) {
    // Only modify the keyboard state if the currently active tab sent the
    // notification.
    const TabContents* current_tab = webui_login_->tab_contents();
    TabContents* source_tab = Source<TabContents>(source).ptr();
    const bool editable = *Details<const bool>(details).ptr();

    if (current_tab == source_tab && TabContentsHasFocus(source_tab))
      UpdateKeyboardAndLayout(editable);

    // Save the state of the focused field so that the keyboard visibility
    // can be determined after tab switching.
    GetFocusedStateAccessor()->SetProperty(
        source_tab->property_bag(), editable);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    GetFocusedStateAccessor()->DeleteProperty(
        Source<TabContents>(source).ptr()->property_bag());
  }
}

}  // namespace chromeos
