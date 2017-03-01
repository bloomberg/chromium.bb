// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_BASE_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/signin_manager_wrapper.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/js/sync_js_controller.h"
#include "components/version_info/version_info.h"

namespace syncer {

// This is a base class for implementations of SyncService that contains some
// common functionality and member variables. Anything that can live inside the
// sync component should eventually live here instead of a concrete
// implementation. This is set up as a base class so things can be transferred
// piece by piece as easily as possible.
class SyncServiceBase : public SyncService, public SyncEngineHost {
 public:
  SyncServiceBase(std::unique_ptr<SyncClient> sync_client,
                  std::unique_ptr<SigninManagerWrapper> signin,
                  const version_info::Channel& channel,
                  const base::FilePath& base_directory,
                  const std::string& debug_identifier);
  ~SyncServiceBase() override;

  // SyncService partial implementation.
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;

 protected:
  // Notify all observers that a change has occurred.
  void NotifyObservers();

  // Kicks off asynchronous initialization of the SyncEngine.
  void InitializeEngine();

  // Destroys the |crypto_| object and creates a new one with fresh state.
  void ResetCryptoState();

  // Returns SyncCredentials from the OAuth2TokenService.
  virtual SyncCredentials GetCredentials() = 0;

  // Returns a weak handle to the JsEventHandler.
  virtual WeakHandle<JsEventHandler> GetJsEventHandler() = 0;

  // Returns a callback that makes an HttpPostProviderFactory.
  virtual SyncEngine::HttpPostProviderFactoryGetter
  MakeHttpPostProviderFactoryGetter() = 0;

  // Returns a weak handle to an UnrecoverableErrorHandler (may be |this|).
  virtual WeakHandle<UnrecoverableErrorHandler>
  GetUnrecoverableErrorHandler() = 0;

  // This profile's SyncClient, which abstracts away non-Sync dependencies and
  // the Sync API component factory.
  const std::unique_ptr<SyncClient> sync_client_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address.
  const std::unique_ptr<SigninManagerWrapper> signin_;

  // The product channel of the embedder.
  const version_info::Channel channel_;

  // The path to the base directory under which sync should store its
  // information.
  const base::FilePath base_directory_;

  // The full path to the sync data folder. The folder is not fully deleted when
  // sync is disabled, since it holds both Directory and ModelTypeStore data.
  // Directory files will be selectively targeted instead.
  const base::FilePath sync_data_folder_;

  // An identifier representing this instance for debugging purposes.
  const std::string debug_identifier_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  SyncPrefs sync_prefs_;

  // A utility object containing logic and state relating to encryption. It is
  // never null.
  std::unique_ptr<SyncServiceCrypto> crypto_;

  // The thread where all the sync operations happen. This thread is kept alive
  // until browser shutdown and reused if sync is turned off and on again. It is
  // joined during the shutdown process, but there is an abort mechanism in
  // place to prevent slow HTTP requests from blocking browser shutdown.
  std::unique_ptr<base::Thread> sync_thread_;

  // Our asynchronous engine to communicate with sync components living on
  // other threads.
  std::unique_ptr<SyncEngine> engine_;

  // The list of observers of the SyncService state.
  base::ObserverList<SyncServiceObserver> observers_;

  // Used to ensure that certain operations are performed on the thread that
  // this object was created on.
  base::ThreadChecker thread_checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncServiceBase);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_BASE_H_
