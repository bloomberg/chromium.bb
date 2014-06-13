// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_SERVICE_IMPL_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_SERVICE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_client.h"
#include "content/public/browser/push_messaging_service.h"

namespace gcm {

class GCMProfileService;

class PushMessagingServiceImpl : public content::PushMessagingService {
 public:
  explicit PushMessagingServiceImpl(GCMProfileService* gcm_profile_service);
  virtual ~PushMessagingServiceImpl();

  // content::PushMessagingService implementation:
  virtual void Register(
      const std::string& app_id,
      const std::string& sender_id,
      const content::PushMessagingService::RegisterCallback& callback) OVERRIDE;

 private:
  void DidRegister(
      const content::PushMessagingService::RegisterCallback& callback,
      const std::string& registration_id,
      GCMClient::Result result);

  GCMProfileService* gcm_profile_service_;  // It owns us.

  base::WeakPtrFactory<PushMessagingServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingServiceImpl);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_SERVICE_IMPL_H_
