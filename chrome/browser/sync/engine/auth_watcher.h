// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AuthWatcher watches authentication events and user open and close
// events and accordingly opens and closes shares.

#ifndef CHROME_BROWSER_SYNC_ENGINE_AUTH_WATCHER_H_
#define CHROME_BROWSER_SYNC_ENGINE_AUTH_WATCHER_H_

#include <map>
#include <string>

#include "base/atomicops.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/util/sync_types.h"
#include "chrome/common/deprecated/event_sys.h"
#include "chrome/common/net/gaia/gaia_authenticator.h"

namespace syncable {
struct DirectoryManagerEvent;
class DirectoryManager;
}

namespace browser_sync {

class AuthWatcher;
class ServerConnectionManager;
class URLFactory;
class UserSettings;
struct ServerConnectionEvent;

struct AuthWatcherEvent {
  enum WhatHappened {
    AUTHENTICATION_ATTEMPT_START,
    AUTHWATCHER_DESTROYED,
    AUTH_RENEWED,  // Currently only used in testing.
    AUTH_SUCCEEDED,
    GAIA_AUTH_FAILED,
    SERVICE_USER_NOT_SIGNED_UP,
    SERVICE_AUTH_FAILED,
    SERVICE_CONNECTION_FAILED,
    // Used in a safety check in AuthWatcher::AuthenticateWithToken()
    ILLEGAL_VALUE,
  };
  WhatHappened what_happened;
  const gaia::GaiaAuthenticator::AuthResults* auth_results;
  // use AuthWatcherEvent as its own traits type in hookups.
  typedef AuthWatcherEvent EventType;
  static inline bool IsChannelShutdownEvent(const AuthWatcherEvent& event) {
    return event.what_happened == AUTHWATCHER_DESTROYED;
  }

  // Used for AUTH_SUCCEEDED/AUTH_RENEWED notification.
  std::string user_email;
  // May be empty if we're only locally authenticated.
  std::string auth_token;

  // How was this auth attempt initiated?
  enum AuthenticationTrigger {
    USER_INITIATED = 0,  // default value.
    EXPIRED_CREDENTIALS,
  };

  AuthenticationTrigger trigger;
};

// The mother-class of Authentication for the sync backend.  Handles both gaia
// and sync service authentication via asynchronous Authenticate* methods,
// raising AuthWatcherEvents on success/failure.  The implementation currently
// runs its own backend thread for the actual auth processing, which means
// the AuthWatcherEvents can be raised on a different thread than the one that
// invoked authentication.
class AuthWatcher : public base::RefCountedThreadSafe<AuthWatcher> {
 friend class AuthWatcherTest;
 FRIEND_TEST_ALL_PREFIXES(AuthWatcherTest, Construction);
 public:
  // Normal progression is local -> gaia -> token.
  enum Status { LOCALLY_AUTHENTICATED, GAIA_AUTHENTICATED, NOT_AUTHENTICATED };
  typedef syncable::DirectoryManagerEvent DirectoryManagerEvent;
  typedef syncable::DirectoryManager DirectoryManager;

  AuthWatcher(DirectoryManager* dirman,
              ServerConnectionManager* scm,
              const std::string& user_agent,
              const std::string& service_id,
              const std::string& gaia_url,
              UserSettings* user_settings,
              gaia::GaiaAuthenticator* gaia_auth);
  ~AuthWatcher();

  typedef EventChannel<AuthWatcherEvent, Lock> Channel;

  inline Channel* channel() const {
    return channel_.get();
  }

  // The following 3 flavors of authentication routines are asynchronous and can
  // be called from any thread.
  // If |captcha_value| is specified but |captcha_token| is not, this will
  // attempt authentication using the last observed captcha token out of
  // convenience in the common case so the token doesn't have to be plumbed
  // everywhere.
  void Authenticate(const std::string& email, const std::string& password,
      const std::string& captcha_token, const std::string& captcha_value);

  void Authenticate(const std::string& email, const std::string& password,
      bool persist_creds_to_disk) {
    Authenticate(email, password, "", "");
  }

  // Use this to update only the token of the current email address.
  void RenewAuthToken(const std::string& updated_token);

  // Use this version when you don't need the gaia authentication step because
  // you already have a valid LSID cookie for |gaia_email|.
  void AuthenticateWithLsid(const std::string& lsid);

  // Use this version when you don't need the gaia authentication step because
  // you already have a valid token for |gaia_email|.
  void AuthenticateWithToken(const std::string& gaia_email,
                             const std::string& auth_token);

  // Joins on the backend thread.  The AuthWatcher is useless after this and
  // should be destroyed.
  void Shutdown() { auth_backend_thread_.Stop(); }

  std::string email() const;
  syncable::DirectoryManager* dirman() const { return dirman_; }
  ServerConnectionManager* scm() const { return scm_; }
  UserSettings* settings() const { return user_settings_; }
  Status status() const { return (Status)status_; }

 private:
  void ClearAuthenticationData();

  void NotifyAuthChanged(const std::string& email,
                         const std::string& auth_token,
                         bool renewed);
  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  void SaveUserSettings(const std::string& username,
                        const std::string& auth_token);

  MessageLoop* message_loop() { return auth_backend_thread_.message_loop(); }

  void DoRenewAuthToken(const std::string& updated_token);

  // These two helpers should only be called from the auth function.
  // Called when authentication with gaia succeeds, to save credential info.
  void PersistCredentials();
  // Called when authentication with gaia fails.
  void ProcessGaiaAuthFailure();

  // Just checks that the user has at least one local share cache.
  bool AuthenticateLocally(std::string email);
  // Also checks the user's password against stored password hash.
  bool AuthenticateLocally(std::string email, const std::string& password);

  // Sets the trigger member of the event and sends the event on channel_.
  void NotifyListeners(AuthWatcherEvent* event);

  inline std::string FormatAsEmailAddress(const std::string& email) const {
    std::string mail(email);
    if (email.find('@') == std::string::npos) {
      mail.push_back('@');
      // TODO(chron): Should this be done only at the UI level?
      mail.append(DEFAULT_SIGNIN_DOMAIN);
    }
    return mail;
  }

  // A struct to marshal various data across to the auth_backend_thread_ on
  // Authenticate() and AuthenticateWithToken calls.
  struct AuthRequest {
    std::string email;
    std::string password;
    std::string auth_token;
    std::string captcha_token;
    std::string captcha_value;
    bool persist_creds_to_disk;
    AuthWatcherEvent::AuthenticationTrigger trigger;
  };

  // The public interface Authenticate methods are proxies to these, which
  // can only be called from |auth_backend_thread_|.
  void DoAuthenticate(const AuthRequest& request);
  void DoAuthenticateWithLsid(const std::string& lsid);
  void DoAuthenticateWithToken(const std::string& email,
                               const std::string& auth_token);

  // The public HandleServerConnectionEvent method proxies to this method, which
  // can only be called on |auth_backend_thread_|.
  void DoHandleServerConnectionEvent(
      const ServerConnectionEvent& event,
      const std::string& auth_token_snapshot);

  scoped_ptr<gaia::GaiaAuthenticator> const gaia_;
  syncable::DirectoryManager* const dirman_;
  ServerConnectionManager* const scm_;
  scoped_ptr<EventListenerHookup> connmgr_hookup_;
  Status status_;
  UserSettings* const user_settings_;
  scoped_ptr<Channel> channel_;

  base::Thread auth_backend_thread_;

  AuthWatcherEvent::AuthenticationTrigger current_attempt_trigger_;
  DISALLOW_COPY_AND_ASSIGN(AuthWatcher);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_AUTH_WATCHER_H_
