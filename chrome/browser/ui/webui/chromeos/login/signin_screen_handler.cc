// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/dnsrr_resolver.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kDefaultDomain[] = "@gmail.com";

// Account picker screen id.
const char kAccountPickerScreen[] = "account-picker";
// Sign in screen id.
const char kSigninScreen[] = "signin";
// Sign in screen id for GAIA extension hosted content.
const char kGaiaSigninScreen[] = "gaia-signin";
// Start page of GAIA authentication extension.
const char kGaiaExtStartPage[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html";

// User dictionary keys.
const char kKeyName[] = "name";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyCanRemove[] = "canRemove";
const char kKeyImageUrl[] = "imageUrl";
const char kKeyOauthTokenStatus[] = "oauthTokenStatus";

// Max number of users to show.
const int kMaxUsers = 5;

// Sanitize emails. Currently, it only ensures all emails have a domain.
std::string SanitizeEmail(const std::string& email) {
  std::string sanitized(email);

  // Apply a default domain if necessary.
  if (sanitized.find('@') == std::string::npos)
    sanitized += kDefaultDomain;

  return sanitized;
}

}  // namespace

namespace chromeos {

// Class which observes network state changes and calls registerd callbacks.
// State is considered changed if connection or the active network has been
// changed. Also, it answers to the requests about current network state.
class NetworkStateInformer
    : public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  explicit NetworkStateInformer(WebUI* web_ui);
  virtual ~NetworkStateInformer();

  // Adds observer's callback to be called when network state has been changed.
  void AddObserver(const std::string& callback);

  // Removes observer's callback.
  void RemoveObserver(const std::string& callback);

  // Sends current network state and network name using the callback.
  void SendState(const std::string& callback);

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* cros) OVERRIDE;

 private:
  enum State {OFFLINE, ONLINE, CAPTIVE_PORTAL};

  bool UpdateState(chromeos::NetworkLibrary* cros);

  base::hash_set<std::string> observers_;
  std::string active_network_;
  std::string network_name_;
  State state_;
  WebUI* web_ui_;
};

// Clears DNS cache on IO thread.
class ClearDnsCacheTaskOnIOThread : public Task {
 public:
  ClearDnsCacheTaskOnIOThread(Task* callback, IOThread* io_thread)
      : callback_(callback), io_thread_(io_thread) {
  }
  virtual ~ClearDnsCacheTaskOnIOThread() {}

  // Task overrides.
  virtual void Run() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (browser_shutdown::IsTryingToQuit())
     return;

    io_thread_->globals()->dnsrr_resolver.get()->OnIPAddressChanged();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback_);
  }

 private:
  Task* callback_;
  IOThread* io_thread_;
  DISALLOW_COPY_AND_ASSIGN(ClearDnsCacheTaskOnIOThread);
};

// NetworkStateInformer implementation -----------------------------------------

NetworkStateInformer::NetworkStateInformer(WebUI* web_ui) : web_ui_(web_ui) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  UpdateState(cros);
  cros->AddNetworkManagerObserver(this);
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

void NetworkStateInformer::SendState(const std::string& callback) {
  base::FundamentalValue state_value(state_);
  base::StringValue network_value(network_name_);
  web_ui_->CallJavascriptFunction(callback, state_value, network_value);
}

void NetworkStateInformer::OnNetworkManagerChanged(NetworkLibrary* cros) {
  if (UpdateState(cros)) {
    for (base::hash_set<std::string>::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      SendState(*it);
    }
  }
}

bool NetworkStateInformer::UpdateState(NetworkLibrary* cros) {
  State new_state;
  std::string new_active_network;
  if (!cros->Connected()) {
    new_state = OFFLINE;
    network_name_.clear();
  } else {
    const Network* active_network = cros->active_network();
    new_active_network = active_network->unique_id();
    network_name_ = active_network->name();
    if (active_network && active_network->restricted_pool()) {
      new_state = CAPTIVE_PORTAL;
    } else {
      new_state = ONLINE;
    }
  }
  bool updated = (new_state != state_) ||
      (active_network_ != new_active_network);
  state_ = new_state;
  active_network_ = new_active_network;
  return updated;
}

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler()
    : delegate_(WebUILoginDisplay::GetInstance()),
      show_on_init_(false),
      oobe_ui_(false),
      dns_cleared_(false),
      dns_clear_task_running_(false),
      cookies_cleared_(false),
      extension_driven_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kWebUILogin)),
      cookie_remover_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  delegate_->SetWebUIHandler(this);
}

SigninScreenHandler::~SigninScreenHandler() {
  method_factory_.RevokeAll();
  if (cookie_remover_)
    cookie_remover_->RemoveObserver(this);
}

void SigninScreenHandler::GetLocalizedStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("signinScreenTitle",
      l10n_util::GetStringUTF16(IDS_SIGNIN_SCREEN_TITLE));
  localized_strings->SetString("passwordHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  localized_strings->SetString("signinButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_BUTTON));
  localized_strings->SetString("enterGuestButton",
      l10n_util::GetStringUTF16(IDS_ENTER_GUEST_SESSION_BUTTON));
  localized_strings->SetString("shutDown",
      l10n_util::GetStringUTF16(IDS_SHUTDOWN_BUTTON));
  localized_strings->SetString("addUser",
      l10n_util::GetStringUTF16(IDS_ADD_USER_BUTTON));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("addUserOfflineMessage",
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
  localized_strings->SetString("createAccount",
      l10n_util::GetStringUTF16(IDS_CREATE_ACCOUNT_HTML));
  localized_strings->SetString("guestSignin",
      l10n_util::GetStringUTF16(IDS_BROWSE_WITHOUT_SIGNING_IN_HTML));
  localized_strings->SetString("removeUser",
      l10n_util::GetStringUTF16(IDS_LOGIN_REMOVE));

  if (extension_driven_)
    localized_strings->SetString("authType", "ext");
  else
    localized_strings->SetString("authType", "webui");
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

    ShowScreen(kAccountPickerScreen, NULL);
  }
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::Initialize() {
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

void SigninScreenHandler::OnDnsCleared() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  dns_clear_task_running_ = false;
  dns_cleared_ = true;
  ShowSigninScreenIfReady();
}

