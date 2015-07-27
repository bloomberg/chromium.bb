// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
#define CONTENT_COMMON_SITE_ISOLATION_POLICY_H_

#include "base/basictypes.h"
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
  // Returns true if the current process model might allow the use of cross-
  // process iframes. This should typically used to avoid executing codepaths
  // that only matter for cross-process iframes, to protect the default
  // behavior.
  //
  // Note: Since cross-process frames will soon be possible by default (e.g. for
  // <iframe src="http://..."> in an extension process), usage should be limited
  // to temporary stop-gaps.
  //
  // Instead of calling this method, prefer to examine object state to see
  // whether a particular frame happens to have a cross-process relationship
  // with another, or to consult DoesSiteRequireDedicatedProcess() to see if a
  // particular site merits protection.
  static bool AreCrossProcessFramesPossible();

  // Returns true if pages loaded from |url|'s site ought to be handled only by
  // a renderer process isolated from other sites. If --site-per-process is on
  // the command line, this is true for all sites.
  //
  // Eventually, this function will be made to return true for only some schemes
  // (e.g. extensions) or a whitelist of sites that we should protect for this
  // user.
  //
  // Although |url| is currently ignored, callers can assume for now that they
  // can pass a full URL here -- they needn't canonicalize it to a site.
  static bool DoesSiteRequireDedicatedProcess(const GURL& url);

  // Returns true if navigation and history code should maintain per-frame
  // navigation entries. This is an in-progress feature related to site
  // isolation, so the return value is currently tied to --site-per-process.
  // TODO(creis, avi): Make this the default, and eliminate this.
  static bool UseSubframeNavigationEntries();

 private:
  SiteIsolationPolicy();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SITE_ISOLATION_POLICY_H_
