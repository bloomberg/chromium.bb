// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AuthWatcher watches authentication events and user open and close
// events and accordingly opens and closes shares.

#ifndef CHROME_BROWSER_SYNC_ENGINE_AUTH_WATCHER_H_
#define CHROME_BROWSER_SYNC_ENGINE_AUTH_WATCHER_H_

#include <map>
#include <string>

#include "base/atomicops.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/net/gaia_authenticator.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "chrome/browser/sync/util/pthread_helpers.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace syncable {
struct DirectoryManagerEvent;
class DirectoryManager;
}

namespace browser_sync {
class AllStatus;
class AuthWatcher;
class ServerConnectionManager;
class TalkMediator;
class URLFactory;
class UserSettings;
struct ServerConnectionEvent;

struct AuthWatcherEvent {
  enum WhatHappened {
    AUTHENTICATION_ATTEMPT_START,
    AUTHWATCHER_DESTROYED,
    AUTH_SUCCEEDED,
    GAIA_AUTH_FAILED,
    SERVICE_USER_NOT_SIGNED_UP,
    SERVICE_AUTH_FAILED,
    SERVICE_CONNECTION_FAILED,
    // Used in a safety check in AuthWatcher::AuthenticateWithToken()
    ILLEGAL_VALUE,
  };
  WhatHappened what_happened;
  const GaiaAuthenticator::AuthResults* auth_results;
  // use AuthWatcherEvent as its own traits type in hookups.
  typedef AuthWatcherEvent EventType;
  static inline bool IsChannelShutdownEvent(const AuthWatcherEvent& event) {
    return event.what_happened == AUTHWATCHER_DESTROYED;
  }

  // Used for AUTH_SUCCEEDED notification
  std::string user_email;

  // How was this auth attempt initiated?
  enum AuthenticationTrigger {
    USER_INITIATED = 0,  // default value.
    EXPIRED_CREDENTIALS,
  };

  AuthenticationTrigger trigger;
};

class AuthWatcher {
 public:
  // Normal progression is local -> gaia -> token
  enum Status { LOCALLY_AUTHENTICATED, GAIA_AUTHENTICATED, NOT_AUTHENTICATED };
  typedef syncable::DirectoryManagerEvent DirectoryManagerEvent;
  typedef syncable::DirectoryManager DirectoryManager;
  typedef TalkMediator TalkMediator;

  AuthWatcher(DirectoryManager* dirman,
              ServerConnectionManager* scm,
              AllStatus* allstatus,
              const std::string& user_agent,
              const std::string& service_id,
              const std::string& gaia_url,
              UserSettings* user_settings,
              GaiaAuthenticator* gaia_auth,
              TalkMediator* talk_mediator);
  ~AuthWatcher();

  // Returns true if the open share has gotten zero
  // updates from the sync server (initial sync complete.)
  bool LoadDirectoryListAndOpen(const PathString& login);

  typedef EventChannel<AuthWatcherEvent, PThreadMutex> Channel;

  inline Channel* channel() const {
    return channel_.get();
  }

  void Authenticate(const std::string& email, const std::string& password,
      const std::string& captcha_token, const std::string& captcha_value,
      bool persist_creds_to_disk);

  void Authenticate(const std::string& email, const std::string& password,
      bool persist_creds_to_disk) {
    Authenticate(email, password, "", "", persist_creds_to_disk);
  }

  // Retrieves an auth token for a named service for which a long-lived token
  // was obtained at login time. Returns true if a long-lived token can be
  // found, false otherwise.
  bool GetAuthTokenForService(const std::string& service_name,
                              std::string* service_token);

  std::string email() const;
  syncable::DirectoryManager* dirman() const { return dirman_; }
  ServerConnectionManager* scm() const { return scm_; }
  AllStatus* allstatus() const { return allstatus_; }
  UserSettings* settings() const { return user_settings_; }
  Status status() const { return (Status)status_; }

  void Logout();

  // For synchronizing other destructors.
  void WaitForAuthThreadFinish();

 protected:
  void Reset();
  void ClearAuthenticationData();

  void NotifyAuthSucceeded(const std::string& email);
  bool StartNewAuthAttempt(const std::string& email,
      const std::string& password,
      const std::string& auth_token, const std::string& captcha_token,
      const std::string& captcha_value, bool persist_creds_to_disk,
      AuthWatcherEvent::AuthenticationTrigger trigger);
  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  void SaveUserSettings(const std::string& username,
                        const std::string& auth_token,
                        const bool save_credentials);

  // These two helpers should only be called from the auth function.
  // returns false iff we had problems and should try GAIA_AUTH again.
  bool ProcessGaiaAuthSuccess();
  void ProcessGaiaAuthFailure();

  // Just checks that the user has at least one local share cache.
  bool AuthenticateLocally(std::string email);
  // Also checks the user's password against stored password hash.
  bool AuthenticateLocally(std::string email, const std::string& password);

  // Sets the trigger member of the event and sends the event on channel_.
  void NotifyListeners(AuthWatcherEvent* event);

  const std::string& sync_service_token() const { return sync_service_token_; }

 public:
  bool AuthenticateWithToken(const std::string& email,
                             const std::string& auth_token);

 protected:
  typedef PThreadScopedLock<PThreadMutex> MutexLock;

  // Passed to newly created threads.
  struct ThreadParams {
    AuthWatcher* self;
    std::string email;
    std::string password;
    std::string auth_token;
    std::string captcha_token;
    std::string captcha_value;
    bool persist_creds_to_disk;
    AuthWatcherEvent::AuthenticationTrigger trigger;
  };

  // Initial function passed to pthread_create.
  static void* AuthenticationThreadStartRoutine(void* arg);
  // Member function called by AuthenticationThreadStartRoutine.
  void* AuthenticationThreadMain(struct ThreadParams* arg);

  scoped_ptr<GaiaAuthenticator> const gaia_;
  syncable::DirectoryManager* const dirman_;
  ServerConnectionManager* const scm_;
  scoped_ptr<EventListenerHookup> connmgr_hookup_;
  AllStatus* const allstatus_;
  // TODO(chron): It is incorrect to make assignments to AtomicWord.
  volatile base::subtle::AtomicWord status_;
  UserSettings* user_settings_;
  TalkMediator* talk_mediator_;  // Interface to the notifications engine.
  scoped_ptr<Channel> channel_;

  // We store our service token in memory as a workaround to the fact that we
  // don't persist it when the user unchecks "remember me".
  // We also include it on outgoing requests.
  std::string sync_service_token_;

  PThreadMutex mutex_;
  // All members below are protected by the above mutex
  pthread_t thread_;
  bool thread_handle_valid_;
  bool authenticating_now_;
  AuthWatcherEvent::AuthenticationTrigger current_attempt_trigger_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_AUTH_WATCHER_H_
