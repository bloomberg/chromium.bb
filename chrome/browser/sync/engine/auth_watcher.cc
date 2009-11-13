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
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
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
      auth_backend_thread_("SyncEngine_AuthWatcherThread"),
      current_attempt_trigger_(AuthWatcherEvent::USER_INITIATED) {

  if (!auth_backend_thread_.Start())
    NOTREACHED() << "Couldn't start SyncEngine_AuthWatcherThread";

  gaia_->set_message_loop(message_loop());

  connmgr_hookup_.reset(
      NewEventListenerHookup(scm->channel(), this,
                             &AuthWatcher::HandleServerConnectionEvent));
  AuthWatcherEvent done = { AuthWatcherEvent::AUTHWATCHER_DESTROYED };
  channel_.reset(new Channel(done));
}

void AuthWatcher::PersistCredentials() {
  DCHECK_EQ(MessageLoop::current(), message_loop());
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
}

const char kAuthWatcher[] = "AuthWatcher";

void AuthWatcher::AuthenticateWithToken(const std::string& gaia_email,
                                        const std::string& auth_token) {
  message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &AuthWatcher::DoAuthenticateWithToken, gaia_email, auth_token));
}

void AuthWatcher::DoAuthenticateWithToken(const std::string& gaia_email,
                                          const std::string& auth_token) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  Authenticator auth(scm_, user_settings_);
  Authenticator::AuthenticationResult result =
      auth.AuthenticateToken(auth_token);
  AuthWatcherEvent event = {AuthWatcherEvent::ILLEGAL_VALUE , 0};
  gaia_->SetUsername(gaia_email);
  gaia_->SetAuthToken(auth_token, SAVE_IN_MEMORY_ONLY);
  const bool was_authenticated = NOT_AUTHENTICATED != status_;
  switch (result) {
    case Authenticator::SUCCESS:
      {
        status_ = GAIA_AUTHENTICATED;
        const PathString& share_name = gaia_email;
        user_settings_->SwitchUser(gaia_email);

        // Set the authentication token for notifications
        talk_mediator_->SetAuthToken(gaia_email, auth_token);
        scm_->set_auth_token(auth_token);

        if (!was_authenticated)
          LoadDirectoryListAndOpen(share_name);
        NotifyAuthSucceeded(gaia_email);
        return;
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
    return;
  }
  // Always fall back to local authentication.
  if (was_authenticated || AuthenticateLocally(gaia_email)) {
    if (AuthWatcherEvent::SERVICE_CONNECTION_FAILED == event.what_happened)
      return;
  }
  DCHECK_NE(event.what_happened, AuthWatcherEvent::ILLEGAL_VALUE);
  NotifyListeners(&event);
}

bool AuthWatcher::AuthenticateLocally(string email) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  user_settings_->GetEmailForSignin(&email);
  if (file_util::PathExists(FilePath(dirman_->GetSyncDataDatabasePath()))) {
    gaia_->SetUsername(email);
    status_ = LOCALLY_AUTHENTICATED;
    user_settings_->SwitchUser(email);
    const PathString& share_name = email;
    LoadDirectoryListAndOpen(share_name);
    NotifyAuthSucceeded(email);
    return true;
  } else {
    return false;
  }
}

bool AuthWatcher::AuthenticateLocally(string email, const string& password) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  user_settings_->GetEmailForSignin(&email);
  return user_settings_->VerifyAgainstStoredHash(email, password)
    && AuthenticateLocally(email);
}

