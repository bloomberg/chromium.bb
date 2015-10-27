// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/unverified_download_policy.h"

#include "base/files/file_path.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/safe_browsing/unverified_download_field_trial.h"
#include "chrome/common/safe_browsing/download_protection_util.h"

namespace safe_browsing {

bool IsUnverifiedDownloadAllowed(const base::FilePath& file) {
  // Currently, the policy is determined entirely by a field trial.
  bool is_allowed = IsUnverifiedDownloadAllowedByFieldTrial(file);
  int uma_file_type =
      download_protection_util::GetSBClientDownloadExtensionValueForUMA(file);
  if (is_allowed) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.UnverifiedDownloads.Allowed",
                                uma_file_type);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.UnverifiedDownloads.Blocked",
                                uma_file_type);
  }
  return is_allowed;
}

}  // namespace safe_browsing
