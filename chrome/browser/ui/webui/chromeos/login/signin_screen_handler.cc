// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/error_screen_actor.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ash/wm/session_state_controller.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;

namespace {

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
const char kKeyEnterpriseDomain[] = "enterpriseDomain";
const char kKeyPublicAccount[] = "publicAccount";
const char kKeyLocallyManagedUser[] = "locallyManagedUser";
const char kKeySignedIn[] = "signedIn";
const char kKeyCanRemove[] = "canRemove";
const char kKeyIsOwner[] = "isOwner";
const char kKeyOauthTokenStatus[] = "oauthTokenStatus";

// Max number of users to show.
const size_t kMaxUsers = 18;

// Timeout to delay first notification about offline state for a
// current network.
const int kOfflineTimeoutSec = 5;

// Timeout used to prevent infinite connecting to a flaky network.
const int kConnectingTimeoutSec = 60;

// Type of the login screen UI that is currently presented to user.
const char kSourceGaiaSignin[] = "gaia-signin";
const char kSourceAccountPicker[] = "account-picker";

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

const char kNetworkStateOffline[] = "offline";
const char kNetworkStateOnline[] = "online";
const char kNetworkStateCaptivePortal[] = "behind captive portal";
const char kNetworkStateConnecting[] = "connecting";
const char kNetworkStateProxyAuthRequired[] = "proxy auth required";

const char* NetworkStateStatusString(NetworkStateInformer::State state) {
  switch (state) {
    case NetworkStateInformer::OFFLINE:
      return kNetworkStateOffline;
    case NetworkStateInformer::ONLINE:
      return kNetworkStateOnline;
    case NetworkStateInformer::CAPTIVE_PORTAL:
      return kNetworkStateCaptivePortal;
    case NetworkStateInformer::CONNECTING:
      return kNetworkStateConnecting;
    case NetworkStateInformer::PROXY_AUTH_REQUIRED:
      return kNetworkStateProxyAuthRequired;
    default:
      NOTREACHED();
      return NULL;
  }
}

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
  // TODO(nkostylev): Allow locally managed user creation only if:
  // 1. Enterprise managed device > is allowed by policy.
  // 2. Consumer device > owner exists.
  // g_browser_process->browser_policy_connector()->IsEnterpriseManaged()
  // const UserList& users = delegate_->GetUsers();
  // bool single_user = users.size() == 1;
  // chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  params->SetBoolean("createLocallyManagedUser",
                     command_line->HasSwitch(::switches::kEnableManagedUsers));
}

bool IsProxyError(NetworkStateInformer::State state,
                  const std::string& reason) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
      reason == ErrorScreenActor::kErrorReasonProxyAuthCancelled ||
      reason == ErrorScreenActor::kErrorReasonProxyConnectionFailed;
}

bool IsSigninScreen(const OobeUI::Screen screen) {
  return screen == OobeUI::SCREEN_GAIA_SIGNIN ||
      screen == OobeUI::SCREEN_ACCOUNT_PICKER;
}

// Returns true if |state| is related to the sign-in screen errors.
bool IsSigninScreenError(ErrorScreenActor::State state) {
  return state == ErrorScreenActor::STATE_PROXY_ERROR ||
      state == ErrorScreenActor::STATE_CAPTIVE_PORTAL_ERROR ||
      state == ErrorScreenActor::STATE_TIMEOUT_ERROR ||
      state == ErrorScreenActor::STATE_OFFLINE_ERROR;
}

// Returns a pointer to a Network instance by service path or NULL if
// network can not be found.
Network* FindNetworkByPath(const std::string& service_path) {
  CrosLibrary* cros = CrosLibrary::Get();
  if (!cros)
    return NULL;
  NetworkLibrary* network_library = cros->GetNetworkLibrary();
  if (!network_library)
    return NULL;
  return network_library->FindNetworkByPath(service_path);
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  Network* network = FindNetworkByPath(service_path);
  if (!network)
    return "";
  return network->name();
}

// Returns network unique id by service path.
std::string GetNetworkUniqueId(const std::string& service_path) {
  Network* network = FindNetworkByPath(service_path);
  if (!network)
    return "";
  return network->unique_id();
}

// Returns captive portal state for a network by its service path.
NetworkPortalDetector::CaptivePortalState GetCaptivePortalState(
    const std::string& service_path) {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  Network* network = FindNetworkByPath(service_path);
  if (!detector || !network)
    return NetworkPortalDetector::CaptivePortalState();
  return detector->GetCaptivePortalState(network);
}

