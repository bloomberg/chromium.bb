// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_PROFILE_SERVICE_H_
#define COMPONENTS_GCM_DRIVER_GCM_PROFILE_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/version_info/version_info.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

class GCMClientFactory;
class GCMDriver;

// Providing GCM service, via GCMDriver.
class GCMProfileService : public KeyedService {
 public:
#if defined(OS_ANDROID)
  GCMProfileService(
      base::FilePath path,
      scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);
#else
  GCMProfileService(
      PrefService* prefs,
      base::FilePath path,
      net::URLRequestContextGetter* request_context,
      version_info::Channel channel,
      const std::string& product_category_for_subtypes,
      std::unique_ptr<ProfileIdentityProvider> identity_provider,
      std::unique_ptr<GCMClientFactory> gcm_client_factory,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);
#endif
  ~GCMProfileService() override;

  // Returns whether GCM is enabled.
  static bool IsGCMEnabled(PrefService* prefs);

  // KeyedService:
  void Shutdown() override;

  // For testing purpose.
  void SetDriverForTesting(GCMDriver* driver);

  GCMDriver* driver() const { return driver_.get(); }

 protected:
  // Used for constructing fake GCMProfileService for testing purpose.
  GCMProfileService();

 private:
  std::unique_ptr<ProfileIdentityProvider> profile_identity_provider_;
  std::unique_ptr<GCMDriver> driver_;

#if !defined(OS_ANDROID)
  net::URLRequestContextGetter* request_context_ = nullptr;

  // Used for both account tracker and GCM.UserSignedIn UMA.
  class IdentityObserver;
  std::unique_ptr<IdentityObserver> identity_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_PROFILE_SERVICE_H_

