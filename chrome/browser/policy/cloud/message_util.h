// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_MESSAGE_UTIL_H_
#define CHROME_BROWSER_POLICY_CLOUD_MESSAGE_UTIL_H_

#include "base/string16.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/cloud/cloud_policy_validator.h"

namespace policy {

// Returns a string describing |status| suitable for display in UI.
string16 FormatDeviceManagementStatus(DeviceManagementStatus status);

// Returns a string describing |validation_status| suitable for display in UI.
string16 FormatValidationStatus(
    CloudPolicyValidatorBase::Status validation_status);

// Returns a textual description of |store_status| for display in the UI. If
// |store_status| is STATUS_VALIDATION_FAILED, |validation_status| will be
// consulted to create a description of the validation failure.
string16 FormatStoreStatus(CloudPolicyStore::Status store_status,
                           CloudPolicyValidatorBase::Status validation_status);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_MESSAGE_UTIL_H_
