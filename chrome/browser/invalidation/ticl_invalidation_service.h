// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_
#define CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "chrome/browser/invalidation/invalidator_storage.h"
#include "chrome/browser/signin/token_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidator_registrar.h"

class Profile;
class SigninManagerBase;

namespace syncer {
class Invalidator;
}

namespace invalidation {

// This InvalidationService wraps the C++ Invalidation Client (TICL) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
class TiclInvalidationService
    : public base::NonThreadSafe,
      public InvalidationService,
      public content::NotificationObserver,
      public OAuth2TokenService::Consumer,
      public syncer::InvalidationHandler {
 public:
  TiclInvalidationService(SigninManagerBase* signin,
                          TokenService* token_service,
                          OAuth2TokenService* oauth2_token_service,
                          Profile* profile);
  virtual ~TiclInvalidationService();

  void Init();

  // InvalidationService implementation.
  // It is an error to have registered handlers when Shutdown() is called.
  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void AcknowledgeInvalidation(
      const invalidation::ObjectId& id,
      const syncer::AckHandle& ack_handle) OVERRIDE;
  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void RequestAccessToken();

  // OAuth2TokenService::Consumer implementation
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE;

  // syncer::InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  // Overrides BrowserContextKeyedService method.
  virtual void Shutdown() OVERRIDE;

 protected:
  // Initializes with an injected invalidator.
  void InitForTest(syncer::Invalidator* invalidator);

  friend class TiclInvalidationServiceTestDelegate;

 private:
  bool IsReadyToStart();
  bool IsStarted();

  void StartInvalidator();
  void UpdateInvalidatorCredentials();
  void StopInvalidator();
  void Logout();

  Profile *const profile_;
  SigninManagerBase *const signin_manager_;
  TokenService *const token_service_;
  OAuth2TokenService *const oauth2_token_service_;

  scoped_ptr<syncer::InvalidatorRegistrar> invalidator_registrar_;
  scoped_ptr<InvalidatorStorage> invalidator_storage_;
  scoped_ptr<syncer::Invalidator> invalidator_;

  content::NotificationRegistrar notification_registrar_;

  // TiclInvalidationService needs to remember access token in order to
  // invalidate it with OAuth2TokenService.
  std::string access_token_;

  // TiclInvalidationService needs to hold reference to access_token_request_
  // for the duration of request in order to receive callbacks.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  base::OneShotTimer<TiclInvalidationService> request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  DISALLOW_COPY_AND_ASSIGN(TiclInvalidationService);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_
