// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_
#define CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/invalidation/invalidation_frontend.h"
#include "chrome/browser/invalidation/invalidator_storage.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidator_registrar.h"

class Profile;
class SigninManagerBase;
class TokenService;

namespace syncer {
class Invalidator;
}

namespace invalidation {

// This InvalidationService wraps the C++ Invalidation Client (TICL) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
class TiclInvalidationService
    : public base::NonThreadSafe,
      public BrowserContextKeyedService,
      public InvalidationFrontend,
      public content::NotificationObserver,
      public syncer::InvalidationHandler {
 public:
  TiclInvalidationService(SigninManagerBase* signin,
                          TokenService* token_service,
                          Profile* profile);
  virtual ~TiclInvalidationService();

  void Init();

  // InvalidationFrontend implementation.
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
  virtual std::string GetInvalidatorClientId() const;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  void Start();
  void UpdateToken();
  void StopInvalidator();
  void Logout();

  Profile *const profile_;
  SigninManagerBase *const signin_manager_;
  TokenService *const token_service_;

  scoped_ptr<syncer::InvalidatorRegistrar> invalidator_registrar_;
  scoped_ptr<InvalidatorStorage> invalidator_storage_;
  scoped_ptr<syncer::Invalidator> invalidator_;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TiclInvalidationService);
};

}

#endif  // CHROME_BROWSER_INVALIDATION_TICL_INVALIDATION_SERVICE_H_
