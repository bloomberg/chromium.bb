// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/env.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::WebContents;

namespace {

// These strings must be kept in sync with handleAccelerator() in oobe.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnrollment[] = "enrollment";
const char kAccelNameVersion[] = "version";
const char kAccelNameReset[] = "reset";

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
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
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
    webui_->CallJavascriptFunction("login.GaiaSigninScreen.onFrameError",
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

// static
const char WebUILoginView::kViewClassName[] =
    "browser/chromeos/login/WebUILoginView";

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : webui_login_(NULL),
      login_window_(NULL),
      host_window_frozen_(false),
      is_hidden_(false),
      login_prompt_visible_handled_(false),
      should_emit_login_prompt_visible_(true),
      forward_keyboard_event_(true) {
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
  accel_map_[ui::Accelerator(ui::VKEY_R,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameReset;

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i)
    AddAccelerator(i->first);
}

WebUILoginView::~WebUILoginView() {
  if (ash::Shell::GetInstance()->HasPrimaryStatusArea()) {
    ash::Shell::GetInstance()->GetPrimarySystemTray()->
        SetNextFocusableView(NULL);
  }
}

void WebUILoginView::Init(views::Widget* login_window) {
  login_window_ = login_window;
  webui_login_ = new views::WebView(ProfileManager::GetDefaultProfile());
  AddChildView(webui_login_);

  WebContents* web_contents = webui_login_->GetWebContents();

  // Create the password manager that is needed for the proxy.
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents);
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents, PasswordManagerDelegateImpl::FromWebContents(web_contents));

  // LoginHandlerViews uses a constrained window for the password manager view.
  WebContentsModalDialogManager::CreateForWebContents(web_contents);

  web_contents->SetDelegate(this);
  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents->GetMutableRendererPrefs(),
      ProfileManager::GetDefaultProfile());

  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
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

  // TODO(nkostylev): Use WebContentsObserver::RenderViewCreated to track
  // when RenderView is created.
  // Use a background with transparency to trigger transparency in Webkit.
  SkBitmap background;
  background.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  background.allocPixels();
  background.eraseARGB(0x00, 0x00, 0x00, 0x00);
  content::RenderViewHost* host = GetWebContents()->GetRenderViewHost();
  host->GetView()->SetBackground(background);
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

void WebUILoginView::OnPostponedShow() {
  set_is_hidden(false);
  OnLoginPromptVisible();
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
  if (ash::Shell::GetInstance()->HasPrimaryStatusArea()) {
    ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
    if (visible) {
      // Tray may have been initialized being hidden.
      tray->SetVisible(visible);
      tray->GetWidget()->Show();
    } else {
      tray->GetWidget()->Hide();
    }
  }
}

void WebUILoginView::SetUIEnabled(bool enabled) {
  forward_keyboard_event_ = enabled;
  ash::Shell::GetInstance()->GetPrimarySystemTray()->SetEnabled(enabled);
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
  webui_login_->web_contents()->GetView()->Focus();
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
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
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

void WebUILoginView::HandleKeyboardEvent(content::WebContents* source,
                                         const NativeWebKeyboardEvent& event) {
  if (forward_keyboard_event_) {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                          GetFocusManager());
  }

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

bool WebUILoginView::TakeFocus(content::WebContents* source, bool reverse) {
  // In case of blocked UI (ex.: sign in is in progress)
  // we should not process focus change events.
  if (!forward_keyboard_event_)
    return false;

  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  if (tray && tray->GetWidget()->IsVisible()) {
    tray->SetNextFocusableView(this);
    ash::Shell::GetInstance()->RotateFocus(reverse ? ash::Shell::BACKWARD :
                                                     ash::Shell::FORWARD);
  }

  return true;
}

void WebUILoginView::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  if (MediaStreamInfoBarDelegate::Create(web_contents, request, callback))
    NOTREACHED() << "Media stream not allowed for WebUI";
}

void WebUILoginView::OnLoginPromptVisible() {
  // If we're hidden than will generate this signal once we're shown.
  if (is_hidden_ || login_prompt_visible_handled_) {
    LOG(INFO) << "Login WebUI >> not emitting signal, hidden: " << is_hidden_;
    return;
  }
  TRACE_EVENT0("chromeos", "WebUILoginView::OnLoginPromoptVisible");
  if (should_emit_login_prompt_visible_) {
    LOG(INFO) << "Login WebUI >> login-prompt-visible";
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        EmitLoginPromptVisible();
  }
  login_prompt_visible_handled_ = true;

  // Let RenderWidgetHostViewAura::OnPaint() show white background when
  // loading page and when backing store is not present.
  aura::Env::GetInstance()->set_render_white_bg(true);
}

void WebUILoginView::ReturnFocus(bool reverse) {
  // Return the focus to the web contents.
  webui_login_->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
}

}  // namespace chromeos
