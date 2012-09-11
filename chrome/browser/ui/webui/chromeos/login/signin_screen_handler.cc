// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ash/wm/power_button_controller.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;

namespace {

const char kDefaultDomain[] = "@gmail.com";

// Account picker screen id.
const char kAccountPickerScreen[] = "account-picker";
// Sign in screen id for GAIA extension hosted content.
const char kGaiaSigninScreen[] = "gaia-signin";
// Start page of GAIA authentication extension.
const char kGaiaExtStartPage[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html";
// Same as above but offline version.
const char kGaiaExtStartPageOffline[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/offline.html";

// User dictionary keys.
const char kKeyUsername[] = "username";
const char kKeyDisplayName[] = "displayName";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyNameTooltip[] = "nameTooltip";
const char kKeySignedIn[] = "signedIn";
const char kKeyCanRemove[] = "canRemove";
const char kKeyOauthTokenStatus[] = "oauthTokenStatus";

// Max number of users to show.
const size_t kMaxUsers = 5;

// Timeout to smooth temporary network state transitions for flaky networks.
const int kNetworkStateCheckDelayMs = 5000;

const char kReasonNetworkChanged[] = "network changed";
const char kReasonProxyChanged[] = "proxy changed";
const char kReasonPortalDetected[] = "portal detected";

// The Task posted to PostTaskAndReply in StartClearingDnsCache on the IO
// thread.
void ClearDnsCache(IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (browser_shutdown::IsTryingToQuit())
    return;

  io_thread->ClearHostCache();
}

}  // namespace

namespace chromeos {

namespace {

// Updates params dictionary passed to the auth extension with related
// preferences from CrosSettings.
void UpdateAuthParamsFromSettings(DictionaryValue* params,
                                  const CrosSettings* cros_settings) {
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  bool allow_guest = true;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  // Account creation depends on Guest sign-in (http://crosbug.com/24570).
  params->SetBoolean("createAccount", allow_new_user && allow_guest);
  params->SetBoolean("guestSignin", allow_guest);
}

} //  namespace

// Class which observes network state changes and calls registered callbacks.
// State is considered changed if connection or the active network has been
// changed. Also, it answers to the requests about current network state.
class NetworkStateInformer
    : public chromeos::NetworkLibrary::NetworkManagerObserver,
      public content::NotificationObserver,
      public CaptivePortalWindowProxyDelegate {
 public:
  NetworkStateInformer(SigninScreenHandler* handler, content::WebUI* web_ui);
  virtual ~NetworkStateInformer();

  void Init();

  // Adds observer's callback to be called when network state has been changed.
  void AddObserver(const std::string& callback);

  // Removes observer's callback.
  void RemoveObserver(const std::string& callback);

  // Sends current network state, network name, reason and last network type
  // using the callback.
  void SendState(const std::string& callback, const std::string& reason);

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* cros) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CaptivePortalWindowProxyDelegate implementation:
  virtual void OnPortalDetected() OVERRIDE;

  // Returns active network's ID. It can be used to uniquely
  // identify the network.
  std::string active_network_id() {
    return last_online_network_id_;
  }

  bool is_online() { return state_ == ONLINE; }

 private:
  enum State {OFFLINE, ONLINE, CAPTIVE_PORTAL};

  bool UpdateState(chromeos::NetworkLibrary* cros);
  void UpdateStateAndNotify();

  void SendStateToObservers(const std::string& reason);

  content::NotificationRegistrar registrar_;
  base::hash_set<std::string> observers_;
  ConnectionType last_network_type_;
  std::string network_name_;
  State state_;
  SigninScreenHandler* handler_;
  content::WebUI* web_ui_;
  base::CancelableClosure check_state_;
  std::string last_online_network_id_;
  std::string last_connected_network_id_;
};

// NetworkStateInformer implementation -----------------------------------------

NetworkStateInformer::NetworkStateInformer(SigninScreenHandler* handler,
                                           content::WebUI* web_ui)
    : last_network_type_(TYPE_WIFI),
      state_(OFFLINE),
      handler_(handler),
      web_ui_(web_ui) {
}

void NetworkStateInformer::Init() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  UpdateState(cros);
  cros->AddNetworkManagerObserver(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_PROXY_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
}

NetworkStateInformer::~NetworkStateInformer() {
  CrosLibrary::Get()->GetNetworkLibrary()->
      RemoveNetworkManagerObserver(this);
}

void NetworkStateInformer::AddObserver(const std::string& callback) {
  observers_.insert(callback);
}

void NetworkStateInformer::RemoveObserver(const std::string& callback) {
  observers_.erase(callback);
}

void NetworkStateInformer::SendState(const std::string& callback,
                                     const std::string& reason) {
  base::FundamentalValue state_value(state_);
  base::StringValue network_value(network_name_);
  base::StringValue reason_value(reason);
  base::FundamentalValue last_network_value(last_network_type_);
  web_ui_->CallJavascriptFunction(callback, state_value, network_value,
                                  reason_value, last_network_value);
}

void NetworkStateInformer::OnNetworkManagerChanged(NetworkLibrary* cros) {
  State new_state = OFFLINE;
  std::string new_network_id;
  const Network* active_network = cros->active_network();
  if (active_network) {
    if (active_network->online()) {
      new_state = ONLINE;
      new_network_id = active_network->unique_id();
    } else if (active_network->restricted_pool()) {
      new_state = CAPTIVE_PORTAL;
    }
  }

  if ((state_ != ONLINE && new_state == ONLINE) ||
      (state_ == ONLINE && new_state == ONLINE &&
       new_network_id != last_online_network_id_)) {
    if (new_state == ONLINE)
      last_online_network_id_ = new_network_id;
    // Transitions {OFFLINE, PORTAL} -> ONLINE and connections to different
    // network are processed without delay.
    UpdateStateAndNotify();
  } else {
    check_state_.Cancel();
    check_state_.Reset(
        base::Bind(&NetworkStateInformer::UpdateStateAndNotify,
                   base::Unretained(this)));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        check_state_.callback(),
        base::TimeDelta::FromMilliseconds(kNetworkStateCheckDelayMs));
  }
}

void NetworkStateInformer::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SESSION_STARTED)
    registrar_.RemoveAll();
  else if (type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED)
    SendStateToObservers(kReasonProxyChanged);
  else
    NOTREACHED() << "Unknown notification: " << type;
}

