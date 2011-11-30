// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/dnsrr_resolver.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

const char kDefaultDomain[] = "@gmail.com";

// Account picker screen id.
const char kAccountPickerScreen[] = "account-picker";
// Sign in screen id for GAIA extension hosted content.
const char kGaiaSigninScreen[] = "gaia-signin";
// Start page of GAIA authentication extension.
const char kGaiaExtStartPage[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html";

// User dictionary keys.
const char kKeyName[] = "name";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeySignedIn[] = "signedIn";
const char kKeyCanRemove[] = "canRemove";
const char kKeyOauthTokenStatus[] = "oauthTokenStatus";

// Max number of users to show.
const int kMaxUsers = 5;

const char kReasonNetworkChanged[] = "network changed";
const char kReasonProxyChanged[] = "proxy changed";

// Sanitize emails. Currently, it only ensures all emails have a domain.
std::string SanitizeEmail(const std::string& email) {
  std::string sanitized(email);

  // Apply a default domain if necessary.
  if (sanitized.find('@') == std::string::npos)
    sanitized += kDefaultDomain;

  return sanitized;
}

// The Task posted to PostTaskAndReply in StartClearingDnsCache on the IO
// thread.
void ClearDnsCache(IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (browser_shutdown::IsTryingToQuit())
    return;

  io_thread->globals()->dnsrr_resolver.get()->OnIPAddressChanged();
}

}  // namespace

namespace chromeos {

// Class which observes network state changes and calls registered callbacks.
// State is considered changed if connection or the active network has been
// changed. Also, it answers to the requests about current network state.
class NetworkStateInformer
    : public chromeos::NetworkLibrary::NetworkManagerObserver,
      public content::NotificationObserver {
 public:
  explicit NetworkStateInformer(WebUI* web_ui);
  virtual ~NetworkStateInformer();

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
 private:
  enum State {OFFLINE, ONLINE, CAPTIVE_PORTAL};

  bool UpdateState(chromeos::NetworkLibrary* cros);

  void SendStateToObservers(const std::string& reason);

  content::NotificationRegistrar registrar_;
  base::hash_set<std::string> observers_;
  std::string active_network_;
  ConnectionType last_network_type_;
  std::string network_name_;
  State state_;
  WebUI* web_ui_;
};

// NetworkStateInformer implementation -----------------------------------------

NetworkStateInformer::NetworkStateInformer(WebUI* web_ui)
    : last_network_type_(TYPE_WIFI),
      state_(OFFLINE),
      web_ui_(web_ui) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  UpdateState(cros);
  cros->AddNetworkManagerObserver(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_PROXY_CHANGED,
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
  if (UpdateState(cros)) {
    SendStateToObservers(kReasonNetworkChanged);
  }
}

void NetworkStateInformer::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED);
  SendStateToObservers(kReasonProxyChanged);
}

bool NetworkStateInformer::UpdateState(NetworkLibrary* cros) {
  if (cros->active_network())
    last_network_type_ = cros->active_network()->type();

  State new_state;
  std::string new_active_network;
  if (!cros->Connected()) {
    new_state = OFFLINE;
    network_name_.clear();
  } else {
    const Network* active_network = cros->active_network();
    if (active_network) {
      new_active_network = active_network->unique_id();
      network_name_ = active_network->name();
      if (active_network->restricted_pool()) {
        new_state = CAPTIVE_PORTAL;
      } else {
        new_state = ONLINE;
      }
    } else {
      // Bogus network situation:
      // Connected() returns true but no active network.
      new_state = OFFLINE;
      NOTREACHED();
    }
  }
  bool updated = (new_state != state_) ||
      (active_network_ != new_active_network);
  state_ = new_state;
  active_network_ = new_active_network;
  return updated;
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
      is_first_webui_ready_(false),
      is_first_attempt_(true),
      dns_cleared_(false),
      dns_clear_task_running_(false),
      cookies_cleared_(false),
      cookie_remover_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      key_event_listener_(NULL) {
}

SigninScreenHandler::~SigninScreenHandler() {
  weak_factory_.InvalidateWeakPtrs();
  if (cookie_remover_)
    cookie_remover_->RemoveObserver(this);
  if (key_event_listener_)
    key_event_listener_->RemoveCapsLockObserver(this);
}

void SigninScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("signinScreenTitle",
      l10n_util::GetStringUTF16(IDS_SIGNIN_SCREEN_TITLE));
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
  localized_strings->SetString("captivePortalNetworkSelect",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT));
  localized_strings->SetString("proxyMessageText",
      l10n_util::GetStringUTF16(IDS_LOGIN_PROXY_ERROR_MESSAGE));
  localized_strings->SetString("createAccount",
      l10n_util::GetStringUTF16(IDS_CREATE_ACCOUNT_HTML));
  localized_strings->SetString("guestSignin",
      l10n_util::GetStringUTF16(IDS_BROWSE_WITHOUT_SIGNING_IN_HTML));
  localized_strings->SetString("removeUser",
      l10n_util::GetStringUTF16(IDS_LOGIN_REMOVE));

  localized_strings->SetString("authType", "ext");
}

