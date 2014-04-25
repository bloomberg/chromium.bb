// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/services/gcm/gcm_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

// A specialization of GCMService that is tied to a Profile.
class GCMProfileService : public GCMService, public KeyedService {
 public:
  // Any change made to this enum should have corresponding change in the
  // GetGCMEnabledStateString(...) function.
  enum GCMEnabledState {
    // GCM is always enabled. GCMClient will always load and connect with GCM.
    ALWAYS_ENABLED,
    // GCM is only enabled for apps. GCMClient will start to load and connect
    // with GCM only when GCM API is used.
    ENABLED_FOR_APPS,
    // GCM is always disabled. GCMClient will never load and connect with GCM.
    ALWAYS_DISABLED
  };

  // Returns the GCM enabled state.
  static GCMEnabledState GetGCMEnabledState(Profile* profile);

  // Returns text representation of a GCMEnabledState enum entry.
  static std::string GetGCMEnabledStateString(GCMEnabledState state);

  // Register profile-specific prefs for GCM.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit GCMProfileService(Profile* profile);
  virtual ~GCMProfileService();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // Returns the user name if the profile is signed in.
  std::string SignedInUserName() const;

 protected:
  // Overridden from GCMService:
  virtual bool ShouldStartAutomatically() const OVERRIDE;
  virtual base::FilePath GetStorePath() const OVERRIDE;
  virtual scoped_refptr<net::URLRequestContextGetter>
      GetURLRequestContextGetter() const OVERRIDE;

 private:
  // The profile which owns this object.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