void AuthWatcher::ProcessGaiaAuthFailure() {
  DCHECK_EQ(MessageLoop::current(), message_loop());
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

void AuthWatcher::DoAuthenticate(const AuthRequest& request) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  AuthWatcherEvent event = { AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START };
  NotifyListeners(&event);

  current_attempt_trigger_ = request.trigger;

  SaveCredentials save = request.persist_creds_to_disk ?
      PERSIST_TO_DISK : SAVE_IN_MEMORY_ONLY;
  SignIn const signin = user_settings_->
      RecallSigninType(request.email, GMAIL_SIGNIN);

  // We let the caller be lazy and try using the last captcha token seen by
  // the gaia authenticator if they haven't provided a token but have sent
  // a challenge response. Of course, if the captcha token is specified,
  // we use that one instead.
  std::string captcha_token(request.captcha_token);
  if (!request.captcha_value.empty() && captcha_token.empty())
    captcha_token = gaia_->captcha_token();

  if (!request.password.empty()) {
    bool authenticated = false;
    if (!captcha_token.empty()) {
      authenticated = gaia_->Authenticate(request.email, request.password,
                                          save, captcha_token,
                                          request.captcha_value, signin);
    } else {
      authenticated = gaia_->Authenticate(request.email, request.password,
                                          save, signin);
    }
    if (authenticated) {
      PersistCredentials();
      DoAuthenticateWithToken(gaia_->email(), gaia_->auth_token());
    } else {
      ProcessGaiaAuthFailure();
    }
  } else if (!request.auth_token.empty()) {
      DoAuthenticateWithToken(request.email, request.auth_token);
  } else {
      LOG(ERROR) << "Attempt to authenticate with no credentials.";
  }
}

void AuthWatcher::NotifyAuthSucceeded(const string& email) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  LOG(INFO) << "NotifyAuthSucceeded";
  AuthWatcherEvent event = { AuthWatcherEvent::AUTH_SUCCEEDED };
  event.user_email = email;

  NotifyListeners(&event);
}

void AuthWatcher::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &AuthWatcher::DoHandleServerConnectionEvent, event,
          scm_->auth_token()));
}

void AuthWatcher::DoHandleServerConnectionEvent(
    const ServerConnectionEvent& event,
    const std::string& auth_token_snapshot) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  if (event.server_reachable &&
      // If the auth_token at the time of the event differs from the current
      // one, we have authenticated since then and don't need to re-try.
      (auth_token_snapshot == gaia_->auth_token()) &&
      (event.connection_code == HttpResponse::SYNC_AUTH_ERROR ||
       status_ == LOCALLY_AUTHENTICATED)) {
    // We're either online or just got reconnected and want to try to
    // authenticate. If we've got a saved token this should just work. If not
    // the auth failure should trigger UI indications that we're not logged in.

    // METRIC: If we get a SYNC_AUTH_ERROR, our token expired.
    GaiaAuthenticator::AuthResults authresults = gaia_->results();
    AuthRequest request = { authresults.email, authresults.password,
                            authresults.auth_token, std::string(),
                            std::string(),
                            PERSIST_TO_DISK == authresults.credentials_saved,
                            AuthWatcherEvent::EXPIRED_CREDENTIALS };
    DoAuthenticate(request);
  }
}

bool AuthWatcher::LoadDirectoryListAndOpen(const PathString& login) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
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
  auth_backend_thread_.Stop();
  // The gaia authenticator takes a const MessageLoop* because it only uses it
  // to ensure all methods are invoked on the given loop.  Once our thread has
  // stopped, the current message loop will be NULL, and no methods should be
  // invoked on |gaia_| after this point.  We could set it to NULL, but
  // abstaining allows for even more sanity checking that nothing is invoked on
  // it from now on.
}

void AuthWatcher::Authenticate(const string& email, const string& password,
    const string& captcha_token, const string& captcha_value,
    bool persist_creds_to_disk) {
  LOG(INFO) << "AuthWatcher::Authenticate called";

  string empty;
  AuthRequest request = { FormatAsEmailAddress(email), password, empty,
                          captcha_token, captcha_value, persist_creds_to_disk,
                          AuthWatcherEvent::USER_INITIATED };
  message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &AuthWatcher::DoAuthenticate, request));
}

void AuthWatcher::ClearAuthenticationData() {
  scm_->set_auth_token(std::string());
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