void SigninScreenHandler::Show(bool oobe_ui) {
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

    ShowScreen(kAccountPickerScreen, NULL);
  }
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  DCHECK(delegate_);
  delegate_->SetWebUIHandler(this);
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::Initialize() {
  // Register for Caps Lock state change notifications;
  key_event_listener_ = SystemKeyEventListener::GetInstance();
  if (key_event_listener_)
    key_event_listener_->AddCapsLockObserver(this);

  if (show_on_init_) {
    show_on_init_ = false;
    Show(oobe_ui_);
  }
}

void SigninScreenHandler::RegisterMessages() {
  network_state_informer_.reset(new NetworkStateInformer(web_ui_));

  web_ui_->RegisterMessageCallback("authenticateUser",
      base::Bind(&SigninScreenHandler::HandleAuthenticateUser,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("completeLogin",
      base::Bind(&SigninScreenHandler::HandleCompleteLogin,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getUsers",
      base::Bind(&SigninScreenHandler::HandleGetUsers,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("launchIncognito",
      base::Bind(&SigninScreenHandler::HandleLaunchIncognito,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("fixCaptivePortal",
      base::Bind(&SigninScreenHandler::HandleFixCaptivePortal,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("showAddUser",
      base::Bind(&SigninScreenHandler::HandleShowAddUser,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("shutdownSystem",
      base::Bind(&SigninScreenHandler::HandleShutdownSystem,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("removeUser",
      base::Bind(&SigninScreenHandler::HandleRemoveUser,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("toggleEnrollmentScreen",
      base::Bind(&SigninScreenHandler::HandleToggleEnrollmentScreen,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("launchHelpApp",
      base::Bind(&SigninScreenHandler::HandleLaunchHelpApp,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("createAccount",
      base::Bind(&SigninScreenHandler::HandleCreateAccount,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("accountPickerReady",
      base::Bind(&SigninScreenHandler::HandleAccountPickerReady,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("loginWebuiReady",
      base::Bind(&SigninScreenHandler::HandleLoginWebuiReady,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("loginRequestNetworkState",
      base::Bind(&SigninScreenHandler::HandleLoginRequestNetworkState,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("loginAddNetworkStateObserver",
      base::Bind(&SigninScreenHandler::HandleLoginAddNetworkStateObserver,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("loginRemoveNetworkStateObserver",
      base::Bind(&SigninScreenHandler::HandleLoginRemoveNetworkStateObserver,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("signOutUser",
      base::Bind(&SigninScreenHandler::HandleSignOutUser,
                 base::Unretained(this)));
}

void SigninScreenHandler::HandleGetUsers(const base::ListValue* args) {
  SendUserList(false);
}

void SigninScreenHandler::ClearAndEnablePassword() {
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.resetSigninUI");
}

void SigninScreenHandler::OnLoginSuccess(const std::string& username) {
  base::StringValue username_value(username);
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.onLoginSuccess", username_value);
}

void SigninScreenHandler::OnUserRemoved(const std::string& username) {
  SendUserList(false);
}

void SigninScreenHandler::OnUserImageChanged(const User& user) {
  base::StringValue user_email(user.email());
  web_ui_->CallJavascriptFunction(
      "login.AccountPickerScreen.updateUserImage", user_email);
}

void SigninScreenHandler::ShowError(int login_attempts,
                                    const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  base::FundamentalValue login_attempts_value(login_attempts);
  base::StringValue error_message(error_text);
  base::StringValue help_link(help_link_text);
  base::FundamentalValue help_id(static_cast<int>(help_topic_id));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.showSignInError",
                                  login_attempts_value,
                                  error_message,
                                  help_link,
                                  help_id);
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
    web_ui_->CallJavascriptFunction(
        "login.AccountPickerScreen.setCapsLockState", capsLockState);
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

  LoadAuthExtension(!is_first_attempt_, false);
  ShowScreen(kGaiaSigninScreen, NULL);

  if (is_first_attempt_) {
    is_first_attempt_ = false;
    if (is_first_webui_ready_)
      HandleLoginWebuiReady(NULL);
  }
}

void SigninScreenHandler::LoadAuthExtension(bool force, bool silent_load) {
  DictionaryValue params;

  params.SetBoolean("forceReload", force);
  params.SetBoolean("silentLoad", silent_load);
  params.SetString("startUrl", kGaiaExtStartPage);
  params.SetString("email", email_);
  email_.clear();

  // TODO(pastarmovj): Watch for changes of this variables to update the UI
  // properly when the policy has been fetched on sign-on screen.
  bool allow_new_user = true;
  CrosSettings::Get()->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  params.SetBoolean("createAccount", allow_new_user);
  bool allow_guest = true;
  CrosSettings::Get()->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  params.SetBoolean("guestSignin", allow_guest);

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  if (!app_locale.empty())
    params.SetString("hl", app_locale);

  params.SetString("gaiaOrigin", GaiaUrls::GetInstance()->gaia_origin_url());

  // Test automation data:
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
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
  web_ui_->CallJavascriptFunction("login.GaiaSigninScreen.loadAuthExtension",
                                  params);
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
  std::string username;
  std::string password;
  if (!args->GetString(0, &username) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  username = SanitizeEmail(username);
  delegate_->CompleteLogin(username, password);
}

void SigninScreenHandler::HandleAuthenticateUser(const base::ListValue* args) {
  std::string username;
  std::string password;
  if (!args->GetString(0, &username) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  username = SanitizeEmail(username);
  delegate_->Login(username, password);
}

void SigninScreenHandler::HandleLaunchIncognito(const base::ListValue* args) {
  delegate_->LoginAsGuest();
}

void SigninScreenHandler::HandleFixCaptivePortal(const base::ListValue* args) {
  delegate_->FixCaptivePortal();
}

void SigninScreenHandler::HandleShutdownSystem(const base::ListValue* args) {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

void SigninScreenHandler::HandleRemoveUser(const base::ListValue* args) {
  std::string email;
  if (!args->GetString(0, &email)) {
    NOTREACHED();
    return;
  }

  delegate_->RemoveUser(email);
}

void SigninScreenHandler::HandleShowAddUser(const base::ListValue* args) {
  email_.clear();
  // |args| can be null if it's OOBE.
  if (args)
    args->GetString(0, &email_);
  if (is_first_attempt_ && email_.empty()) {
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
  delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleLaunchHelpApp(const base::ListValue* args) {
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
    bool signed_in = UserManager::Get()->user_is_logged_in() &&
        email == UserManager::Get()->logged_in_user().email();

    if (non_owner_count < max_non_owner_users || is_owner) {
      DictionaryValue* user_dict = new DictionaryValue();
      user_dict->SetString(kKeyName, (*it)->GetDisplayName());
      user_dict->SetString(kKeyEmailAddress, email);
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
    guest_dict->SetString(kKeyName, l10n_util::GetStringUTF16(IDS_GUEST));
    guest_dict->SetString(kKeyEmailAddress, "");
    guest_dict->SetBoolean(kKeyCanRemove, false);
    guest_dict->SetInteger(kKeyOauthTokenStatus,
                           User::OAUTH_TOKEN_STATUS_UNKNOWN);
    users_list.Append(guest_dict);
  }

  // Call the Javascript callback
  base::FundamentalValue animated_value(animated);
  web_ui_->CallJavascriptFunction("login.AccountPickerScreen.loadUsers",
                                  users_list, animated_value);
}

void SigninScreenHandler::HandleAccountPickerReady(
    const base::ListValue* args) {
  // Fetching of the extension is not started before account picker page is
  // loaded because it can affect the loading speed.
  if (is_first_attempt_ && !cookie_remover_ && !dns_clear_task_running_)
    LoadAuthExtension(true, true);
}

void SigninScreenHandler::HandleLoginWebuiReady(const base::ListValue* args) {
  if (!is_first_attempt_) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_WEBUI_READY,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  } else {
    is_first_webui_ready_ = true;
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

void SigninScreenHandler::HandleSignOutUser(const base::ListValue* args) {
  // TODO(flackr): Deliver this message to the delegate (crbug.com/105267).
  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->Signout();
}

void SigninScreenHandler::HandleCreateAccount(const base::ListValue* args) {
  delegate_->CreateAccount();
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

  cookie_remover_ = new BrowsingDataRemover(
      Profile::FromBrowserContext(web_ui_->tab_contents()->browser_context()),
      BrowsingDataRemover::EVERYTHING,
      base::Time());
  cookie_remover_->AddObserver(this);
  cookie_remover_->Remove(BrowsingDataRemover::REMOVE_SITE_DATA);
}

}  // namespace chromeos
