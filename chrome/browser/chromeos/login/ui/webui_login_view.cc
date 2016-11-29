// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/webui_login_view.h"

#include "ash/common/focus_cycler.h"
#include "ash/common/system/status_area_widget_delegate.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/login/ui/web_contents_set_background_color.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/view_type_utils.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/widget.h"

using content::NativeWebKeyboardEvent;
using content::RenderViewHost;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace {

// These strings must be kept in sync with handleAccelerator()
// in display_manager.js.
const char kAccelNameCancel[] = "cancel";
const char kAccelNameEnableDebugging[] = "debugging";
const char kAccelNameEnrollment[] = "enrollment";
// TODO(rsorokin): Remove custom Active Directory shortcut for the launch.
const char kAccelNameEnrollmentAd[] = "enrollment_ad";
const char kAccelNameKioskEnable[] = "kiosk_enable";
const char kAccelNameVersion[] = "version";
const char kAccelNameReset[] = "reset";
const char kAccelNameDeviceRequisition[] = "device_requisition";
const char kAccelNameDeviceRequisitionRemora[] = "device_requisition_remora";
const char kAccelNameDeviceRequisitionShark[] = "device_requisition_shark";
const char kAccelNameAppLaunchBailout[] = "app_launch_bailout";
const char kAccelNameAppLaunchNetworkConfig[] = "app_launch_network_config";
const char kAccelNameToggleEasyBootstrap[] = "toggle_easy_bootstrap";
const char kAccelNameBootstrappingSlave[] = "bootstrapping_slave";

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

// A helper method returns status area widget delegate if exists,
// otherwise nullptr.
ash::StatusAreaWidgetDelegate* GetStatusAreaWidgetDelegate() {
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  return tray ? static_cast<ash::StatusAreaWidgetDelegate*>(
                    tray->GetWidget()->GetContentsView())
              : nullptr;
}

}  // namespace

namespace chromeos {

// static
const char WebUILoginView::kViewClassName[] =
    "browser/chromeos/login/WebUILoginView";

// WebUILoginView::CycleFocusTraversable ---------------------------------------
class WebUILoginView::CycleFocusTraversable : public views::FocusTraversable {
 public:
  explicit CycleFocusTraversable(WebUILoginView* webui_login_view)
      : cycle_focus_search_(webui_login_view, true, false) {}
  ~CycleFocusTraversable() override {}

  // views::FocusTraversable
  views::FocusSearch* GetFocusSearch() override { return &cycle_focus_search_; }

  views::FocusTraversable* GetFocusTraversableParent() override {
    return nullptr;
  }

  views::View* GetFocusTraversableParentView() override { return nullptr; }

 private:
  views::FocusSearch cycle_focus_search_;

