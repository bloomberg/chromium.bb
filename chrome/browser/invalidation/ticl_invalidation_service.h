// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_
#define CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/invalidation/invalidation_auth_provider.h"
#include "chrome/browser/invalidation/invalidation_logger.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidator_registrar.h"

class Profile;

namespace net {
class URLRequestContextGetter;
}

namespace syncer {
class InvalidationStateTracker;
class Invalidator;
}

namespace invalidation {
class GCMInvalidationBridge;

// This InvalidationService wraps the C++ Invalidation Client (TICL) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
class TiclInvalidationService : public base::NonThreadSafe,
                                public InvalidationService,
                                public OAuth2TokenService::Consumer,
                                public OAuth2TokenService::Observer,
                                public InvalidationAuthProvider::Observer,
                                public syncer::InvalidationHandler {
 public:
  enum InvalidationNetworkChannel {
    PUSH_CLIENT_CHANNEL = 0,
    GCM_NETWORK_CHANNEL = 1,

    // This enum is used in UMA_HISTOGRAM_ENUMERATION. Insert new values above
    // this line.
    NETWORK_CHANNELS_COUNT = 2
  };

  TiclInvalidationService(
      scoped_ptr<InvalidationAuthProvider> auth_provider,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      Profile* profile);
  virtual ~TiclInvalidationService();

  void Init(
      scoped_ptr<syncer::InvalidationStateTracker> invalidation_state_tracker);

  // InvalidationService implementation.
  // It is an error to have registered handlers when Shutdown() is called.
  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;
  virtual InvalidationLogger* GetInvalidationLogger() OVERRIDE;
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> caller) const OVERRIDE;
  virtual InvalidationAuthProvider* GetInvalidationAuthProvider() OVERRIDE;

  void RequestAccessToken();

  // OAuth2TokenService::Consumer implementation
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE;

  // OAuth2TokenService::Observer implementation
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;

  // InvalidationAuthProvider::Observer implementation.
  virtual void OnInvalidationAuthLogout() OVERRIDE;

  // syncer::InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual std::string GetOwnerName() const OVERRIDE;

  // Overrides KeyedService method.
  virtual void Shutdown() OVERRIDE;

 protected:
  // Initializes with an injected invalidator.
  void InitForTest(
      scoped_ptr<syncer::InvalidationStateTracker> invalidation_state_tracker,
      syncer::Invalidator* invalidator);

  friend class TiclInvalidationServiceTestDelegate;
  friend class TiclInvalidationServiceChannelTest;

 private:
  bool IsReadyToStart();
  bool IsStarted() const;

  void StartInvalidator(InvalidationNetworkChannel network_channel);
  void UpdateInvalidationNetworkChannel();
  void UpdateInvalidatorCredentials();
  void StopInvalidator();

  Profile *const profile_;
  scoped_ptr<InvalidationAuthProvider> auth_provider_;

  scoped_ptr<syncer::InvalidatorRegistrar> invalidator_registrar_;
  scoped_ptr<syncer::InvalidationStateTracker> invalidation_state_tracker_;
  scoped_ptr<syncer::Invalidator> invalidator_;

  // TiclInvalidationService needs to remember access token in order to
  // invalidate it with OAuth2TokenService.
  std::string access_token_;

  // TiclInvalidationService needs to hold reference to access_token_request_
  // for the duration of request in order to receive callbacks.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  base::OneShotTimer<TiclInvalidationService> request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  PrefChangeRegistrar pref_change_registrar_;
  InvalidationNetworkChannel network_channel_type_;
  scoped_ptr<GCMInvalidationBridge> gcm_invalidation_bridge_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  // Keep a copy of the important parameters used in network channel creation
  // for debugging.
  base::DictionaryValue network_channel_options_;

  DISALLOW_COPY_AND_ASSIGN(TiclInvalidationService);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_