void NetworkStateInformer::OnPortalDetected() {
  SendStateToObservers(kReasonPortalDetected);
}

bool NetworkStateInformer::UpdateState(NetworkLibrary* cros) {
  State new_state = OFFLINE;
  std::string new_network_id;
  network_name_.clear();

  const Network* active_network = cros->active_network();
  if (active_network) {
    if (active_network->online())
      new_state = ONLINE;
    else if (active_network->restricted_pool())
      new_state = CAPTIVE_PORTAL;

    new_network_id = active_network->unique_id();
    network_name_ = active_network->name();
    last_network_type_ = active_network->type();
  }

  bool updated = (new_state != state_) ||
      (new_state != OFFLINE && new_network_id != last_connected_network_id_);
  state_ = new_state;
  if (state_ != OFFLINE)
    last_connected_network_id_ = new_network_id;

  if (updated && state_ == ONLINE)
    handler_->OnNetworkReady();

  return updated;
}

void NetworkStateInformer::UpdateStateAndNotify() {
  // Cancel pending update request if any.
  check_state_.Cancel();

  if (UpdateState(CrosLibrary::Get()->GetNetworkLibrary()))
    SendStateToObservers(kReasonNetworkChanged);
}

void NetworkStateInformer::SendStateToObservers(const std::string& reason) {
  for (base::hash_set<std::string>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    SendState(*it, reason);
  }
}

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler()
    : delegate_(NULL),
      show_on_init_(false),
      oobe_ui_(false),
      focus_stolen_(false),
      gaia_silent_load_(false),
      is_account_picker_showing_first_time_(false),
      dns_cleared_(false),
      dns_clear_task_running_(false),
      cookies_cleared_(false),
      cookie_remover_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      webui_visible_(false) {
  CrosSettings::Get()->AddSettingsObserver(kAccountsPrefAllowNewUser, this);
  CrosSettings::Get()->AddSettingsObserver(kAccountsPrefAllowGuest, this);
}

SigninScreenHandler::~SigninScreenHandler() {
  weak_factory_.InvalidateWeakPtrs();
  if (cookie_remover_)
    cookie_remover_->RemoveObserver(this);
  SystemKeyEventListener* key_event_listener =
      SystemKeyEventListener::GetInstance();
  if (key_event_listener)
    key_event_listener->RemoveCapsLockObserver(this);
  if (delegate_)
    delegate_->SetWebUIHandler(NULL);
  CrosSettings::Get()->RemoveSettingsObserver(kAccountsPrefAllowNewUser, this);
  CrosSettings::Get()->RemoveSettingsObserver(kAccountsPrefAllowGuest, this);
}

void SigninScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("signinScreenTitle",
      l10n_util::GetStringUTF16(IDS_SIGNIN_SCREEN_TITLE));
  localized_strings->SetString("signinScreenPasswordChanged",
      l10n_util::GetStringUTF16(IDS_SIGNIN_SCREEN_PASSWORD_CHANGED));
  localized_strings->SetString("passwordHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  localized_strings->SetString("removeButtonAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_REMOVE_BUTTON_ACCESSIBLE_NAME));
  localized_strings->SetString("passwordFieldAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME));
  localized_strings->SetString("signedIn",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_ACTIVE_USER));
  localized_strings->SetString("signinButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_BUTTON));
  localized_strings->SetString("enterGuestButton",
      l10n_util::GetStringUTF16(IDS_ENTER_GUEST_SESSION_BUTTON));
  localized_strings->SetString("enterGuestButtonAccessibleName",
      l10n_util::GetStringUTF16(
          IDS_ENTER_GUEST_SESSION_BUTTON_ACCESSIBLE_NAME));
  localized_strings->SetString("shutDown",
      l10n_util::GetStringUTF16(IDS_SHUTDOWN_BUTTON));
  localized_strings->SetString("addUser",
      l10n_util::GetStringUTF16(IDS_ADD_USER_BUTTON));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("signOutUser",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT));
  localized_strings->SetString("addUserErrorMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_ADD_USER_OFFLINE));
  localized_strings->SetString("offlineMessageTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_TITLE));
  localized_strings->SetString("offlineMessageBody",
      l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_MESSAGE));
  localized_strings->SetString("captivePortalTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE));
  localized_strings->SetString("captivePortalMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL));
  localized_strings->SetString("captivePortalProxyMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY));
  localized_strings->SetString("captivePortalNetworkSelect",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT));
  localized_strings->SetString("proxyMessageText",
      l10n_util::GetStringUTF16(IDS_LOGIN_PROXY_ERROR_MESSAGE));
  localized_strings->SetString("createAccount",
      l10n_util::GetStringUTF16(IDS_CREATE_ACCOUNT_HTML));
  localized_strings->SetString("guestSignin",
      l10n_util::GetStringUTF16(IDS_BROWSE_WITHOUT_SIGNING_IN_HTML));
  localized_strings->SetString("offlineLogin",
      l10n_util::GetStringUTF16(IDS_OFFLINE_LOGIN_HTML));
  localized_strings->SetString("removeUser",
      l10n_util::GetStringUTF16(IDS_LOGIN_REMOVE));
  localized_strings->SetString("disabledAddUserTooltip",
      l10n_util::GetStringUTF16(
          g_browser_process->browser_policy_connector()->IsEnterpriseManaged() ?
            IDS_DISABLED_ADD_USER_TOOLTIP_ENTERPRISE :
            IDS_DISABLED_ADD_USER_TOOLTIP));

  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled()) {
    localized_strings->SetString("demoLoginMessage",
        l10n_util::GetStringUTF16(IDS_KIOSK_MODE_LOGIN_MESSAGE));
  }
}

void SigninScreenHandler::Show(bool oobe_ui) {
  CHECK(delegate_);
  oobe_ui_ = oobe_ui;
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (oobe_ui) {
    // Shows new user sign-in for OOBE.
    HandleShowAddUser(NULL);
  } else {
    // Populates account picker. Animation is turned off for now until we
    // figure out how to make it fast enough.
    SendUserList(false);

    // Reset Caps Lock state when login screen is shown.
    input_method::InputMethodManager::GetInstance()->GetXKeyboard()->
        SetCapsLockEnabled(false);

    DictionaryValue params;
    params.SetBoolean("disableAddUser",
                      DoRestrictedUsersMatchExistingOnScreen());
    ShowScreen(kAccountPickerScreen, &params);
  }
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetWebUIHandler(this);
}

void SigninScreenHandler::OnNetworkReady() {
  MaybePreloadAuthExtension();
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::Initialize() {
  // If delegate_ is NULL here (e.g. WebUIScreenLocker has been destroyed),
  // don't do anything, just return.
  if (!delegate_)
    return;

  // Register for Caps Lock state change notifications;
  SystemKeyEventListener* key_event_listener =
      SystemKeyEventListener::GetInstance();
  if (key_event_listener)
    key_event_listener->AddCapsLockObserver(this);

  if (show_on_init_) {
    show_on_init_ = false;
    Show(oobe_ui_);
  }
}

gfx::NativeWindow SigninScreenHandler::GetNativeWindow() {
  return delegate_ ? delegate_->GetNativeWindow() : NULL;
}

void SigninScreenHandler::RegisterMessages() {
  network_state_informer_.reset(new NetworkStateInformer(this, web_ui()));
  network_state_informer_->Init();

  web_ui()->RegisterMessageCallback("authenticateUser",
      base::Bind(&SigninScreenHandler::HandleAuthenticateUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("completeLogin",
      base::Bind(&SigninScreenHandler::HandleCompleteLogin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getUsers",
      base::Bind(&SigninScreenHandler::HandleGetUsers,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("launchDemoUser",
      base::Bind(&SigninScreenHandler::HandleLaunchDemoUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("launchIncognito",
      base::Bind(&SigninScreenHandler::HandleLaunchIncognito,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("fixCaptivePortal",
      base::Bind(&SigninScreenHandler::HandleFixCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showCaptivePortal",
        base::Bind(&SigninScreenHandler::HandleShowCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("hideCaptivePortal",
      base::Bind(&SigninScreenHandler::HandleHideCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("offlineLogin",
      base::Bind(&SigninScreenHandler::HandleOfflineLogin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showAddUser",
      base::Bind(&SigninScreenHandler::HandleShowAddUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("shutdownSystem",
      base::Bind(&SigninScreenHandler::HandleShutdownSystem,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("userSelectedDelayed",
      base::Bind(&SigninScreenHandler::HandleUserSelected,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("userDeselected",
      base::Bind(&SigninScreenHandler::HandleUserDeselected,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeUser",
      base::Bind(&SigninScreenHandler::HandleRemoveUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("toggleEnrollmentScreen",
      base::Bind(&SigninScreenHandler::HandleToggleEnrollmentScreen,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("launchHelpApp",
      base::Bind(&SigninScreenHandler::HandleLaunchHelpApp,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createAccount",
      base::Bind(&SigninScreenHandler::HandleCreateAccount,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("accountPickerReady",
      base::Bind(&SigninScreenHandler::HandleAccountPickerReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("wallpaperReady",
      base::Bind(&SigninScreenHandler::HandleWallpaperReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginWebuiReady",
      base::Bind(&SigninScreenHandler::HandleLoginWebuiReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginRequestNetworkState",
      base::Bind(&SigninScreenHandler::HandleLoginRequestNetworkState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginAddNetworkStateObserver",
      base::Bind(&SigninScreenHandler::HandleLoginAddNetworkStateObserver,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginRemoveNetworkStateObserver",
      base::Bind(&SigninScreenHandler::HandleLoginRemoveNetworkStateObserver,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("demoWebuiReady",
      base::Bind(&SigninScreenHandler::HandleDemoWebuiReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("signOutUser",
      base::Bind(&SigninScreenHandler::HandleSignOutUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("userImagesLoaded",
      base::Bind(&SigninScreenHandler::HandleUserImagesLoaded,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("networkErrorShown",
      base::Bind(&SigninScreenHandler::HandleNetworkErrorShown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openProxySettings",
      base::Bind(&SigninScreenHandler::HandleOpenProxySettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginVisible",
      base::Bind(&SigninScreenHandler::HandleLoginVisible,
                 base::Unretained(this)));
}

void SigninScreenHandler::HandleGetUsers(const base::ListValue* args) {
  SendUserList(false);
}

void SigninScreenHandler::ClearAndEnablePassword() {
  base::FundamentalValue force_online(false);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.resetSigninUI", force_online);
}

void SigninScreenHandler::OnLoginSuccess(const std::string& username) {
  base::StringValue username_value(username);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.onLoginSuccess", username_value);
}

void SigninScreenHandler::OnUserRemoved(const std::string& username) {
  SendUserList(false);
}

void SigninScreenHandler::OnUserImageChanged(const User& user) {
  if (!page_is_ready())
    return;

  base::StringValue user_email(user.email());
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.updateUserImage", user_email);
}

void SigninScreenHandler::OnPreferencesChanged() {
  if (delegate_ && !delegate_->IsShowUsers()) {
    HandleShowAddUser(NULL);
  } else {
    SendUserList(false);
    ShowScreen(kAccountPickerScreen, NULL);
  }
}

void SigninScreenHandler::ShowError(int login_attempts,
                                    const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  base::FundamentalValue login_attempts_value(login_attempts);
  base::StringValue error_message(error_text);
  base::StringValue help_link(help_link_text);
  base::FundamentalValue help_id(static_cast<int>(help_topic_id));
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showSignInError",
                                   login_attempts_value,
                                   error_message,
                                   help_link,
                                   help_id);
}

void SigninScreenHandler::ShowGaiaPasswordChanged(const std::string& username) {
  email_ = username;
  password_changed_for_.insert(email_);
  base::StringValue email_value(email_);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showSigninUI", email_value);
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.updateUserGaiaNeeded", email_value);
}

void SigninScreenHandler::ResetSigninScreenHandlerDelegate() {
  SetDelegate(NULL);
}

void SigninScreenHandler::OnBrowsingDataRemoverDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cookie_remover_ = NULL;
  cookies_cleared_ = true;
  ShowSigninScreenIfReady();
}

void SigninScreenHandler::OnCapsLockChange(bool enabled) {
  if (page_is_ready()) {
    base::FundamentalValue capsLockState(enabled);
    web_ui()->CallJavascriptFunction(
        "login.AccountPickerScreen.setCapsLockState", capsLockState);
  }
}

void SigninScreenHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED) {
    UpdateAuthExtension();
    UpdateAddButtonStatus();
  } else {
    NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::OnDnsCleared() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  dns_clear_task_running_ = false;
  dns_cleared_ = true;
  ShowSigninScreenIfReady();
}

void SigninScreenHandler::ShowSigninScreenIfReady() {
  if (!dns_cleared_ || !cookies_cleared_)
    return;

  if (gaia_silent_load_ &&
      (!network_state_informer_->is_online() ||
       gaia_silent_load_network_ !=
          network_state_informer_->active_network_id())) {
    // Network has changed. Force Gaia reload.
    gaia_silent_load_ = false;
    // Gaia page will be realoded, so focus isn't stolen anymore.
    focus_stolen_ = false;
  }

  LoadAuthExtension(!gaia_silent_load_, false, false);
  ShowScreen(kGaiaSigninScreen, NULL);

  if (gaia_silent_load_) {
    // The variable is assigned to false because silently loaded Gaia page was
    // used.
    gaia_silent_load_ = false;
    if (focus_stolen_)
      HandleLoginWebuiReady(NULL);
  }
}

void SigninScreenHandler::LoadAuthExtension(
    bool force, bool silent_load, bool offline) {
  DictionaryValue params;

  params.SetBoolean("forceReload", force);
  params.SetBoolean("silentLoad", silent_load);
  params.SetBoolean("isLocal", offline);
  params.SetBoolean("passwordChanged",
                    !email_.empty() && password_changed_for_.count(email_));
  if (delegate_)
    params.SetBoolean("isShowUsers", delegate_->IsShowUsers());
  params.SetString("startUrl",
                   offline ? kGaiaExtStartPageOffline : kGaiaExtStartPage);
  params.SetString("email", email_);
  email_.clear();

  UpdateAuthParamsFromSettings(&params, CrosSettings::Get());

  if (!offline) {
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    if (!app_locale.empty())
      params.SetString("hl", app_locale);
  } else {
    base::DictionaryValue *localized_strings = new base::DictionaryValue();
    localized_strings->SetString("stringEmail",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMAIL));
    localized_strings->SetString("stringPassword",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_PASSWORD));
    localized_strings->SetString("stringSignIn",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_SIGNIN));
    localized_strings->SetString("stringError",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_ERROR));
    params.Set("localizedStrings", localized_strings);
  }

  params.SetString("gaiaOrigin", GaiaUrls::GetInstance()->gaia_origin_url());
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGaiaUrlPath)) {
    params.SetString("gaiaUrlPath",
                     command_line->GetSwitchValueASCII(switches::kGaiaUrlPath));
  }

  // Test automation data:
  if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
    if (!test_user_.empty()) {
      params.SetString("test_email", test_user_);
      test_user_.clear();
    }
    if (!test_pass_.empty()) {
      params.SetString("test_password", test_pass_);
      test_pass_.clear();
    }
  }
  web_ui()->CallJavascriptFunction("login.GaiaSigninScreen.loadAuthExtension",
                                   params);
}

void SigninScreenHandler::UpdateAuthExtension() {
  DictionaryValue params;
  UpdateAuthParamsFromSettings(&params, CrosSettings::Get());
  web_ui()->CallJavascriptFunction("login.GaiaSigninScreen.updateAuthExtension",
                                   params);
}

void SigninScreenHandler::UpdateAddButtonStatus() {
  base::FundamentalValue disabled(DoRestrictedUsersMatchExistingOnScreen());
  web_ui()->CallJavascriptFunction(
      "cr.ui.login.DisplayManager.updateAddUserButtonStatus", disabled);
}

void SigninScreenHandler::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  VLOG(2) << "ShowSigninScreenForCreds " << username << " " << password;

  test_user_ = username;
  test_pass_ = password;
  HandleShowAddUser(NULL);
}

void SigninScreenHandler::HandleCompleteLogin(const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string typed_email;
  std::string password;
  if (!args->GetString(0, &typed_email) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  typed_email = gaia::SanitizeEmail(typed_email);
  delegate_->SetDisplayEmail(typed_email);
  delegate_->CompleteLogin(typed_email, password);
}

void SigninScreenHandler::HandleAuthenticateUser(const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string username;
  std::string password;
  if (!args->GetString(0, &username) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  username = gaia::SanitizeEmail(username);
  delegate_->Login(username, password);
}

void SigninScreenHandler::HandleLaunchDemoUser(const base::ListValue* args) {
  if (delegate_)
    delegate_->LoginAsDemoUser();
}

void SigninScreenHandler::HandleLaunchIncognito(const base::ListValue* args) {
  if (delegate_)
    delegate_->LoginAsGuest();
}

void SigninScreenHandler::HandleFixCaptivePortal(const base::ListValue* args) {
  if (!delegate_)
    return;
  // TODO(altimofeev): move error page and captive portal window showing logic
  // to C++ (currenly most of it is done on the JS side).
  if (!captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(network_state_informer_.get(),
                                     GetNativeWindow()));
  }
  captive_portal_window_proxy_->ShowIfRedirected();
}

void SigninScreenHandler::HandleShowCaptivePortal(const base::ListValue* args) {
  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  HandleFixCaptivePortal(args);
  captive_portal_window_proxy_->Show();
}

void SigninScreenHandler::HandleHideCaptivePortal(const base::ListValue* args) {
  if (captive_portal_window_proxy_.get())
    captive_portal_window_proxy_->Close();
}

void SigninScreenHandler::HandleOfflineLogin(const base::ListValue* args) {
  if (!delegate_ || delegate_->IsShowUsers()) {
    NOTREACHED();
    return;
  }
  if (!args->GetString(0, &email_))
    email_.clear();

  // Load auth extension. Parameters are: force reload, do not load extension in
  // background, use offline version.
  LoadAuthExtension(true, false, true);
  ShowScreen(kGaiaSigninScreen, NULL);
}

void SigninScreenHandler::HandleShutdownSystem(const base::ListValue* args) {
#if defined(USE_AURA)
  // Display the shutdown animation before actually requesting shutdown.
  ash::Shell::GetInstance()->power_button_controller()->RequestShutdown();
#else
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
#endif
}

void SigninScreenHandler::HandleUserDeselected(const base::ListValue* args) {
  if (delegate_)
    delegate_->OnUserDeselected();
}

void SigninScreenHandler::HandleUserSelected(const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string email;
  if (!args->GetString(0, &email)) {
    NOTREACHED();
    return;
  }

  delegate_->OnUserSelected(email);
}

void SigninScreenHandler::HandleRemoveUser(const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string email;
  if (!args->GetString(0, &email)) {
    NOTREACHED();
    return;
  }

  delegate_->RemoveUser(email);
  UpdateAddButtonStatus();
}

void SigninScreenHandler::HandleShowAddUser(const base::ListValue* args) {
  email_.clear();
  // |args| can be null if it's OOBE.
  if (args)
    args->GetString(0, &email_);
  is_account_picker_showing_first_time_ = false;

  if (gaia_silent_load_ && email_.empty()) {
    dns_cleared_ = true;
    cookies_cleared_ = true;
    ShowSigninScreenIfReady();
  } else {
    StartClearingDnsCache();
    StartClearingCookies();
  }
}

void SigninScreenHandler::HandleToggleEnrollmentScreen(
    const base::ListValue* args) {
  if (delegate_)
    delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleLaunchHelpApp(const base::ListValue* args) {
  if (!delegate_)
    return;
  double help_topic_id;  // Javascript number is passed back as double.
  if (!args->GetDouble(0, &help_topic_id)) {
    NOTREACHED();
    return;
  }

  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(
      static_cast<HelpAppLauncher::HelpTopic>(help_topic_id));
}

void SigninScreenHandler::SendUserList(bool animated) {
  if (!delegate_)
    return;
  bool show_guest = delegate_->IsShowGuest();

  size_t max_non_owner_users = show_guest ? kMaxUsers - 2 : kMaxUsers - 1;
  size_t non_owner_count = 0;

  ListValue users_list;
  const UserList& users = delegate_->GetUsers();

  bool single_user = users.size() == 1;
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    const std::string& email = (*it)->email();
    std::string owner;
    chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
    bool is_owner = (email == owner);
    bool signed_in = UserManager::Get()->IsUserLoggedIn() &&
        email == UserManager::Get()->GetLoggedInUser().email();

    if (non_owner_count < max_non_owner_users || is_owner) {
      DictionaryValue* user_dict = new DictionaryValue();
      user_dict->SetString(kKeyUsername, email);
      user_dict->SetString(kKeyEmailAddress, (*it)->display_email());
      user_dict->SetString(kKeyDisplayName, (*it)->GetDisplayName());
      user_dict->SetString(kKeyNameTooltip, (*it)->display_email());
      user_dict->SetInteger(kKeyOauthTokenStatus, (*it)->oauth_token_status());
      user_dict->SetBoolean(kKeySignedIn, signed_in);

      // Single user check here is necessary because owner info might not be
      // available when running into login screen on first boot.
      // See http://crosbug.com/12723
      user_dict->SetBoolean(kKeyCanRemove,
                            !single_user &&
                            !email.empty() &&
                            !is_owner &&
                            !signed_in);

      users_list.Append(user_dict);
      if (!is_owner)
        ++non_owner_count;
    }
  }

  if (show_guest) {
    // Add the Guest to the user list.
    DictionaryValue* guest_dict = new DictionaryValue();
    guest_dict->SetString(kKeyUsername, "");
    guest_dict->SetString(kKeyEmailAddress, "");
    guest_dict->SetString(kKeyDisplayName,
                          l10n_util::GetStringUTF16(IDS_GUEST));
    guest_dict->SetBoolean(kKeyCanRemove, false);
    guest_dict->SetInteger(kKeyOauthTokenStatus,
                           User::OAUTH_TOKEN_STATUS_UNKNOWN);
    users_list.Append(guest_dict);
  }

  // Call the Javascript callback
  base::FundamentalValue animated_value(animated);
  web_ui()->CallJavascriptFunction("login.AccountPickerScreen.loadUsers",
                                   users_list, animated_value);
}

void SigninScreenHandler::HandleAccountPickerReady(
    const base::ListValue* args) {
  is_account_picker_showing_first_time_ = true;
  MaybePreloadAuthExtension();

  if (ScreenLocker::default_screen_locker()) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOCK_WEBUI_READY,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }
}

void SigninScreenHandler::HandleWallpaperReady(
    const base::ListValue* args) {
  if (ScreenLocker::default_screen_locker()) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOCK_BACKGROUND_DISPLAYED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }
}

void SigninScreenHandler::HandleLoginWebuiReady(const base::ListValue* args) {
  if (focus_stolen_) {
    // Set focus to the Gaia page.
    // TODO(altimofeev): temporary solution, until focus parameters are
    // implemented on the Gaia side.
    // Do this only once. Any subsequent call would relod GAIA frame.
    focus_stolen_ = false;
    const char code[] = "gWindowOnLoad();";
    RenderViewHost* rvh = web_ui()->GetWebContents()->GetRenderViewHost();
    rvh->ExecuteJavascriptInWebFrame(
        ASCIIToUTF16("//iframe[@id='signin-frame']\n//iframe"),
        ASCIIToUTF16(code));
  }
  if (!gaia_silent_load_) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_WEBUI_LOADED,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  } else {
    focus_stolen_ = true;
    // Prevent focus stealing by the Gaia page.
    // TODO(altimofeev): temporary solution, until focus parameters are
    // implemented on the Gaia side.
    const char code[] = "var gWindowOnLoad = window.onload; "
                        "window.onload=function() {};";
    RenderViewHost* rvh = web_ui()->GetWebContents()->GetRenderViewHost();
    rvh->ExecuteJavascriptInWebFrame(
        ASCIIToUTF16("//iframe[@id='signin-frame']\n//iframe"),
        ASCIIToUTF16(code));
  }
}

void SigninScreenHandler::HandleLoginRequestNetworkState(
    const base::ListValue* args) {
  std::string callback;
  std::string reason;
  if (!args->GetString(0, &callback) || !args->GetString(1, &reason)) {
    NOTREACHED();
    return;
  }
  network_state_informer_->SendState(callback, reason);
}

void SigninScreenHandler::HandleLoginAddNetworkStateObserver(
    const base::ListValue* args) {
  std::string callback;
  if (!args->GetString(0, &callback)) {
    NOTREACHED();
    return;
  }
  network_state_informer_->AddObserver(callback);
}

void SigninScreenHandler::HandleLoginRemoveNetworkStateObserver(
    const base::ListValue* args) {
  std::string callback;
  if (!args->GetString(0, &callback)) {
    NOTREACHED();
    return;
  }
  network_state_informer_->RemoveObserver(callback);
}

void SigninScreenHandler::HandleDemoWebuiReady(const base::ListValue* args) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_DEMO_WEBUI_LOADED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void SigninScreenHandler::HandleSignOutUser(const base::ListValue* args) {
  if (delegate_)
    delegate_->Signout();
}

void SigninScreenHandler::HandleUserImagesLoaded(const base::ListValue* args) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_IMAGES_LOADED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void SigninScreenHandler::HandleNetworkErrorShown(const base::ListValue* args) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void SigninScreenHandler::HandleCreateAccount(const base::ListValue* args) {
  if (delegate_)
    delegate_->CreateAccount();
}

void SigninScreenHandler::HandleOpenProxySettings(const base::ListValue* args) {
  BaseLoginDisplayHost::default_host()->OpenProxySettings();
}

void SigninScreenHandler::HandleLoginVisible(const base::ListValue* args) {
  LOG(INFO) << "Login WebUI >> LoginVisible, webui_visible_: "
            << webui_visible_;
  if (!webui_visible_) {
    // There might be multiple messages from OOBE UI so send notifications after
    // the first one only.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }
  webui_visible_ = true;
  if (ScreenLocker::default_screen_locker())
    web_ui()->CallJavascriptFunction("login.AccountPickerScreen.setWallpaper");
}

void SigninScreenHandler::StartClearingDnsCache() {
  if (dns_clear_task_running_ || !g_browser_process->io_thread())
    return;

  dns_cleared_ = false;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ClearDnsCache, g_browser_process->io_thread()),
      base::Bind(&SigninScreenHandler::OnDnsCleared,
                 weak_factory_.GetWeakPtr()));
  dns_clear_task_running_ = true;
}

void SigninScreenHandler::StartClearingCookies() {
  cookies_cleared_ = false;
  if (cookie_remover_)
    cookie_remover_->RemoveObserver(this);

  cookie_remover_ = BrowsingDataRemover::CreateForUnboundedRange(
      Profile::FromWebUI(web_ui()));
  cookie_remover_->AddObserver(this);
  cookie_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA,
                          BrowsingDataHelper::UNPROTECTED_WEB);
}

void SigninScreenHandler::MaybePreloadAuthExtension() {
  // Fetching of the extension is not started before account picker page is
  // loaded because it can affect the loading speed. Also if |cookie_remover_|
  // or |dns_clear_task_running_| then auth extension showing has already been
  // initiated and preloading is senseless.
  // Do not load the extension for the screen locker, see crosbug.com/25018.
  if (is_account_picker_showing_first_time_ &&
      !gaia_silent_load_ &&
      !ScreenLocker::default_screen_locker() &&
      !cookie_remover_ &&
      !dns_clear_task_running_ &&
      network_state_informer_->is_online()) {
    gaia_silent_load_ = true;
    gaia_silent_load_network_ = network_state_informer_->active_network_id();
    LoadAuthExtension(true, true, false);
  }
}

bool SigninScreenHandler::DoRestrictedUsersMatchExistingOnScreen() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  const UserList& users = UserManager::Get()->GetUsers();
  if (!delegate_ || users.size() > kMaxUsers - delegate_->IsShowGuest()) {
    return false;
  }
  const base::ListValue* existing = NULL;
  if (!cros_settings->GetList(kAccountsPrefUsers, &existing) ||
      !existing || users.size() != existing->GetSize()) {
    return false;
  }
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if (!cros_settings->FindEmailInList(kAccountsPrefUsers, (*it)->email())) {
      return false;
    }
  }
  return true;
}

}  // namespace chromeos
