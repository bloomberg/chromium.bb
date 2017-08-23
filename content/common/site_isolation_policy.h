// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
#define CONTENT_COMMON_SITE_ISOLATION_POLICY_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// A centralized place for making policy decisions about out-of-process iframes,
// site isolation, --site-per-process, and related features.
//
// This is currently static because all these modes are controlled by command-
// line flags.
//
// These methods can be called from any thread.
class CONTENT_EXPORT SiteIsolationPolicy {
 public:
  // Returns true if every site should be placed in a dedicated process.
  static bool UseDedicatedProcessesForAllSites();

  // Returns true if third-party subframes of a page should be kept in a
  // different process from the main frame.
  static bool IsTopDocumentIsolationEnabled();

  // Returns true if there exist origins that require process isolation.  Such
  // origins require a dedicated process, and hence they make cross-process
  // iframes possible.
  static bool AreIsolatedOriginsEnabled();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
