// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/auth_watcher.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/authenticator.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/common/deprecated/event_sys-inl.h"
#include "chrome/common/net/gaia/gaia_authenticator.h"

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
                         const string& user_agent,
                         const string& service_id,
                         const string& gaia_url,
                         UserSettings* user_settings,
                         gaia::GaiaAuthenticator* gaia_auth)
    : gaia_(gaia_auth),
      dirman_(dirman),
      scm_(scm),
      status_(NOT_AUTHENTICATED),
      user_settings_(user_settings),
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
  gaia::GaiaAuthenticator::AuthResults results = gaia_->results();

  // We just successfully signed in again, let's clear out any residual cached
  // login data from earlier sessions.
  ClearAuthenticationData();

  user_settings_->StoreEmailForSignin(results.email, results.primary_email);
  results.email = results.primary_email;
  gaia_->SetUsernamePassword(results.primary_email, results.password);
  if (!user_settings_->VerifyAgainstStoredHash(results.email, results.password))
    user_settings_->StoreHashedPassword(results.email, results.password);

  user_settings_->SetAuthTokenForService(results.email,
                                         SYNC_SERVICE_NAME,
                                         gaia_->auth_token());
}

// TODO(chron): Full integration test suite needed. http://crbug.com/35429
void AuthWatcher::RenewAuthToken(const std::string& updated_token) {
  message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &AuthWatcher::DoRenewAuthToken, updated_token));
}

void AuthWatcher::DoRenewAuthToken(const std::string& updated_token) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  // TODO(chron): We should probably only store auth token in one place.
  if (scm_->auth_token() == updated_token) {
    return;  // This thread is the only one writing to the SCM's auth token.
  }
  LOG(INFO) << "Updating auth token:" << updated_token;
  scm_->set_auth_token(updated_token);
  gaia_->RenewAuthToken(updated_token);  // Must be on AuthWatcher thread
  user_settings_->SetAuthTokenForService(user_settings_->email(),
                                         SYNC_SERVICE_NAME,
                                         updated_token);

  NotifyAuthChanged(user_settings_->email(), updated_token, true);
}

void AuthWatcher::AuthenticateWithLsid(const std::string& lsid) {
  message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &AuthWatcher::DoAuthenticateWithLsid, lsid));
}

void AuthWatcher::DoAuthenticateWithLsid(const std::string& lsid) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  AuthWatcherEvent event = { AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START };
  NotifyListeners(&event);

  if (gaia_->AuthenticateWithLsid(lsid)) {
    PersistCredentials();
    DoAuthenticateWithToken(gaia_->email(), gaia_->auth_token());
  } else {
    ProcessGaiaAuthFailure();
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
  string email = gaia_email;
  if (auth.display_email() && *auth.display_email()) {
    email = auth.display_email();
    LOG(INFO) << "Auth returned email " << email << " for gaia email " <<
        gaia_email;
  }

  AuthWatcherEvent event = {AuthWatcherEvent::ILLEGAL_VALUE , 0};
  gaia_->SetUsername(email);
  gaia_->SetAuthToken(auth_token);
  const bool was_authenticated = NOT_AUTHENTICATED != status_;
  switch (result) {
    case Authenticator::SUCCESS:
      {
        status_ = GAIA_AUTHENTICATED;
        const std::string& share_name = email;
        user_settings_->SwitchUser(email);
        scm_->set_auth_token(auth_token);

        if (!was_authenticated) {
          LOG(INFO) << "Opening DB for AuthenticateWithToken ("
                    << share_name << ")";
          dirman_->Open(share_name);
        }
        NotifyAuthChanged(email, auth_token, false);
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
  if (was_authenticated || AuthenticateLocally(email)) {
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
    LOG(INFO) << "Opening DB for AuthenticateLocally (" << email << ")";
    dirman_->Open(email);
    NotifyAuthChanged(email, "", false);
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
  gaia::GaiaAuthenticator::AuthResults results = gaia_->results();
  if (LOCALLY_AUTHENTICATED != status_ &&
      AuthenticateLocally(results.email, results.password)) {
    // TODO(chron): Do we really want a bogus token?
    const string auth_token("bogus");
    user_settings_->SetAuthTokenForService(results.email,
                                           SYNC_SERVICE_NAME,
                                           auth_token);
  }
  AuthWatcherEvent myevent = { AuthWatcherEvent::GAIA_AUTH_FAILED, &results };
  NotifyListeners(&myevent);
}

void AuthWatcher::DoAuthenticate(const AuthRequest& request) {
  DCHECK_EQ(MessageLoop::current(), message_loop());

  AuthWatcherEvent event = { AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START };
  NotifyListeners(&event);

  current_attempt_trigger_ = request.trigger;

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
                                          captcha_token,
                                          request.captcha_value);
    } else {
      authenticated = gaia_->Authenticate(request.email, request.password);
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

void AuthWatcher::NotifyAuthChanged(const string& email,
                                    const string& auth_token,
                                    bool renewed) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  LOG(INFO) << "NotifyAuthSucceeded";
  AuthWatcherEvent event = {
    renewed ?
        AuthWatcherEvent::AUTH_RENEWED :
        AuthWatcherEvent::AUTH_SUCCEEDED
  };
  event.user_email = email;
  event.auth_token = auth_token;

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
    gaia::GaiaAuthenticator::AuthResults authresults = gaia_->results();
    AuthRequest request = { authresults.email, authresults.password,
                            authresults.auth_token, std::string(),
                            std::string(),
                            AuthWatcherEvent::EXPIRED_CREDENTIALS };
    DoAuthenticate(request);
  }
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
    const string& captcha_token, const string& captcha_value) {
  LOG(INFO) << "AuthWatcher::Authenticate called";

  string empty;
  AuthRequest request = { FormatAsEmailAddress(email), password, empty,
                          captcha_token, captcha_value,
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
