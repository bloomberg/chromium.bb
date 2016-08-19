// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_profile_service.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if defined(OS_ANDROID)
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/gcm_driver/gcm_driver_android.h"
#else
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_account_tracker.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/account_tracker.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#endif

namespace gcm {

#if !defined(OS_ANDROID)
// Identity observer only has actual work to do when the user is actually signed
// in. It ensures that account tracker is taking
class GCMProfileService::IdentityObserver : public IdentityProvider::Observer {
 public:
  IdentityObserver(ProfileIdentityProvider* identity_provider,
                   net::URLRequestContextGetter* request_context,
                   GCMDriver* driver);
  ~IdentityObserver() override;

  // IdentityProvider::Observer:
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

 private:
  void StartAccountTracker(net::URLRequestContextGetter* request_context);

  GCMDriver* driver_;
  IdentityProvider* identity_provider_;
  std::unique_ptr<GCMAccountTracker> gcm_account_tracker_;

  // The account ID that this service is responsible for. Empty when the service
  // is not running.
  std::string account_id_;

  base::WeakPtrFactory<GCMProfileService::IdentityObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdentityObserver);
};

GCMProfileService::IdentityObserver::IdentityObserver(
    ProfileIdentityProvider* identity_provider,
    net::URLRequestContextGetter* request_context,
    GCMDriver* driver)
    : driver_(driver),
      identity_provider_(identity_provider),
      weak_ptr_factory_(this) {
  identity_provider_->AddObserver(this);

  OnActiveAccountLogin();
  StartAccountTracker(request_context);
}

GCMProfileService::IdentityObserver::~IdentityObserver() {
  if (gcm_account_tracker_)
    gcm_account_tracker_->Shutdown();
  identity_provider_->RemoveObserver(this);
}

void GCMProfileService::IdentityObserver::OnActiveAccountLogin() {
  // This might be called multiple times when the password changes.
  const std::string account_id = identity_provider_->GetActiveAccountId();
  if (account_id == account_id_)
    return;
  account_id_ = account_id;

  // Still need to notify GCMDriver for UMA purpose.
  driver_->OnSignedIn();
}

void GCMProfileService::IdentityObserver::OnActiveAccountLogout() {
  account_id_.clear();

  // Still need to notify GCMDriver for UMA purpose.
  driver_->OnSignedOut();
}

void GCMProfileService::IdentityObserver::StartAccountTracker(
    net::URLRequestContextGetter* request_context) {
  if (gcm_account_tracker_)
    return;

  std::unique_ptr<gaia::AccountTracker> gaia_account_tracker(
      new gaia::AccountTracker(identity_provider_, request_context));

  gcm_account_tracker_.reset(
      new GCMAccountTracker(std::move(gaia_account_tracker), driver_));

  gcm_account_tracker_->Start();
}

#endif  // !defined(OS_ANDROID)

// static
bool GCMProfileService::IsGCMEnabled(PrefService* prefs) {
#if defined(OS_ANDROID)
  return true;
#else
  return prefs->GetBoolean(gcm::prefs::kGCMChannelStatus);
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID)
GCMProfileService::GCMProfileService(
    base::FilePath path,
    scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  driver_.reset(new GCMDriverAndroid(path.Append(gcm_driver::kGCMStoreDirname),
                                     blocking_task_runner));
}
#else
GCMProfileService::GCMProfileService(
    PrefService* prefs,
    base::FilePath path,
    net::URLRequestContextGetter* request_context,
    version_info::Channel channel,
    const std::string& product_category_for_subtypes,
    std::unique_ptr<ProfileIdentityProvider> identity_provider,
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : profile_identity_provider_(std::move(identity_provider)),
      request_context_(request_context) {
  driver_ = CreateGCMDriverDesktop(
      std::move(gcm_client_factory), prefs,
      path.Append(gcm_driver::kGCMStoreDirname), request_context_, channel,
      product_category_for_subtypes, ui_task_runner, io_task_runner,
      blocking_task_runner);

  identity_observer_.reset(new IdentityObserver(
      profile_identity_provider_.get(), request_context_, driver_.get()));
}
#endif  // defined(OS_ANDROID)

GCMProfileService::GCMProfileService() {}

GCMProfileService::~GCMProfileService() {}

void GCMProfileService::Shutdown() {
#if !defined(OS_ANDROID)
  identity_observer_.reset();
#endif  // !defined(OS_ANDROID)
  if (driver_) {
    driver_->Shutdown();
    driver_.reset();
  }
}

void GCMProfileService::SetDriverForTesting(GCMDriver* driver) {
  driver_.reset(driver);
#if !defined(OS_ANDROID)
  if (identity_observer_) {
    identity_observer_.reset(new IdentityObserver(
        profile_identity_provider_.get(), request_context_, driver));
  }
#endif  // !defined(OS_ANDROID)
}

}  // namespace gcm
