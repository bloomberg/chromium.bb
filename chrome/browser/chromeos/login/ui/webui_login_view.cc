// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/webui_login_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/render_messages.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace {

// These strings must be kept in sync with handleAccelerator()
// in display_manager.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnrollment[] = "enrollment";
const char kAccelNameKioskEnable[] = "kiosk_enable";
const char kAccelNameVersion[] = "version";
const char kAccelNameReset[] = "reset";
const char kAccelFocusPrev[] = "focus_prev";
const char kAccelFocusNext[] = "focus_next";
const char kAccelNameDeviceRequisition[] = "device_requisition";
const char kAccelNameDeviceRequisitionRemora[] = "device_requisition_remora";
const char kAccelNameDeviceRequisitionShark[] = "device_requisition_shark";
const char kAccelNameAppLaunchBailout[] = "app_launch_bailout";
const char kAccelNameAppLaunchNetworkConfig[] = "app_launch_network_config";
const char kAccelNameShowRollbackOption[] = "show_rollback_on_reset_screen";
const char kAccelNameHideRollbackOption[] = "hide_rollback_on_reset_screen";

// A class to change arrow key traversal behavior when it's alive.
class ScopedArrowKeyTraversal {
 public:
  explicit ScopedArrowKeyTraversal(bool new_arrow_key_tranversal_enabled)
      : previous_arrow_key_traversal_enabled_(
            views::FocusManager::arrow_key_traversal_enabled()) {
    views::FocusManager::set_arrow_key_traversal_enabled(
        new_arrow_key_tranversal_enabled);
  }
  ~ScopedArrowKeyTraversal() {
    views::FocusManager::set_arrow_key_traversal_enabled(
        previous_arrow_key_traversal_enabled_);
  }

 private:
  const bool previous_arrow_key_traversal_enabled_;
  DISALLOW_COPY_AND_ASSIGN(ScopedArrowKeyTraversal);
};

}  // namespace

