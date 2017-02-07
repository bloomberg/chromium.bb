// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_info_fetcher.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace enterprise_management {
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace policy {
class DeviceManagementRequestJob;
class DMTokenStorage;
}  // namespace policy

namespace arc {

// This class is responsible to fetch an enrollment token for a new managed
// Google Play account when using ARC with Active Directory.
class ArcActiveDirectoryEnrollmentTokenFetcher : public ArcAuthInfoFetcher {
 public:
  ArcActiveDirectoryEnrollmentTokenFetcher();
  ~ArcActiveDirectoryEnrollmentTokenFetcher() override;

  // ArcAuthInfoFetcher:
  void Fetch(const FetchCallback& callback) override;

 private:
  void OnDMTokenAvailable(const std::string& dm_token);
  void OnFetchEnrollmentTokenCompleted(
      policy::DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  std::unique_ptr<policy::DeviceManagementRequestJob> fetch_request_job_;
  std::unique_ptr<policy::DMTokenStorage> dm_token_storage_;
  FetchCallback callback_;

  base::WeakPtrFactory<ArcActiveDirectoryEnrollmentTokenFetcher>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcActiveDirectoryEnrollmentTokenFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_
