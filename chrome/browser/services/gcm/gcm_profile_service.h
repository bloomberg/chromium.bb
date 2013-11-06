// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_

#include "base/basictypes.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

namespace gcm {

// Acts as a bridge between GCM API and GCMClient layer. It is profile based.
class GCMProfileService : public BrowserContextKeyedService {
 public:
  // Returns true if the GCM support is enabled.
  static bool IsGCMEnabled();

  explicit GCMProfileService(Profile* profile);
  virtual ~GCMProfileService();

 private:
  // The profile which owns this object.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileService);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_H_
