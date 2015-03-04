// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

class GCMClientFactory;
class GCMDriver;

#if defined(OS_CHROMEOS)
class GCMConnectionObserver;
#endif

// Providing GCM service, via GCMDriver, to a profile.
class GCMProfileService : public KeyedService {
 public:
  // Returns whether GCM is enabled for |profile|.
  static bool IsGCMEnabled(Profile* profile);

#if defined(OS_ANDROID)
  explicit GCMProfileService(Profile* profile);
#else
  GCMProfileService(Profile* profile,
                    scoped_ptr<GCMClientFactory> gcm_client_factory);
#endif
  ~GCMProfileService() override;

  // KeyedService:
  void Shutdown() override;

  // For testing purpose.
  void SetDriverForTesting(GCMDriver* driver);

  GCMDriver* driver() const { return driver_.get(); }

 protected:
  // Used for constructing fake GCMProfileService for testing purpose.
  GCMProfileService();

 private:
  // The profile which owns this object.
  Profile* profile_;

  scoped_ptr<GCMDriver> driver_;

  // Used for both account tracker and GCM.UserSignedIn UMA.
#if !defined(OS_ANDROID)
  class IdentityObserver;
  scoped_ptr<IdentityObserver> identity_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