  DISALLOW_COPY_AND_ASSIGN(CycleFocusTraversable);
};

// WebUILoginView::StatusAreaFocusTraversable ----------------------------------
class WebUILoginView::StatusAreaFocusTraversable
    : public views::FocusTraversable {
 public:
  StatusAreaFocusTraversable(
      ash::StatusAreaWidgetDelegate* status_area_widget_delegate,
      WebUILoginView* webui_login_view)
      : webui_login_view_(webui_login_view),
        status_area_focus_search_(status_area_widget_delegate, false, false) {}
  ~StatusAreaFocusTraversable() override {}

  // views::FocusTraversable
  views::FocusSearch* GetFocusSearch() override {
    return &status_area_focus_search_;
  }

  views::FocusTraversable* GetFocusTraversableParent() override {
    return webui_login_view_->cycle_focus_traversable_.get();
  }

  views::View* GetFocusTraversableParentView() override {
    return webui_login_view_->status_area_widget_host_;
  }

 private:
  WebUILoginView* const webui_login_view_;
  views::FocusSearch status_area_focus_search_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaFocusTraversable);
};

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView() {
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
  accel_map_[ui::Accelerator(
      ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameEnrollmentAd;
  accel_map_[ui::Accelerator(ui::VKEY_K,
                             ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)] =
      kAccelNameKioskEnable;
  accel_map_[ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)] =
      kAccelNameVersion;
  accel_map_[ui::Accelerator(ui::VKEY_R,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameReset;
  accel_map_[ui::Accelerator(ui::VKEY_X,
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameEnableDebugging;
  accel_map_[ui::Accelerator(
      ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameToggleEasyBootstrap;

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

  accel_map_[ui::Accelerator(
      ui::VKEY_S, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)] =
      kAccelNameBootstrappingSlave;

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i)
    AddAccelerator(i->first);
}

WebUILoginView::~WebUILoginView() {
  for (auto& observer : observer_list_)
    observer.OnHostDestroying();

  if (!chrome::IsRunningInMash() &&
      ash::Shell::GetInstance()->HasPrimaryStatusArea()) {
    views::Widget* tray_widget =
        ash::Shell::GetInstance()->GetPrimarySystemTray()->GetWidget();
    ash::StatusAreaWidgetDelegate* status_area_widget_delegate =
        static_cast<ash::StatusAreaWidgetDelegate*>(
            tray_widget->GetContentsView());
    status_area_widget_delegate->set_custom_focus_traversable(nullptr);
    status_area_widget_delegate->set_default_last_focusable_child(false);
  } else {
    NOTIMPLEMENTED();
  }
}

void WebUILoginView::Init() {
  Profile* signin_profile = ProfileHelper::GetSigninProfile();
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

  web_contents->SetDelegate(this);
  extensions::SetViewType(web_contents, extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  content::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, signin_profile, web_contents);

  status_area_widget_host_ = new views::View;
  AddChildView(status_area_widget_host_);
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
    web_ui->CallJavascriptFunctionUnsafe("cr.ui.Oobe.handleAccelerator",
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

  WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      GetWebContents(), SK_ColorTRANSPARENT);

  // There is no Shell instance while running in mash.
  if (chrome::IsRunningInMash())
    return;

  ash::StatusAreaWidgetDelegate* status_area_widget_delegate =
      GetStatusAreaWidgetDelegate();
  DCHECK(status_area_widget_delegate);
  cycle_focus_traversable_.reset(new CycleFocusTraversable(this));
  status_area_focus_traversable_.reset(
      new StatusAreaFocusTraversable(status_area_widget_delegate, this));
  status_area_widget_delegate->set_custom_focus_traversable(
      status_area_focus_traversable_.get());
}

content::WebUI* WebUILoginView::GetWebUI() {
  return webui_login_->web_contents()->GetWebUI();
}

content::WebContents* WebUILoginView::GetWebContents() {
  return webui_login_->web_contents();
}

OobeUI* WebUILoginView::GetOobeUI() {
  return static_cast<OobeUI*>(GetWebUI()->GetController());
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
  if (!chrome::IsRunningInMash() &&
      ash::Shell::GetInstance()->HasPrimaryStatusArea()) {
    ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
    tray->SetVisible(visible);
    tray->GetWidget()->SetOpacity(visible ? 1.0 : 0.0);
    if (visible) {
      tray->GetWidget()->Show();
    } else {
      tray->GetWidget()->Hide();
    }
  } else {
    NOTIMPLEMENTED();
  }
}

void WebUILoginView::SetUIEnabled(bool enabled) {
  forward_keyboard_event_ = enabled;
  if (chrome::IsRunningInMash()) {
    NOTIMPLEMENTED();
    return;
  }
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();

  // We disable the UI to prevent user from interracting with UI elements,
  // particullary with the system tray menu. However, in case if the system tray
  // bubble is opened at this point, it remains opened and interactive even
  // after SystemTray::SetEnabled(false) call, which can be dangerous
  // (http://crbug.com/497080). Close the menu to fix it. Calling
  // SystemTray::SetEnabled(false) guarantees, that the menu will not be opened
  // until the UI is enabled again.
  if (!enabled && tray->HasSystemBubble())
    tray->CloseSystemBubble();

  tray->SetEnabled(enabled);
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout() {
  DCHECK(webui_login_);
  webui_login_->SetBoundsRect(bounds());

  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
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
      web_ui->CallJavascriptFunctionUnsafe("cr.ui.Oobe.clearErrors");
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

  // Focus is accepted, but the Ash system tray is not available in Mash, so
  // exit early.
  if (chrome::IsRunningInMash())
    return true;

  ash::StatusAreaWidgetDelegate* status_area_widget_delegate =
      GetStatusAreaWidgetDelegate();
  if (status_area_widget_delegate &&
      status_area_widget_delegate->GetWidget()->IsVisible()) {
    status_area_widget_delegate->set_default_last_focusable_child(reverse);
    ash::WmShell::Get()->focus_cycler()->RotateFocus(
        reverse ? ash::FocusCycler::BACKWARD : ash::FocusCycler::FORWARD);
  }

  return true;
}

void WebUILoginView::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  MediaStreamDevicesController controller(web_contents, request, callback);
  if (!controller.IsAskingForAudio() && !controller.IsAskingForVideo())
    return;

  if (controller.IsAskingForAudio()) {
    controller.PermissionDenied();
    return;
  }

  const CrosSettings* const settings = CrosSettings::Get();
  if (!settings) {
    controller.PermissionDenied();
    return;
  }

  const base::Value* const raw_list_value =
      settings->GetPref(kLoginVideoCaptureAllowedUrls);
  if (!raw_list_value) {
    controller.PermissionDenied();
    return;
  }

  const base::ListValue* list_value;
  CHECK(raw_list_value->GetAsList(&list_value));
  for (const auto& base_value : *list_value) {
    std::string value;
    if (base_value->GetAsString(&value)) {
      ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(value);
      if (pattern == ContentSettingsPattern::Wildcard()) {
        LOG(WARNING) << "Ignoring wildcard URL pattern: " << value;
        continue;
      }
      if (pattern.IsValid() && pattern.Matches(request.security_origin)) {
        controller.PermissionGranted();
        return;
      }
    }
  }
  controller.PermissionDenied();
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

void WebUILoginView::OnLoginPromptVisible() {
  // If we're hidden than will generate this signal once we're shown.
  if (is_hidden_ || webui_visible_) {
    VLOG(1) << "Login WebUI >> not emitting signal, hidden: " << is_hidden_;
    return;
  }
  TRACE_EVENT0("chromeos", "WebUILoginView::OnLoginPromptVisible");
  if (should_emit_login_prompt_visible_) {
    VLOG(1) << "Login WebUI >> login-prompt-visible";
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        EmitLoginPromptVisible();
  }

  webui_visible_ = true;
}

}  // namespace chromeos