void RecordDiscrepancyWithShill(
    const Network* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  if (network->online()) {
    UMA_HISTOGRAM_ENUMERATION(
        "CaptivePortal.OOBE.DiscrepancyWithShill_Online",
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  } else if (network->restricted_pool()) {
    UMA_HISTOGRAM_ENUMERATION(
        "CaptivePortal.OOBE.DiscrepancyWithShill_RestrictedPool",
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "CaptivePortal.OOBE.DiscrepancyWithShill_Offline",
        status,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);
  }
}

// Record state and descripancies with shill (e.g. shill thinks that
// network is online but NetworkPortalDetector claims that it's behind
// portal) for the network identified by |service_path|.
void RecordNetworkPortalDetectorStats(const std::string& service_path) {
  const Network* network = FindNetworkByPath(service_path);
  if (!network)
    return;
  NetworkPortalDetector::CaptivePortalState state =
      GetCaptivePortalState(service_path);
  if (state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN)
    return;

  UMA_HISTOGRAM_ENUMERATION("CaptivePortal.OOBE.DetectionResult",
                            state.status,
                            NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT);

  switch (state.status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
      NOTREACHED();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      if (network->online() || network->restricted_pool())
        RecordDiscrepancyWithShill(network, state.status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      if (!network->online())
        RecordDiscrepancyWithShill(network, state.status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      if (!network->restricted_pool())
        RecordDiscrepancyWithShill(network, state.status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      if (!network->online())
        RecordDiscrepancyWithShill(network, state.status);
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED();
      break;
  }
}

void EnableLazyDetection() {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (NetworkPortalDetector::IsEnabled() && detector)
    detector->EnableLazyDetection();
}

void DisableLazyDetection() {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (NetworkPortalDetector::IsEnabled() && detector)
    detector->DisableLazyDetection();
}

}  // namespace

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreenActor* error_screen_actor)
    : ui_state_(UI_STATE_UNKNOWN),
      delegate_(NULL),
      native_window_delegate_(NULL),
      show_on_init_(false),
      oobe_ui_(false),
      focus_stolen_(false),
      gaia_silent_load_(false),
      is_account_picker_showing_first_time_(false),
      dns_cleared_(false),
      dns_clear_task_running_(false),
      cookies_cleared_(false),
      network_state_informer_(network_state_informer),
      cookie_remover_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      webui_visible_(false),
      login_ui_active_(false),
      error_screen_actor_(error_screen_actor),
      is_first_update_state_call_(true),
      offline_login_active_(false),
      last_network_state_(NetworkStateInformer::UNKNOWN),
      has_pending_auth_ui_(false) {
  DCHECK(network_state_informer_);
  DCHECK(error_screen_actor_);
  network_state_informer_->AddObserver(this);
  CrosSettings::Get()->AddSettingsObserver(kAccountsPrefAllowNewUser, this);
  CrosSettings::Get()->AddSettingsObserver(kAccountsPrefAllowGuest, this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());
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
  network_state_informer_->RemoveObserver(this);
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
  localized_strings->SetString("podMenuButtonAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME));
  localized_strings->SetString("podMenuRemoveItemAccessibleName",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME));
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
  localized_strings->SetString("browseAsGuest",
      l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("signOutUser",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT));
  localized_strings->SetString("createAccount",
      l10n_util::GetStringUTF16(IDS_CREATE_ACCOUNT_HTML));
  localized_strings->SetString("guestSignin",
      l10n_util::GetStringUTF16(IDS_BROWSE_WITHOUT_SIGNING_IN_HTML));
  localized_strings->SetString("createLocallyManagedUser",
      l10n_util::GetStringUTF16(IDS_CREATE_LOCALLY_MANAGED_USER_HTML));
  localized_strings->SetString("offlineLogin",
      l10n_util::GetStringUTF16(IDS_OFFLINE_LOGIN_HTML));
  localized_strings->SetString("ownerUserPattern",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_OWNER_USER));
  localized_strings->SetString("removeUser",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_REMOVE_USER));
  localized_strings->SetString("errorTpmFailure",
      l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_TPM_FAILURE));
  localized_strings->SetString("errorTpmFailureReboot",
      l10n_util::GetStringFUTF16(
          IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  localized_strings->SetString("disabledAddUserTooltip",
      l10n_util::GetStringUTF16(
          g_browser_process->browser_policy_connector()->IsEnterpriseManaged() ?
            IDS_DISABLED_ADD_USER_TOOLTIP_ENTERPRISE :
            IDS_DISABLED_ADD_USER_TOOLTIP));

  // Strings used by password changed dialog.
  localized_strings->SetString("passwordChangedTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_TITLE));
  localized_strings->SetString("passwordChangedDesc",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_DESC));
  localized_strings->SetString("passwordChangedMoreInfo",
      l10n_util::GetStringFUTF16(
          IDS_LOGIN_PASSWORD_CHANGED_MORE_INFO,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME)));
  localized_strings->SetString("oldPasswordHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_OLD_PASSWORD_HINT));
  localized_strings->SetString("oldPasswordIncorrect",
        l10n_util::GetStringUTF16(
            IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD));
  localized_strings->SetString("passwordChangedCantRemember",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_CANT_REMEMBER));
  localized_strings->SetString("passwordChangedBackButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_BACK_BUTTON));
  localized_strings->SetString("passwordChangedsOkButton",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("passwordChangedProceedAnyway",
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY));
  localized_strings->SetString("proceedAnywayButton",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY_BUTTON));

  localized_strings->SetString("publicAccountInfoFormat",
      l10n_util::GetStringUTF16(IDS_LOGIN_PUBLIC_ACCOUNT_INFO_FORMAT));
  localized_strings->SetString("publicAccountReminder",
       l10n_util::GetStringUTF16(IDS_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER));
  localized_strings->SetString("publicAccountEnter",
       l10n_util::GetStringUTF16(IDS_LOGIN_PUBLIC_ACCOUNT_ENTER));
  localized_strings->SetString("publicAccountEnterAccessibleName",
       l10n_util::GetStringUTF16(
           IDS_LOGIN_PUBLIC_ACCOUNT_ENTER_ACCESSIBLE_NAME));

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
    input_method::GetInputMethodManager()->GetXKeyboard()->
        SetCapsLockEnabled(false);

    DictionaryValue params;
    params.SetBoolean("disableAddUser", AllWhitelistedUsersPresent());
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, &params);
  }
}

