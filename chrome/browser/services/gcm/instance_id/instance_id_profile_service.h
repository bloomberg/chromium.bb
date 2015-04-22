// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_
#define CHROME_BROWSER_SERVICES_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace instance_id {

// Providing Instance ID support, via InstanceIDDriver, to a profile.
class InstanceIDProfileService : public KeyedService {
 public:
  explicit InstanceIDProfileService(Profile* profile);
  ~InstanceIDProfileService() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstanceIDProfileService);
};

}  // namespace instance_id

#endif  // CHROME_BROWSER_SERVICES_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_
