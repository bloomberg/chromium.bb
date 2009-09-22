// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/auth_watcher.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/authenticator.h"
#include "chrome/browser/sync/engine/net/gaia_authenticator.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/pthread_helpers.h"
#include "chrome/browser/sync/util/user_settings.h"

// How authentication happens:
//
// Kick Off:
//     The sync API looks to see if the user's name and
//     password are stored.  If so, it calls authwatcher.Authenticate() with
//     them.  Otherwise it fires an error event.
//
// On failed Gaia Auth:
//     The AuthWatcher attempts to use saved hashes to authenticate
//     locally, and on success opens the share.
//     On failure, fires an error event.
//
// On successful Gaia Auth:
//     AuthWatcher launches a thread to open the share and to get the
//     authentication token from the sync server.

using std::pair;
using std::string;
using std::vector;

namespace browser_sync {

AuthWatcher::AuthWatcher(DirectoryManager* dirman,
                         ServerConnectionManager* scm,
                         AllStatus* allstatus,
                         const string& user_agent,
                         const string& service_id,
                         const string& gaia_url,
                         UserSettings* user_settings,
                         GaiaAuthenticator* gaia_auth,
                         TalkMediator* talk_mediator)
    : gaia_(gaia_auth),
      dirman_(dirman),
      scm_(scm),
      allstatus_(allstatus),
      status_(NOT_AUTHENTICATED),
      user_settings_(user_settings),
      talk_mediator_(talk_mediator),
      thread_handle_valid_(false),
      authenticating_now_(false),
      current_attempt_trigger_(AuthWatcherEvent::USER_INITIATED) {
  connmgr_hookup_.reset(
      NewEventListenerHookup(scm->channel(), this,
                             &AuthWatcher::HandleServerConnectionEvent));
  AuthWatcherEvent done = { AuthWatcherEvent::AUTHWATCHER_DESTROYED };
  channel_.reset(new Channel(done));
}

void* AuthWatcher::AuthenticationThreadStartRoutine(void* arg) {
  ThreadParams* args = reinterpret_cast<ThreadParams*>(arg);
  return args->self->AuthenticationThreadMain(args);
}

bool AuthWatcher::ProcessGaiaAuthSuccess() {
  GaiaAuthenticator::AuthResults results = gaia_->results();

  // We just successfully signed in again, let's clear out any residual cached
  // login data from earlier sessions.
  ClearAuthenticationData();

  user_settings_->StoreEmailForSignin(results.email, results.primary_email);
  user_settings_->RememberSigninType(results.email, results.signin);
  user_settings_->RememberSigninType(results.primary_email, results.signin);
  results.email = results.primary_email;
  gaia_->SetUsernamePassword(results.primary_email, results.password);
  if (!user_settings_->VerifyAgainstStoredHash(results.email, results.password))
    user_settings_->StoreHashedPassword(results.email, results.password);

  if (PERSIST_TO_DISK == results.credentials_saved) {
    user_settings_->SetAuthTokenForService(results.email,
                                           SYNC_SERVICE_NAME,
                                           gaia_->auth_token());
  }

  return AuthenticateWithToken(results.email, gaia_->auth_token());
}

bool AuthWatcher::GetAuthTokenForService(const string& service_name,
                                         string* service_token) {
  string user_name;

  // We special case this one by trying to return it from memory first. We
  // do this because the user may not have checked "Remember me" and so we
  // may not have persisted the sync service token beyond the initial
  // login.
  if (SYNC_SERVICE_NAME == service_name && !sync_service_token_.empty()) {
    *service_token = sync_service_token_;
    return true;
  }

  if (user_settings_->GetLastUserAndServiceToken(service_name, &user_name,
                                                 service_token)) {
    // The casing gets preserved in some places and not in others it seems,
    // at least I have observed different casings persisted to different DB
    // tables.
    if (!base::strcasecmp(user_name.c_str(),
                          user_settings_->email().c_str())) {
      return true;
    } else {
      LOG(ERROR) << "ERROR: We seem to have saved credentials for someone "
                 << " other than the current user.";
      return false;
    }
  }

  return false;
}

const char kAuthWatcher[] = "AuthWatcher";

bool AuthWatcher::AuthenticateWithToken(const string& gaia_email,
                                        const string& auth_token) {
  // Store a copy of the sync service token in memory.
  sync_service_token_ = auth_token;
  scm_->set_auth_token(sync_service_token_);

  Authenticator auth(scm_, user_settings_);
  Authenticator::AuthenticationResult result =
      auth.AuthenticateToken(auth_token);
  string email = gaia_email;
  if (auth.display_email() && *auth.display_email()) {
    email = auth.display_email();
    LOG(INFO) << "Auth returned email " << email << " for gaia email " <<
        gaia_email;
  }
  AuthWatcherEvent event = {AuthWatcherEvent::ILLEGAL_VALUE , 0};
  gaia_->SetUsername(email);
  gaia_->SetAuthToken(auth_token, SAVE_IN_MEMORY_ONLY);
  const bool was_authenticated = NOT_AUTHENTICATED != status_;
  switch (result) {
    case Authenticator::SUCCESS:
      {
        status_ = GAIA_AUTHENTICATED;
        PathString share_name;
        CHECK(AppendUTF8ToPathString(email.data(), email.size(), &share_name));
        user_settings_->SwitchUser(email);

        // Set the authentication token for notifications
        talk_mediator_->SetAuthToken(email, auth_token);

        if (!was_authenticated)
          LoadDirectoryListAndOpen(share_name);
        NotifyAuthSucceeded(email);
        return true;
      }
  case Authenticator::BAD_AUTH_TOKEN:
    event.what_happened = AuthWatcherEvent::SERVICE_AUTH_FAILED;
    break;
  case Authenticator::CORRUPT_SERVER_RESPONSE:
  case Authenticator::SERVICE_DOWN:
    event.what_happened = AuthWatcherEvent::SERVICE_CONNECTION_FAILED;
    break;
  case Authenticator::USER_NOT_ACTIVATED:
    event.what_happened = AuthWatcherEvent::SERVICE_USER_NOT_SIGNED_UP;
    break;
  default:
    LOG(FATAL) << "Illegal return from AuthenticateToken";
    return true;  // keep the compiler happy
  }
  // Always fall back to local authentication.
  if (was_authenticated || AuthenticateLocally(email)) {
    if (AuthWatcherEvent::SERVICE_CONNECTION_FAILED == event.what_happened)
      return true;
  }
  CHECK(event.what_happened != AuthWatcherEvent::ILLEGAL_VALUE);
  NotifyListeners(&event);
  return true;
}

bool AuthWatcher::AuthenticateLocally(string email) {
  user_settings_->GetEmailForSignin(&email);
  if (file_util::PathExists(FilePath(dirman_->GetSyncDataDatabasePath()))) {
    gaia_->SetUsername(email);
    status_ = LOCALLY_AUTHENTICATED;
    user_settings_->SwitchUser(email);
    PathString share_name;
    CHECK(AppendUTF8ToPathString(email.data(), email.size(), &share_name));
    LoadDirectoryListAndOpen(share_name);
    NotifyAuthSucceeded(email);
    return true;
  } else {
    return false;
  }
}

bool AuthWatcher::AuthenticateLocally(string email, const string& password) {
  user_settings_->GetEmailForSignin(&email);
  return user_settings_->VerifyAgainstStoredHash(email, password)
    && AuthenticateLocally(email);
}

void AuthWatcher::ProcessGaiaAuthFailure() {
  GaiaAuthenticator::AuthResults results = gaia_->results();
  if (LOCALLY_AUTHENTICATED == status_) {
    return;  // nothing todo
  } else if (AuthenticateLocally(results.email, results.password)) {
    // We save the "Remember me" checkbox by putting a non-null auth
    // token into the last_user table.  So if we're offline and the
    // user checks the box, insert a bogus auth token.
    if (PERSIST_TO_DISK == results.credentials_saved) {
      const string auth_token("bogus");
      user_settings_->SetAuthTokenForService(results.email,
                                             SYNC_SERVICE_NAME,
                                             auth_token);
    }
    const bool unavailable = ConnectionUnavailable == results.auth_error ||
                             Unknown == results.auth_error ||
                             ServiceUnavailable == results.auth_error;
    if (unavailable)
      return;
  }
  AuthWatcherEvent myevent = { AuthWatcherEvent::GAIA_AUTH_FAILED, &results };
  NotifyListeners(&myevent);
}

void* AuthWatcher::AuthenticationThreadMain(ThreadParams* args) {
  NameCurrentThreadForDebugging("SyncEngine_AuthWatcherThread");
  {
    // This short lock ensures our launching function (StartNewAuthAttempt) is
    // done.
    MutexLock lock(&mutex_);
    current_attempt_trigger_ = args->trigger;
  }
  SaveCredentials save = args->persist_creds_to_disk ?
      PERSIST_TO_DISK : SAVE_IN_MEMORY_ONLY;
  int attempt = 0;
  SignIn const signin = user_settings_->
    RecallSigninType(args->email, GMAIL_SIGNIN);

  if (!args->password.empty()) while (true) {
    bool authenticated;
    if (!args->captcha_token.empty() && !args->captcha_value.empty())
      authenticated = gaia_->Authenticate(args->email, args->password,
                                          save, true, args->captcha_token,
                                          args->captcha_value, signin);
    else
      authenticated = gaia_->Authenticate(args->email, args->password,
                                          save, true, signin);
    if (authenticated) {
      if (!ProcessGaiaAuthSuccess()) {
        if (3 != ++attempt)
          continue;
        AuthWatcherEvent event =
            { AuthWatcherEvent::SERVICE_CONNECTION_FAILED, 0 };
        NotifyListeners(&event);
      }
    } else {
      ProcessGaiaAuthFailure();
    }
    break;
  } else if (!args->auth_token.empty()) {
    AuthenticateWithToken(args->email, args->auth_token);
  } else {
    LOG(ERROR) << "Attempt to authenticate with no credentials.";
  }
  {
    MutexLock lock(&mutex_);
    authenticating_now_ = false;
  }
  delete args;
  return 0;
}

void AuthWatcher::Reset() {
  status_ = NOT_AUTHENTICATED;
}

void AuthWatcher::NotifyAuthSucceeded(const string& email) {
  LOG(INFO) << "NotifyAuthSucceeded";
  AuthWatcherEvent event = { AuthWatcherEvent::AUTH_SUCCEEDED };
  event.user_email = email;

  NotifyListeners(&event);
}

bool AuthWatcher::StartNewAuthAttempt(const string& email,
    const string& password, const string& auth_token,
    const string& captcha_token, const string& captcha_value,
    bool persist_creds_to_disk,
    AuthWatcherEvent::AuthenticationTrigger trigger) {
  AuthWatcherEvent event = { AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START };
  NotifyListeners(&event);
  MutexLock lock(&mutex_);
  if (authenticating_now_)
    return false;
  if (thread_handle_valid_) {
    int join_return = pthread_join(thread_, 0);
    if (0 != join_return)
      LOG(ERROR) << "pthread_join failed returning " << join_return;
  }
  string mail = email;
  if (email.find('@') == string::npos) {
    mail.push_back('@');
    // TODO(chron): Should this be done only at the UI level?
    mail.append(DEFAULT_SIGNIN_DOMAIN);
  }
  ThreadParams* args = new ThreadParams;
  args->self = this;
  args->email = mail;
  args->password = password;
  args->auth_token = auth_token;
  args->captcha_token = captcha_token;
  args->captcha_value = captcha_value;
  args->persist_creds_to_disk = persist_creds_to_disk;
  args->trigger = trigger;
  if (0 != pthread_create(&thread_, NULL, AuthenticationThreadStartRoutine,
                          args)) {
    LOG(ERROR) << "Failed to create auth thread.";
    return false;
  }
  authenticating_now_ = true;
  thread_handle_valid_ = true;
  return true;
}

void AuthWatcher::WaitForAuthThreadFinish() {
  {
    MutexLock lock(&mutex_);
    if (!thread_handle_valid_)
      return;
  }
  pthread_join(thread_, 0);
}

void AuthWatcher::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  if (event.server_reachable &&
      !authenticating_now_ &&
      (event.connection_code == HttpResponse::SYNC_AUTH_ERROR ||
      status_ == LOCALLY_AUTHENTICATED)) {
    // We're either online or just got reconnected and want to try to
    // authenticate. If we've got a saved token this should just work. If not
    // the auth failure should trigger UI indications that we're not logged in.

    // METRIC: If we get a SYNC_AUTH_ERROR, our token expired.
    GaiaAuthenticator::AuthResults authresults = gaia_->results();
    if (!StartNewAuthAttempt(authresults.email, authresults.password,
                             authresults.auth_token, "", "",
                             PERSIST_TO_DISK == authresults.credentials_saved,
                             AuthWatcherEvent::EXPIRED_CREDENTIALS))
      LOG(INFO) << "Couldn't start a new auth attempt.";
  }
}