void SigninScreenHandler::ShowRetailModeLoginSpinner() {
  web_ui()->CallJavascriptFunction("showLoginSpinner");
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetWebUIHandler(this);
}

void SigninScreenHandler::SetNativeWindowDelegate(
    NativeWindowDelegate* native_window_delegate) {
  native_window_delegate_ = native_window_delegate;
}

void SigninScreenHandler::OnNetworkReady() {
  MaybePreloadAuthExtension();
}

void SigninScreenHandler::UpdateState(NetworkStateInformer::State state,
                                      const std::string& service_path,
                                      ConnectionType connection_type,
                                      const std::string& reason) {
  UpdateStateInternal(state, service_path, connection_type, reason, false);
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::UpdateUIState(UIState ui_state,
                                        DictionaryValue* params) {
  switch (ui_state) {
    case UI_STATE_GAIA_SIGNIN:
      ui_state_ = UI_STATE_GAIA_SIGNIN;
      ShowScreen(OobeUI::kScreenGaiaSignin, params);
      break;
    case UI_STATE_ACCOUNT_PICKER:
      ui_state_ = UI_STATE_ACCOUNT_PICKER;
      ShowScreen(OobeUI::kScreenAccountPicker, params);
      break;
    case UI_STATE_LOCALLY_MANAGED_USER_CREATION:
      ui_state_ = UI_STATE_LOCALLY_MANAGED_USER_CREATION;
      ShowScreen(OobeUI::kScreenManagedUserCreationDialog, params);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// TODO (ygorshenin@): split this method into small parts.
void SigninScreenHandler::UpdateStateInternal(
    NetworkStateInformer::State state,
    const std::string service_path,
    ConnectionType connection_type,
    const std::string reason,
    bool force_update) {
  // Skip "update" notification about OFFLINE state from
  // NetworkStateInformer if previous notification already was
  // delayed.
  if (state == NetworkStateInformer::OFFLINE &&
      reason == ErrorScreenActor::kErrorReasonUpdate &&
      !update_state_closure_.IsCancelled()) {
    return;
  }

  std::string network_id = GetNetworkUniqueId(service_path);
  // TODO (ygorshenin@): switch log level to INFO once signin screen
  // will be tested well.
  LOG(WARNING) << "SigninScreenHandler::UpdateStateInternal(): "
               << "state=" << NetworkStateStatusString(state) << ", "
               << "network_id=" << network_id << ", "
               << "connection_type=" << connection_type << ", "
               << "reason=" << reason << ", "
               << "force_update=" << force_update;
  update_state_closure_.Cancel();

  // Delay UpdateStateInternal() call in the following cases:
  // 1. this is the first call and it's about offline state of the
  //    current network. This can happen because device is just powered
  //    up and network stack isn't ready now.
  // 2. proxy auth ui is displayed. UpdateStateCall() is delayed to
  //    prevent screen change behind proxy auth ui.
  if ((state == NetworkStateInformer::OFFLINE && is_first_update_state_call_) ||
      has_pending_auth_ui_) {
    is_first_update_state_call_ = false;
    update_state_closure_.Reset(
        base::Bind(
            &SigninScreenHandler::UpdateStateInternal,
            weak_factory_.GetWeakPtr(),
            state, service_path, connection_type, reason, force_update));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        update_state_closure_.callback(),
        base::TimeDelta::FromSeconds(kOfflineTimeoutSec));
    return;
  }
  is_first_update_state_call_ = false;

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_closure_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_closure_.Reset(
          base::Bind(&SigninScreenHandler::UpdateStateInternal,
                     weak_factory_.GetWeakPtr(),
                     state, service_path, connection_type, reason, true));
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          connecting_closure_.callback(),
          base::TimeDelta::FromSeconds(kConnectingTimeoutSec));
    }
    return;
  }
  connecting_closure_.Cancel();

  bool is_online = (state == NetworkStateInformer::ONLINE);
  bool is_under_captive_portal =
      (state == NetworkStateInformer::CAPTIVE_PORTAL);
  bool is_proxy_error = IsProxyError(state, reason);
  bool is_gaia_loading_timeout =
      (reason == ErrorScreenActor::kErrorReasonLoadingTimeout);
  bool is_gaia_signin =
      (IsSigninScreen(GetCurrentScreen()) || IsSigninScreenHiddenByError()) &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
  bool is_gaia_reloaded = false;
  bool error_screen_should_overlay = !offline_login_active_ && IsGaiaLogin();

  // Reload frame if network is changed.
  if (reason == ErrorScreenActor::kErrorReasonNetworkChanged &&
      is_online && last_network_state_ != NetworkStateInformer::ONLINE &&
      is_gaia_signin && !is_gaia_reloaded) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry page load since network has been changed.";
    ReloadGaiaScreen();
    is_gaia_reloaded = true;
  }
  last_network_state_ = state;

  if (reason == ErrorScreenActor::kErrorReasonProxyConfigChanged &&
      error_screen_should_overlay && is_gaia_signin && !is_gaia_reloaded) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry page load since proxy settings has been changed.";
    ReloadGaiaScreen();
    is_gaia_reloaded = true;
  }

  if (reason == ErrorScreenActor::kErrorReasonLoadingTimeout)
    is_online = false;

  // Portal was detected via generate_204 redirect on Chrome side.
  // Subsequent call to show dialog if it's already shown does nothing.
  if (reason == ErrorScreenActor::kErrorReasonPortalDetected) {
    is_online = false;
    is_under_captive_portal = true;
  }

  if (is_online || !is_under_captive_portal)
    error_screen_actor_->HideCaptivePortal();

  if ((!is_online || is_gaia_loading_timeout) && is_gaia_signin &&
      !offline_login_active_) {
    SetupAndShowOfflineMessage(state, service_path, connection_type, reason,
                               is_proxy_error, is_under_captive_portal,
                               is_gaia_loading_timeout);
  } else {
    HideOfflineMessage(state, service_path, reason, is_gaia_signin,
                       is_gaia_reloaded);
  }
}

void SigninScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer:: State state,
    const std::string& service_path,
    ConnectionType connection_type,
    const std::string& reason,
    bool is_proxy_error,
    bool is_under_captive_portal,
    bool is_gaia_loading_timeout) {
  std::string network_id = GetNetworkUniqueId(service_path);
  LOG(WARNING) << "Show offline message: "
               << "state=" << NetworkStateStatusString(state) << ", "
               << "network_id=" << network_id << ", "
               << "reason=" << reason << ", "
               << "is_under_captive_portal=" << is_under_captive_portal;

  // Record portal detection stats only if we're going to show or
  // change state of the error screen.
  RecordNetworkPortalDetectorStats(service_path);

  if (is_proxy_error) {
    error_screen_actor_->ShowProxyError();
  } else if (is_under_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsGaiaLogin() ||
        (error_screen_actor_->state() !=
         ErrorScreenActor::STATE_CAPTIVE_PORTAL_ERROR)) {
      error_screen_actor_->FixCaptivePortal();
    }
    std::string network_name = GetNetworkName(service_path);
    error_screen_actor_->ShowCaptivePortalError(network_name);
  } else if (is_gaia_loading_timeout) {
    error_screen_actor_->ShowTimeoutError();
  } else {
    error_screen_actor_->ShowOfflineError();
  }

  bool guest_signin_allowed = IsGuestSigninAllowed() &&
      IsSigninScreenError(error_screen_actor_->state());
  error_screen_actor_->AllowGuestSignin(guest_signin_allowed);

  bool offline_login_allowed = IsOfflineLoginAllowed() &&
      IsSigninScreenError(error_screen_actor_->state()) &&
      error_screen_actor_->state() != ErrorScreenActor::STATE_TIMEOUT_ERROR;
  error_screen_actor_->AllowOfflineLogin(offline_login_allowed);

  if (GetCurrentScreen() != OobeUI::SCREEN_ERROR_MESSAGE) {
    DictionaryValue params;
    params.SetInteger("lastNetworkType", static_cast<int>(connection_type));
    error_screen_actor_->Show(OobeUI::SCREEN_GAIA_SIGNIN, &params);
  }
  EnableLazyDetection();
}

void SigninScreenHandler::HideOfflineMessage(NetworkStateInformer::State state,
                                             const std::string& service_path,
                                             const std::string& reason,
                                             bool is_gaia_signin,
                                             bool is_gaia_reloaded) {
  if (IsSigninScreenHiddenByError()) {
    std::string network_id = GetNetworkUniqueId(service_path);
    LOG(WARNING) << "Hide offline message, "
                 << "state=" << NetworkStateStatusString(state) << ", "
                 << "network_id=" << network_id << ", "
                 << "reason=" << reason;
    error_screen_actor_->Hide();

    // Forces a reload for Gaia screen on hiding error message.
    if (is_gaia_signin && !is_gaia_reloaded)
      ReloadGaiaScreen();
  }
  DisableLazyDetection();
}