// Show sign in screen as soon as we clear dns cache the cookie jar.
void SigninScreenHandler::ShowSigninScreenIfReady() {
  if (!dns_cleared_ || !cookies_cleared_)
    return;

  DictionaryValue params;
  params.SetString("startUrl", kGaiaExtStartPage);
  params.SetString("email", email_);
  email_.clear();

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  if (!app_locale.empty())
    params.SetString("hl", app_locale);

  params.SetBoolean("createAccount",
      UserCrosSettingsProvider::cached_allow_new_user());
  params.SetBoolean("guestSignin",
      UserCrosSettingsProvider::cached_allow_guest());
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
  ShowScreen(kGaiaSigninScreen, &params);
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
  DCHECK(CrosLibrary::Get()->EnsureLoaded());
  CrosLibrary::Get()->GetPowerLibrary()->RequestShutdown();
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
  if (extension_driven_) {
    email_.clear();
    // |args| can be null if it's OOBE.
    if (args)
      args->GetString(0, &email_);
    StartClearingDnsCache();
    StartClearingCookies();
  } else {
    ShowScreen(kSigninScreen, NULL);
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
    help_app_ = new HelpAppLauncher(
        WebUILoginDisplay::GetLoginWindow()->GetNativeWindow());
  help_app_->ShowHelpTopic(
      static_cast<HelpAppLauncher::HelpTopic>(help_topic_id));
}

void SigninScreenHandler::SendUserList(bool animated) {
  bool show_guest = WebUILoginDisplay::GetInstance()->show_guest();

  size_t max_non_owner_users = show_guest ? kMaxUsers - 2 : kMaxUsers - 1;
  size_t non_owner_count = 0;

  ListValue users_list;
  UserVector users = WebUILoginDisplay::GetInstance()->users();

  bool single_user = users.size() == 1;
  for (UserVector::const_iterator it = users.begin();
       it != users.end(); ++it) {
    const std::string& email = it->email();
    bool is_owner = email == UserCrosSettingsProvider::cached_owner();

    if (non_owner_count < max_non_owner_users || is_owner) {
      DictionaryValue* user_dict = new DictionaryValue();
      user_dict->SetString(kKeyName, it->GetDisplayName());
      user_dict->SetString(kKeyEmailAddress, email);
      user_dict->SetInteger(kKeyOauthTokenStatus, it->oauth_token_status());

      // Single user check here is necessary because owner info might not be
      // available when running into login screen on first boot.
      // See http://crosbug.com/12723
      user_dict->SetBoolean(kKeyCanRemove,
                            !single_user &&
                            !email.empty() &&
                            !is_owner);

      if (!email.empty()) {
        long long timestamp = base::TimeTicks::Now().ToInternalValue();
        std::string image_url(
            StringPrintf("%s%s?id=%lld",
                         chrome::kChromeUIUserImageURL,
                         email.c_str(),
                         timestamp));
        user_dict->SetString(kKeyImageUrl, image_url);
      } else {
        std::string image_url(std::string(chrome::kChromeUIScheme) + "://" +
            std::string(chrome::kChromeUIThemePath) +
            "/IDR_LOGIN_DEFAULT_USER");
        user_dict->SetString(kKeyImageUrl, image_url);
      }

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
                           UserManager::OAUTH_TOKEN_STATUS_UNKNOWN);
    std::string image_url(std::string(chrome::kChromeUIScheme) + "://" +
        std::string(chrome::kChromeUIThemePath) + "/IDR_LOGIN_GUEST");
    guest_dict->SetString(kKeyImageUrl, image_url);
    users_list.Append(guest_dict);
  }

  // Call the Javascript callback
  base::FundamentalValue animated_value(animated);
  web_ui_->CallJavascriptFunction("login.AccountPickerScreen.loadUsers",
                                  users_list, animated_value);
}

void SigninScreenHandler::HandleLoginWebuiReady(const base::ListValue* args) {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_WEBUI_READY,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

void SigninScreenHandler::HandleLoginRequestNetworkState(
    const base::ListValue* args) {
  std::string callback;
  if (!args->GetString(0, &callback)) {
    NOTREACHED();
    return;
  }
  network_state_informer_->SendState(callback);
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

void SigninScreenHandler::HandleCreateAccount(const base::ListValue* args) {
  delegate_->CreateAccount();
}

void SigninScreenHandler::StartClearingDnsCache() {
  if (dns_clear_task_running_ || !g_browser_process->io_thread())
    return;

  dns_cleared_ = false;
  ClearDnsCacheTaskOnIOThread* clear_dns_task = new ClearDnsCacheTaskOnIOThread(
      method_factory_.NewRunnableMethod(&SigninScreenHandler::OnDnsCleared),
      g_browser_process->io_thread());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, clear_dns_task);
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