bool AuthWatcher::LoadDirectoryListAndOpen(const PathString& login) {
  LOG(INFO) << "LoadDirectoryListAndOpen(" << login << ")";
  bool initial_sync_ended = false;

  dirman_->Open(login);
  syncable::ScopedDirLookup dir(dirman_, login);
  if (dir.good() && dir->initial_sync_ended())
    initial_sync_ended = true;

  LOG(INFO) << "LoadDirectoryListAndOpen returning " << initial_sync_ended;
  return initial_sync_ended;
}

AuthWatcher::~AuthWatcher() {
  WaitForAuthThreadFinish();
}

void AuthWatcher::Authenticate(const string& email, const string& password,
    const string& captcha_token, const string& captcha_value,
    bool persist_creds_to_disk) {
  LOG(INFO) << "AuthWatcher::Authenticate called";
  WaitForAuthThreadFinish();

  // We CHECK here because WaitForAuthThreadFinish should ensure there's no
  // ongoing auth attempt.
  string empty;
  CHECK(StartNewAuthAttempt(email, password, empty, captcha_token,
                            captcha_value, persist_creds_to_disk,
                            AuthWatcherEvent::USER_INITIATED));
}

void AuthWatcher::Logout() {
  scm_->ResetAuthStatus();
  Reset();
  WaitForAuthThreadFinish();
  ClearAuthenticationData();
}

void AuthWatcher::ClearAuthenticationData() {
  sync_service_token_.clear();
  scm_->set_auth_token(sync_service_token());
  user_settings_->ClearAllServiceTokens();
}

string AuthWatcher::email() const {
  return gaia_->email();
}

void AuthWatcher::NotifyListeners(AuthWatcherEvent* event) {
  event->trigger = current_attempt_trigger_;
  channel_->NotifyListeners(*event);
}

}  // namespace browser_sync