void SigninScreenHandler::ReloadGaiaScreen() {
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE) {
    LOG(WARNING) << "Skipping reload of auth extension frame since "
                 << "network state=" << NetworkStateStatusString(state);
    return;
  }
  LOG(WARNING) << "Reload auth extension frame.";
  web_ui()->CallJavascriptFunction("login.GaiaSigninScreen.doReload");
}

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
  if (native_window_delegate_)
    return native_window_delegate_->GetNativeWindow();
  return NULL;
}

void SigninScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("authenticateUser",
      base::Bind(&SigninScreenHandler::HandleAuthenticateUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("completeLogin",
      base::Bind(&SigninScreenHandler::HandleCompleteLogin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("completeAuthentication",
      base::Bind(&SigninScreenHandler::HandleCompleteAuthentication,
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
  web_ui()->RegisterMessageCallback("showLocallyManagedUserCreationScreen",
      base::Bind(
          &SigninScreenHandler::HandleShowLocallyManagedUserCreationScreen,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback("launchPublicAccount",
      base::Bind(&SigninScreenHandler::HandleLaunchPublicAccount,
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
  web_ui()->RegisterMessageCallback("loadWallpaper",
      base::Bind(&SigninScreenHandler::HandleLoadWallpaper,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeUser",
      base::Bind(&SigninScreenHandler::HandleRemoveUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("toggleEnrollmentScreen",
      base::Bind(&SigninScreenHandler::HandleToggleEnrollmentScreen,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("toggleResetScreen",
      base::Bind(&SigninScreenHandler::HandleToggleResetScreen,
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
  web_ui()->RegisterMessageCallback("cancelPasswordChangedFlow",
        base::Bind(&SigninScreenHandler::HandleCancelPasswordChangedFlow,
                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback("migrateUserData",
        base::Bind(&SigninScreenHandler::HandleMigrateUserData,
                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback("resyncUserData",
        base::Bind(&SigninScreenHandler::HandleResyncUserData,
                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginUIStateChanged",
      base::Bind(&SigninScreenHandler::HandleLoginUIStateChanged,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("unlockOnLoginSuccess",
      base::Bind(&SigninScreenHandler::HandleUnlockOnLoginSuccess,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loginScreenUpdate",
      base::Bind(&SigninScreenHandler::HandleLoginScreenUpdate,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showGaiaFrameError",
      base::Bind(&SigninScreenHandler::HandleShowGaiaFrameError,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showLoadingTimeoutError",
      base::Bind(&SigninScreenHandler::HandleShowLoadingTimeoutError,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("updateOfflineLogin",
      base::Bind(&SigninScreenHandler::HandleUpdateOfflineLogin,
                 base::Unretained(this)));
}

void SigninScreenHandler::HandleGetUsers(const base::ListValue* args) {
  SendUserList(false);
}

void SigninScreenHandler::ClearAndEnablePassword() {
  base::FundamentalValue force_online(false);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.resetSigninUI", force_online);
}

void SigninScreenHandler::ClearUserPodPassword() {
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.clearUserPodPassword");
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
  // Make sure that one of the login UI is active now, otherwise
  // preferences update would be picked up next time it will be shown.
  if (!login_ui_active_) {
    LOG(WARNING) << "Login UI is not active - ignoring prefs change.";
    return;
  }

  if (delegate_ && !delegate_->IsShowUsers()) {
    HandleShowAddUser(NULL);
  } else {
    SendUserList(false);
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, NULL);
  }
}

void SigninScreenHandler::ResetSigninScreenHandlerDelegate() {
  SetDelegate(NULL);
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

void SigninScreenHandler::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  switch (error_id) {
    case LoginDisplay::TPM_ERROR:
      web_ui()->CallJavascriptFunction("cr.ui.Oobe.showTpmError");
      break;
    default:
      NOTREACHED() << "Unknown sign in error";
      break;
  }
}

void SigninScreenHandler::ShowSigninUI(const std::string& email) {
  base::StringValue email_value(email);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showSigninUI", email_value);
}

void SigninScreenHandler::ShowGaiaPasswordChanged(const std::string& username) {
  email_ = username;
  password_changed_for_.insert(email_);
  base::StringValue email_value(email_);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showSigninUI", email_value);
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.updateUserGaiaNeeded", email_value);
}

void SigninScreenHandler::ShowPasswordChangedDialog(bool show_password_error) {
  base::FundamentalValue showError(show_password_error);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showPasswordChangedScreen",
                                   showError);
}

void SigninScreenHandler::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  VLOG(2) << "ShowSigninScreenForCreds " << username << " " << password;

  test_user_ = username;
  test_pass_ = password;
  HandleShowAddUser(NULL);
}

void SigninScreenHandler::OnBrowsingDataRemoverDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cookie_remover_ = NULL;
  cookies_cleared_ = true;
  cookie_remover_callback_.Run();
  cookie_remover_callback_.Reset();
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
  switch (type) {
    case chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED: {
      UpdateAuthExtension();
      UpdateAddButtonStatus();
      break;
    }
    case chrome::NOTIFICATION_AUTH_NEEDED: {
      has_pending_auth_ui_ = true;
      break;
    }
    case chrome::NOTIFICATION_AUTH_SUPPLIED:
      has_pending_auth_ui_ = false;
      if (IsSigninScreenHiddenByError()) {
        // Hide error screen and reload auth extension.
        HideOfflineMessage(network_state_informer_->state(),
                           network_state_informer_->last_network_service_path(),
                           ErrorScreenActor::kErrorReasonProxyAuthSupplied,
                           true, false);
      } else if (ui_state_ == UI_STATE_GAIA_SIGNIN) {
        // Reload auth extension as credentials are supplied.
        ReloadGaiaScreen();
      }
      break;
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      // Don't reload auth extension if proxy auth dialog was cancelled.
      has_pending_auth_ui_ = false;
      break;
    }
    default:
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
  if (!dns_cleared_ || !cookies_cleared_ || !delegate_)
    return;

  std::string active_network =
      network_state_informer_->active_network_service_path();
  if (gaia_silent_load_ &&
      (!network_state_informer_->is_online() ||
       gaia_silent_load_network_ != active_network)) {
    // Network has changed. Force Gaia reload.
    gaia_silent_load_ = false;
    // Gaia page will be realoded, so focus isn't stolen anymore.
    focus_stolen_ = false;
  }

  // Note that LoadAuthExtension clears |email_|.
  if (email_.empty())
    delegate_->LoadSigninWallpaper();
  else
    delegate_->LoadWallpaper(email_);

  LoadAuthExtension(!gaia_silent_load_, false, false);
  UpdateUIState(UI_STATE_GAIA_SIGNIN, NULL);

  if (gaia_silent_load_) {
    // The variable is assigned to false because silently loaded Gaia page was
    // used.
    gaia_silent_load_ = false;
    if (focus_stolen_)
      HandleLoginWebuiReady(NULL);
  }

  UpdateState(network_state_informer_->state(),
              network_state_informer_->last_network_service_path(),
              network_state_informer_->last_network_type(),
              ErrorScreenActor::kErrorReasonUpdate);
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
    localized_strings->SetString("stringEmptyEmail",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMPTY_EMAIL));
    localized_strings->SetString("stringEmptyPassword",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMPTY_PASSWORD));
    localized_strings->SetString("stringError",
        l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_ERROR));
    params.Set("localizedStrings", localized_strings);
  }

  params.SetString("gaiaOrigin", GaiaUrls::GetInstance()->gaia_origin_url());
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGaiaUrlPath)) {
    params.SetString("gaiaUrlPath",
        command_line->GetSwitchValueASCII(::switches::kGaiaUrlPath));
  }

  // Test automation data:
  if (command_line->HasSwitch(::switches::kAuthExtensionPath)) {
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
  base::FundamentalValue disabled(AllWhitelistedUsersPresent());
  web_ui()->CallJavascriptFunction(
      "cr.ui.login.DisplayManager.updateAddUserButtonStatus", disabled);
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
  delegate_->CompleteLogin(UserCredentials(typed_email,
                                           password,
                                           std::string()));  // auth_code
}

void SigninScreenHandler::HandleCompleteAuthentication(
    const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string email;
  std::string password;
  std::string auth_code;
  if (!args->GetString(0, &email) ||
      !args->GetString(1, &password) ||
      !args->GetString(2, &auth_code)) {
    NOTREACHED();
    return;
  }

  email = gaia::SanitizeEmail(email);
  delegate_->SetDisplayEmail(email);
  delegate_->CompleteLogin(UserCredentials(email,
                                           password,
                                           auth_code));
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
  delegate_->Login(UserCredentials(username,
                                   password,
                                   std::string()));  // auth_code
}

void SigninScreenHandler::HandleLaunchDemoUser(const base::ListValue* args) {
  if (delegate_)
    delegate_->LoginAsRetailModeUser();
}

void SigninScreenHandler::HandleLaunchIncognito(const base::ListValue* args) {
  if (delegate_)
    delegate_->LoginAsGuest();
}

void SigninScreenHandler::HandleShowLocallyManagedUserCreationScreen(
    const base::ListValue* args) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kEnableManagedUsers))
    return;
  BaseLoginDisplayHost::default_host()->
      StartWizard(WizardController::kLocallyManagedUserCreationScreenName,
      NULL);
}

void SigninScreenHandler::HandleLaunchPublicAccount(
    const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string username;
  if (!args->GetString(0, &username)) {
    NOTREACHED();
    return;
  }
  delegate_->LoginAsPublicAccount(username);
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
  UpdateUIState(UI_STATE_GAIA_SIGNIN, NULL);
}

void SigninScreenHandler::HandleShutdownSystem(const base::ListValue* args) {
#if defined(USE_AURA)
  // Display the shutdown animation before actually requesting shutdown.
  ash::Shell::GetInstance()->session_state_controller()->RequestShutdown();
#else
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
#endif
}

void SigninScreenHandler::HandleLoadWallpaper(const base::ListValue* args) {
  if (!delegate_)
    return;

  std::string email;
  if (!args->GetString(0, &email)) {
    NOTREACHED();
    return;
  }

  delegate_->LoadWallpaper(email);
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

    cookie_remover_callback_ = base::Bind(
        &SigninScreenHandler::ShowSigninScreenIfReady,
        weak_factory_.GetWeakPtr());
    StartClearingCookies();
  }
}

void SigninScreenHandler::HandleToggleEnrollmentScreen(
    const base::ListValue* args) {
  if (delegate_)
    delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleToggleResetScreen(
    const base::ListValue* args) {
  if (delegate_ &&
      !g_browser_process->browser_policy_connector()->IsEnterpriseManaged()) {
    delegate_->ShowResetScreen();
  }
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

void SigninScreenHandler::FillUserDictionary(User* user,
                                             bool is_owner,
                                             DictionaryValue* user_dict) {
  const std::string& email = user->email();
  bool is_public_account =
      user->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT;
  bool is_locally_managed_user =
      user->GetType() == User::USER_TYPE_LOCALLY_MANAGED;
  bool signed_in = user == UserManager::Get()->GetLoggedInUser();

  user_dict->SetString(kKeyUsername, email);
  user_dict->SetString(kKeyEmailAddress, user->display_email());
  user_dict->SetString(kKeyDisplayName, user->GetDisplayName());
  user_dict->SetBoolean(kKeyPublicAccount, is_public_account);
  user_dict->SetBoolean(kKeyLocallyManagedUser, is_locally_managed_user);
  user_dict->SetInteger(kKeyOauthTokenStatus, user->oauth_token_status());
  user_dict->SetBoolean(kKeySignedIn, signed_in);
  user_dict->SetBoolean(kKeyIsOwner, is_owner);

  if (is_public_account) {
    policy::BrowserPolicyConnector* policy_connector =
        g_browser_process->browser_policy_connector();

    if (policy_connector->IsEnterpriseManaged()) {
      user_dict->SetString(kKeyEnterpriseDomain,
                           policy_connector->GetEnterpriseDomain());
    }
  }
}

void SigninScreenHandler::SendUserList(bool animated) {
  if (!delegate_)
    return;

  size_t max_non_owner_users = kMaxUsers - 1;
  size_t non_owner_count = 0;

  ListValue users_list;
  const UserList& users = delegate_->GetUsers();

  bool single_user = users.size() == 1;
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    const std::string& email = (*it)->email();
    std::string owner;
    chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
    bool is_owner = (email == owner);

    if (non_owner_count < max_non_owner_users || is_owner) {
      DictionaryValue* user_dict = new DictionaryValue();
      FillUserDictionary(*it, is_owner, user_dict);
      bool is_public_account =
          ((*it)->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT);
      bool signed_in = *it == UserManager::Get()->GetLoggedInUser();
      // Single user check here is necessary because owner info might not be
      // available when running into login screen on first boot.
      // See http://crosbug.com/12723
      user_dict->SetBoolean(kKeyCanRemove,
                            !single_user &&
                            !email.empty() &&
                            !is_owner &&
                            !is_public_account &&
                            !signed_in);

      users_list.Append(user_dict);
      if (!is_owner)
        ++non_owner_count;
    }
  }

  base::FundamentalValue animated_value(animated);
  base::FundamentalValue guest_value(delegate_->IsShowGuest());
  web_ui()->CallJavascriptFunction("login.AccountPickerScreen.loadUsers",
                                   users_list, animated_value, guest_value);
}

void SigninScreenHandler::HandleAccountPickerReady(
    const base::ListValue* args) {
  LOG(INFO) << "Login WebUI >> AccountPickerReady";

  if (delegate_ && !ScreenLocker::default_screen_locker() &&
      !chromeos::IsMachineHWIDCorrect() &&
      !oobe_ui_) {
    delegate_->ShowWrongHWIDScreen();
    return;
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    prefs->SetBoolean(prefs::kFactoryResetRequested, false);
    prefs->CommitPendingWrite();
    base::ListValue args;
    HandleToggleResetScreen(&args);
    return;
  }

  is_account_picker_showing_first_time_ = true;
  MaybePreloadAuthExtension();

  if (ScreenLocker::default_screen_locker()) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOCK_WEBUI_READY,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }

  if (delegate_)
    delegate_->OnSigninScreenReady();
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
  std::string source;
  if (!args->GetString(0, &source)) {
    NOTREACHED();
    return;
  }

  LOG(INFO) << "Login WebUI >> LoginVisible, source: " << source << ", "
            << "webui_visible_: " << webui_visible_;
  if (!webui_visible_) {
    // There might be multiple messages from OOBE UI so send notifications after
    // the first one only.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }
  webui_visible_ = true;
}

void SigninScreenHandler::HandleCancelPasswordChangedFlow(
    const base::ListValue* args) {
  cookie_remover_callback_ = base::Bind(
      &SigninScreenHandler::CancelPasswordChangedFlowInternal,
      weak_factory_.GetWeakPtr());
  StartClearingCookies();
}

void SigninScreenHandler::HandleMigrateUserData(const base::ListValue* args) {
  std::string old_password;
  if (!args->GetString(0, &old_password)) {
    NOTREACHED();
    return;
  }
  if (delegate_)
    delegate_->MigrateUserData(old_password);
}

void SigninScreenHandler::HandleResyncUserData(const base::ListValue* args) {
  if (delegate_)
    delegate_->ResyncUserData();
}

void SigninScreenHandler::HandleLoginUIStateChanged(
    const base::ListValue* args) {
  std::string source;
  bool new_value;
  if (!args->GetString(0, &source) || !args->GetBoolean(1, &new_value)) {
    NOTREACHED();
    return;
  }
  if (source == kSourceGaiaSignin) {
    ui_state_ = UI_STATE_GAIA_SIGNIN;
  } else if (source == kSourceAccountPicker) {
    ui_state_ = UI_STATE_ACCOUNT_PICKER;
  } else {
    NOTREACHED();
    return;
  }

  LOG(INFO) << "Login WebUI >> active: " << new_value << ", "
            << "source: " << source;
  login_ui_active_ = new_value;
}

void SigninScreenHandler::HandleUnlockOnLoginSuccess(
    const base::ListValue* args) {
  DCHECK(UserManager::Get()->IsUserLoggedIn());
  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->UnlockOnLoginSuccess();
}

void SigninScreenHandler::HandleLoginScreenUpdate(
    const base::ListValue* args) {
  LOG(INFO) << "Auth extension frame is loaded";
  UpdateStateInternal(network_state_informer_->state(),
                      network_state_informer_->last_network_service_path(),
                      network_state_informer_->last_network_type(),
                      ErrorScreenActor::kErrorReasonUpdate,
                      false);
}

void SigninScreenHandler::HandleShowGaiaFrameError(
    const base::ListValue* args) {
  int error;
  if (args->GetSize() != 1 || !args->GetInteger(0, &error)) {
    NOTREACHED();
    return;
  }
  if (network_state_informer_->state() != NetworkStateInformer::ONLINE)
    return;
  LOG(WARNING) << "Gaia frame error: "  << error;
  std::string reason = base::StringPrintf("frame error:%d", error);
  UpdateStateInternal(network_state_informer_->state(),
                      network_state_informer_->last_network_service_path(),
                      network_state_informer_->last_network_type(),
                      reason,
                      false);
}

void SigninScreenHandler::HandleShowLoadingTimeoutError(
    const base::ListValue* args) {
  UpdateStateInternal(network_state_informer_->state(),
                      network_state_informer_->last_network_service_path(),
                      network_state_informer_->last_network_type(),
                      ErrorScreenActor::kErrorReasonLoadingTimeout,
                      false);
}

void SigninScreenHandler::HandleUpdateOfflineLogin(
    const base::ListValue* args) {
  DCHECK(args && args->GetSize() == 1);

  bool offline_login_active = false;
  if (!args->GetBoolean(0, &offline_login_active)) {
    NOTREACHED();
    return;
  }
  offline_login_active_ = offline_login_active;
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
    gaia_silent_load_network_ =
        network_state_informer_->active_network_service_path();
    LoadAuthExtension(true, true, false);
  }
}

bool SigninScreenHandler::AllWhitelistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  UserManager* user_manager = UserManager::Get();
  const UserList& users = user_manager->GetUsers();
  if (!delegate_ || users.size() > kMaxUsers) {
    return false;
  }
  const base::ListValue* whitelist = NULL;
  if (!cros_settings->GetList(kAccountsPrefUsers, &whitelist) || !whitelist)
    return false;
  for (size_t i = 0; i < whitelist->GetSize(); ++i) {
    std::string whitelisted_user;
    // NB: Wildcards in the whitelist are also detected as not present here.
    if (!whitelist->GetString(i, &whitelisted_user) ||
        !user_manager->IsKnownUser(whitelisted_user)) {
      return false;
    }
  }
  return true;
}

void SigninScreenHandler::CancelPasswordChangedFlowInternal() {
  if (delegate_) {
    Show(oobe_ui_);
    delegate_->CancelPasswordChangedFlow();
  }
}

OobeUI::Screen SigninScreenHandler::GetCurrentScreen() const {
  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (oobe_ui)
    screen = oobe_ui->current_screen();
  return screen;
}

bool SigninScreenHandler::IsGaiaLogin() const {
  return IsSigninScreen(GetCurrentScreen()) &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsSigninScreenHiddenByError() const {
  return (GetCurrentScreen() == OobeUI::SCREEN_ERROR_MESSAGE) &&
      (IsSigninScreen(error_screen_actor_->parent_screen()));
}

bool SigninScreenHandler::IsGuestSigninAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;
  bool allow_guest;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  return allow_guest;
}

bool SigninScreenHandler::IsOfflineLoginAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;

  // Offline login is allowed only when user pods are hidden.
  bool show_pods;
  cros_settings->GetBoolean(kAccountsPrefShowUserNamesOnSignIn, &show_pods);
  return !show_pods;
}

}  // namespace chromeos