namespace chromeos {

// static
const char WebUILoginView::kViewClassName[] =
    "browser/chromeos/login/WebUILoginView";

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView()
    : webui_login_(NULL),
      is_hidden_(false),
      webui_visible_(false),
      should_emit_login_prompt_visible_(true),
      forward_keyboard_event_(true) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                 content::NotificationService::AllSources());

  accel_map_[ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)] =
      kAccelNameCancel;
  accel_map_[ui::Accelerator(ui::VKEY_E,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameEnrollment;
  accel_map_[ui::Accelerator(ui::VKEY_K,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameKioskEnable;
  accel_map_[ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)] =
      kAccelNameVersion;
  accel_map_[ui::Accelerator(ui::VKEY_R,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameReset;

  accel_map_[ui::Accelerator(ui::VKEY_LEFT, ui::EF_NONE)] =
      kAccelFocusPrev;
  accel_map_[ui::Accelerator(ui::VKEY_RIGHT, ui::EF_NONE)] =
      kAccelFocusNext;

  // Use KEY_RELEASED because Gaia consumes KEY_PRESSED for up/down key.
  ui::Accelerator key_up(ui::VKEY_UP, ui::EF_NONE);
  key_up.set_type(ui::ET_KEY_RELEASED);
  ui::Accelerator key_down(ui::VKEY_DOWN, ui::EF_NONE);
  key_down.set_type(ui::ET_KEY_RELEASED);
  accel_map_[key_up] = kAccelFocusPrev;
  accel_map_[key_down] = kAccelFocusNext;

  accel_map_[ui::Accelerator(
      ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameDeviceRequisition;
  accel_map_[
      ui::Accelerator(ui::VKEY_H, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameDeviceRequisitionRemora;
  accel_map_[
      ui::Accelerator(ui::VKEY_H,
          ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameDeviceRequisitionShark;

  accel_map_[ui::Accelerator(ui::VKEY_S,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameAppLaunchBailout;

  accel_map_[ui::Accelerator(ui::VKEY_N,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameAppLaunchNetworkConfig;

  ui::Accelerator show_rollback(ui::VKEY_MENU, ui::EF_ALT_DOWN);
  show_rollback.set_type(ui::ET_KEY_PRESSED);
  accel_map_[show_rollback] = kAccelNameShowRollbackOption;

  ui::Accelerator hide_rollback(ui::VKEY_MENU, ui::EF_NONE);
  hide_rollback.set_type(ui::ET_KEY_RELEASED);
  accel_map_[hide_rollback] = kAccelNameHideRollbackOption;

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i)
    AddAccelerator(i->first);
}

WebUILoginView::~WebUILoginView() {
  FOR_EACH_OBSERVER(web_modal::ModalDialogHostObserver,
                    observer_list_,
                    OnHostDestroying());

#if !defined(USE_ATHENA)
  if (ash::Shell::GetInstance()->HasPrimaryStatusArea()) {
    ash::Shell::GetInstance()->GetPrimarySystemTray()->
        SetNextFocusableView(NULL);
  }
#endif
}

void WebUILoginView::Init() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  auth_extension_.reset(new ScopedGaiaAuthExtension(signin_profile));
  webui_login_ = new views::WebView(signin_profile);
  webui_login_->set_allow_accelerators(true);
  AddChildView(webui_login_);

  WebContents* web_contents = webui_login_->GetWebContents();

  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from a captive portal window
  // that may be opened on top of this UI.
  SessionTabHelper::CreateForWebContents(web_contents);

  // Create the password manager that is needed for the proxy.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));

  // LoginHandlerViews uses a constrained window for the password manager view.
  WebContentsModalDialogManager::CreateForWebContents(web_contents);
  WebContentsModalDialogManager::FromWebContents(web_contents)->
      SetDelegate(this);
  if (!popup_manager_.get())
    popup_manager_.reset(new web_modal::PopupManager(this));
  popup_manager_->RegisterWith(web_contents);

  web_contents->SetDelegate(this);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  WebContentsObserver::Observe(web_contents);
  renderer_preferences_util::UpdateFromSystemSettings(
      web_contents->GetMutableRendererPrefs(),
      signin_profile);
}

const char* WebUILoginView::GetClassName() const {
  return kViewClassName;
}

void WebUILoginView::RequestFocus() {
  webui_login_->RequestFocus();
}

web_modal::WebContentsModalDialogHost*
    WebUILoginView::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView WebUILoginView::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point WebUILoginView::GetDialogPosition(const gfx::Size& size) {
  // Center the widget.
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(widget_size.width() / 2 - size.width() / 2,
                    widget_size.height() / 2 - size.height() / 2);
}

gfx::Size WebUILoginView::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void WebUILoginView::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  if (observer && !observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WebUILoginView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
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

void WebUILoginView::LoadURL(const GURL & url) {
  webui_login_->LoadInitialURL(url);
  webui_login_->RequestFocus();

  // TODO(nkostylev): Use WebContentsObserver::RenderViewCreated to track
  // when RenderView is created.
  GetWebContents()->GetRenderViewHost()->GetView()->SetBackgroundOpaque(false);
}

content::WebUI* WebUILoginView::GetWebUI() {
  return webui_login_->web_contents()->GetWebUI();
}

content::WebContents* WebUILoginView::GetWebContents() {
  return webui_login_->web_contents();
}

void WebUILoginView::OpenProxySettings() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!network) {
    LOG(ERROR) << "No default network found!";
    return;
  }
  ProxySettingsDialog* dialog =
      new ProxySettingsDialog(ProfileHelper::GetSigninProfile(),
                              *network, NULL, GetNativeWindow());
  dialog->Show();
}

void WebUILoginView::OnPostponedShow() {
  set_is_hidden(false);
  OnLoginPromptVisible();
}

void WebUILoginView::SetStatusAreaVisible(bool visible) {
#if !defined(USE_ATHENA)
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
#endif
}

void WebUILoginView::SetUIEnabled(bool enabled) {
  forward_keyboard_event_ = enabled;
#if !defined(USE_ATHENA)
  ash::Shell::GetInstance()->GetPrimarySystemTray()->SetEnabled(enabled);
#endif
}

void WebUILoginView::AddFrameObserver(FrameObserver* frame_observer) {
  DCHECK(frame_observer);
  DCHECK(!frame_observer_list_.HasObserver(frame_observer));
  frame_observer_list_.AddObserver(frame_observer);
}

void WebUILoginView::RemoveFrameObserver(FrameObserver* frame_observer) {
  DCHECK(frame_observer);
  DCHECK(frame_observer_list_.HasObserver(frame_observer));
  frame_observer_list_.RemoveObserver(frame_observer);
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(webui_login_);
  webui_login_->SetBoundsRect(bounds());

  FOR_EACH_OBSERVER(web_modal::ModalDialogHostObserver,
                    observer_list_,
                    OnPositionRequiresUpdate());
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
  webui_login_->web_contents()->Focus();
}

void WebUILoginView::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE:
    case chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN: {
      OnLoginPromptVisible();
      registrar_.RemoveAll();
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
    // Disable arrow key traversal because arrow keys are handled via
    // accelerator when this view has focus.
    ScopedArrowKeyTraversal arrow_key_traversal(false);

    unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                          GetFocusManager());
  }

  // Make sure error bubble is cleared on keyboard event. This is needed
  // when the focus is inside an iframe. Only clear on KeyDown to prevent hiding
  // an immediate authentication error (See crbug.com/103643).
  if (event.type == blink::WebInputEvent::KeyDown) {
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

#if !defined(USE_ATHENA)
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  if (tray && tray->GetWidget()->IsVisible()) {
    tray->SetNextFocusableView(this);
    ash::Shell::GetInstance()->RotateFocus(reverse ? ash::Shell::BACKWARD :
                                                    ash::Shell::FORWARD);
  }
#endif

  return true;
}

void WebUILoginView::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  if (MediaStreamInfoBarDelegate::Create(web_contents, request, callback))
    NOTREACHED() << "Media stream not allowed for WebUI";
}

bool WebUILoginView::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(web_contents, security_origin, type);
}

bool WebUILoginView::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void WebUILoginView::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  FOR_EACH_OBSERVER(FrameObserver,
                    frame_observer_list_,
                    OnFrameError(render_frame_host->GetFrameName()));
  if (render_frame_host->GetFrameName() != "gaia-frame")
    return;

  GetWebUI()->CallJavascriptFunction("login.GaiaSigninScreen.onFrameError",
                                     base::FundamentalValue(-error_code),
                                     base::StringValue(validated_url.spec()));
}

void WebUILoginView::OnLoginPromptVisible() {
  // If we're hidden than will generate this signal once we're shown.
  if (is_hidden_ || webui_visible_) {
    VLOG(1) << "Login WebUI >> not emitting signal, hidden: " << is_hidden_;
    return;
  }
  TRACE_EVENT0("chromeos", "WebUILoginView::OnLoginPromoptVisible");
  if (should_emit_login_prompt_visible_) {
    VLOG(1) << "Login WebUI >> login-prompt-visible";
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        EmitLoginPromptVisible();
  }

  webui_visible_ = true;
}

void WebUILoginView::ReturnFocus(bool reverse) {
  // Return the focus to the web contents.
  webui_login_->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
}

}  // namespace chromeos
