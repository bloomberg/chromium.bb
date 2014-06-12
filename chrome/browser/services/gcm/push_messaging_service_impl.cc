// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_service_impl.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "components/gcm_driver/gcm_driver.h"

namespace gcm {

PushMessagingServiceImpl::PushMessagingServiceImpl(
    GCMProfileService* gcm_profile_service)
    : gcm_profile_service_(gcm_profile_service),
      weak_factory_(this) {}

PushMessagingServiceImpl::~PushMessagingServiceImpl() {}

void PushMessagingServiceImpl::Register(
    const std::string& app_id,
    const std::string& sender_id,
    const content::PushMessagingService::RegisterCallback& callback) {
  // The GCMDriver could be NULL if GCMProfileService has been shut down.
  if (!gcm_profile_service_->driver())
    return;
  std::vector<std::string> sender_ids(1, sender_id);
  gcm_profile_service_->driver()->Register(
      app_id,
      sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void PushMessagingServiceImpl::DidRegister(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    GCMClient::Result result) {
  GURL endpoint = GURL("https://android.googleapis.com/gcm/send");
  bool error = (result != GCMClient::SUCCESS);
  callback.Run(endpoint, registration_id, error);
}

}  // namespace gcm
