// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::WebContents;

namespace {

const char kViewClassName[] = "browser/chromeos/login/WebUILoginView";

// These strings must be kept in sync with handleAccelerator() in oobe.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnrollment[] = "enrollment";
const char kAccelNameVersion[] = "version";

// Observes IPC messages from the FrameSniffer and notifies JS if error
// appears.
class SnifferObserver : public content::RenderViewHostObserver {
 public:
  SnifferObserver(RenderViewHost* host, content::WebUI* webui)
      : content::RenderViewHostObserver(host), webui_(webui) {
    DCHECK(webui_);
    Send(new ChromeViewMsg_StartFrameSniffer(routing_id(),
                                             UTF8ToUTF16("gaia-frame")));
  }

  virtual ~SnifferObserver() {}

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SnifferObserver, message)
      IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FrameLoadingError, OnError)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  void OnError(int error) {
    base::FundamentalValue error_value(error);
    webui_->CallJavascriptFunction("login.ErrorMessageScreen.onFrameError",
                                   error_value);
  }

  content::WebUI* webui_;
};

// A View class which places its first child at the right most position.
class RightAlignedView : public views::View {
 public:
  virtual void Layout() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
};

void RightAlignedView::Layout() {
  if (has_children()) {
    views::View* child = child_at(0);
    gfx::Size preferred_size = child->GetPreferredSize();
    child->SetBounds(width() - preferred_size.width(),
                     0, preferred_size.width(), preferred_size.height());
  }
}

void RightAlignedView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

}  // namespace

namespace chromeos {

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : webui_login_(NULL),
      login_window_(NULL),
      host_window_frozen_(false),
      should_emit_login_prompt_visible_(true) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());

  accel_map_[ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)] =
      kAccelNameCancel;
  accel_map_[ui::Accelerator(ui::VKEY_E,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameEnrollment;
  accel_map_[ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)] =
      kAccelNameVersion;

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i)
    AddAccelerator(i->first);
}

WebUILoginView::~WebUILoginView() {
  ash::SystemTray* tray = ash::Shell::GetInstance()->system_tray();
  if (tray)
    tray->SetNextFocusableView(NULL);
}

void WebUILoginView::Init(views::Widget* login_window) {
  login_window_ = login_window;
  webui_login_ = new views::WebView(ProfileManager::GetDefaultProfile());
  AddChildView(webui_login_);

  // We create the WebContents ourselves because the TabContents assumes
  // ownership of it. This should be reworked once we don't need to use the
  // TabContents here.
  WebContents* web_contents =
      WebContents::Create(ProfileManager::GetDefaultProfile(),
                          NULL,
                          MSG_ROUTING_NONE,
                          NULL,
                          NULL);
  tab_contents_.reset(new TabContents(web_contents));
  webui_login_->SetWebContents(web_contents);

  web_contents->SetDelegate(this);
  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents->GetMutableRendererPrefs(),
      ProfileManager::GetDefaultProfile());

  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 content::Source<WebContents>(web_contents));
}

std::string WebUILoginView::GetClassName() const {
  return kViewClassName;
}

bool WebUILoginView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  AccelMap::const_iterator entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end())
    return false;

  if (!webui_login_)
    return true;

  content::WebUI* web_ui = GetWebUI();
  if (web_ui) {
    base::StringValue accel_name(entry->second);
    web_ui->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                   accel_name);
  }

  return true;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::OnWindowCreated() {
}

void WebUILoginView::UpdateWindowType() {
}

void WebUILoginView::LoadURL(const GURL & url) {
  webui_login_->LoadInitialURL(url);
  webui_login_->RequestFocus();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  // Only enable transparency on sign in screen, not on lock screen.
  if (BaseLoginDisplayHost::default_host() &&
      command_line->HasSwitch(switches::kEnableNewOobe)) {
    // TODO(nkostylev): Use WebContentsObserver::RenderViewCreated to track
    // when RenderView is created.
    // Use a background with transparency to trigger transparency in Webkit.
    SkBitmap background;
    background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    background.allocPixels();
    background.eraseARGB(0x00, 0x00, 0x00, 0x00);
    content::RenderViewHost* host =
        tab_contents_->web_contents()->GetRenderViewHost();
    host->GetView()->SetBackground(background);
  }
}

content::WebUI* WebUILoginView::GetWebUI() {
  return webui_login_->web_contents()->GetWebUI();
}

content::WebContents* WebUILoginView::GetWebContents() {
  return webui_login_->web_contents();
}

void WebUILoginView::OpenProxySettings() {
  ProxySettingsDialog* dialog =
      new ProxySettingsDialog(NULL, GetNativeWindow());
  dialog->Show();
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
  ash::SystemTray* tray = ash::Shell::GetInstance()->system_tray();
  if (tray) {
    if (visible)
      tray->GetWidget()->Show();
    else
      tray->GetWidget()->Hide();
  }
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(webui_login_);
  webui_login_->SetBoundsRect(bounds());
}

void WebUILoginView::OnLocaleChanged() {
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  Layout();
  SchedulePaint();
}

void WebUILoginView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return the focus to the web contents.
  webui_login_->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
}

void WebUILoginView::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE: {
      OnLoginPromptVisible();
      registrar_.RemoveAll();
      break;
    }
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      RenderViewHost* render_view_host =
          content::Details<RenderViewHost>(details).ptr();
      new SnifferObserver(render_view_host, GetWebUI());
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

// WebUILoginView private: -----------------------------------------------------

bool WebUILoginView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Do not show the context menu.
#ifndef NDEBUG
  return false;
#else
  return true;
#endif
}

void WebUILoginView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());

  // Make sure error bubble is cleared on keyboard event. This is needed
  // when the focus is inside an iframe. Only clear on KeyDown to prevent hiding
  // an immediate authentication error (See crbug.com/103643).
  if (event.type == WebKit::WebInputEvent::KeyDown) {
    content::WebUI* web_ui = GetWebUI();
    if (web_ui)
      web_ui->CallJavascriptFunction("cr.ui.Oobe.clearErrors");
  }
}

bool WebUILoginView::IsPopupOrPanel(const WebContents* source) const {
  return true;
}

bool WebUILoginView::TakeFocus(bool reverse) {
  ash::SystemTray* tray = ash::Shell::GetInstance()->system_tray();
  if (tray && tray->GetWidget()->IsVisible()) {
    tray->SetNextFocusableView(this);
    ash::Shell::GetInstance()->RotateFocus(reverse ? ash::Shell::BACKWARD :
                                                     ash::Shell::FORWARD);
  }

  return true;
}

void WebUILoginView::OnLoginPromptVisible() {
  if (should_emit_login_prompt_visible_) {
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        EmitLoginPromptVisible();
  }

  OobeUI* oobe_ui = static_cast<OobeUI*>(GetWebUI()->GetController());
  // Notify OOBE that the login frame has been rendered. Currently
  // this is used to start camera presence check.
  oobe_ui->OnLoginPromptVisible();
}

void WebUILoginView::ReturnFocus(bool reverse) {
  // Return the focus to the web contents.
  webui_login_->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
}

}  // namespace chromeos
