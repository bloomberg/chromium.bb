// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_REPORTING_PERMISSIONS_CHECKER_H_
#define CHROME_BROWSER_NET_REPORTING_PERMISSIONS_CHECKER_H_

#include <set>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace url {
class Origin;
}

// Used by the Reporting API to check whether the user has allowed reports to be
// uploaded for particular origins, via the BACKGROUND_SYNC permission.
class ReportingPermissionsChecker {
 public:
  // Does not take ownership of |profile|, which must outlive this instance.
  explicit ReportingPermissionsChecker(Profile* profile);

  ~ReportingPermissionsChecker();

  // Checks whether each origin in |origins| has the BACKGROUND_SYNC permission
  // set, removing any that don't.  Call this from the IO thread.  The filter
  // will perform the check on the IO thread, and invoke |result_callback| back
  // on the IO thread with the result.
  void FilterReportingOrigins(
      std::set<url::Origin> origins,
      base::OnceCallback<void(std::set<url::Origin>)> result_callback) const;

 private:
  void FilterReportingOriginsInUIThread(
      std::set<url::Origin> origins,
      base::OnceCallback<void(std::set<url::Origin>)> result_callback) const;

  Profile* profile_;
  mutable base::WeakPtrFactory<const ReportingPermissionsChecker>
      const_weak_factory_;
};

#endif  // CHROME_BROWSER_NET_REPORTING_PERMISSIONS_CHECKER_H_
