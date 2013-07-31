// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/policy/cloud/cloud_policy_core.h"
#include "chrome/browser/policy/cloud/cloud_policy_manager.h"
#include "content/public/browser/notification_source.h"

namespace policy {

UserCloudPolicyInvalidator::UserCloudPolicyInvalidator(
    Profile* profile,
    CloudPolicyManager* policy_manager)
    : CloudPolicyInvalidator(
          policy_manager,
          policy_manager->core()->store(),
          base::MessageLoopProxy::current()),
      profile_(profile),
      policy_manager_(policy_manager) {
  DCHECK(profile);

  // Register for notification that profile creation is complete.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

void UserCloudPolicyInvalidator::Shutdown() {
  CloudPolicyInvalidator::Shutdown();
}

void UserCloudPolicyInvalidator::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Enable invalidations now that profile creation is complete.
  DCHECK(type == chrome::NOTIFICATION_PROFILE_ADDED);
  policy_manager_->EnableInvalidations(
      base::Bind(
          &CloudPolicyInvalidator::InitializeWithProfile,
          GetWeakPtr(),
          base::Unretained(profile_)));
}

}  // namespace policy
