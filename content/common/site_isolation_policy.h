// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
#define CONTENT_COMMON_SITE_ISOLATION_POLICY_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// A centralized place for making policy decisions about out-of-process iframes,
// site isolation, --site-per-process, and related features.
//
// This is currently static because all these modes are controlled by command-
// line flags or field trials.
//
// These methods can be called from any thread.
class CONTENT_EXPORT SiteIsolationPolicy {
 public:
  // Returns true if every site should be placed in a dedicated process.
  static bool UseDedicatedProcessesForAllSites();

  // Returns true if third-party subframes of a page should be kept in a
  // different process from the main frame.
  static bool IsTopDocumentIsolationEnabled();

  // Returns true if isolated origins feature is enabled.
  static bool AreIsolatedOriginsEnabled();

  // Returns the origins to isolate.  See also AreIsolatedOriginsEnabled.
  // This list applies globally to the whole browser in all profiles.
  // TODO(lukasza): Make sure this list also includes the origins returned by
  // ContentBrowserClient::GetOriginsRequiringDedicatedProcess.
  static std::vector<url::Origin> GetIsolatedOrigins();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  FRIEND_TEST_ALL_PREFIXES(SiteIsolationPolicyTest, ParseIsolatedOrigins);
  static std::vector<url::Origin> ParseIsolatedOrigins(base::StringPiece arg);

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
