// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines methods relevant to all code that wants to work with browsing data.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HELPER_H_

#include <string>

#include "base/basictypes.h"

namespace storage {
class SpecialStoragePolicy;
}

class GURL;

class BrowsingDataHelper {
 public:
  enum OriginSetMask {
    UNPROTECTED_WEB = 1 << 0,  // drive-by web.
    PROTECTED_WEB = 1 << 1,    // hosted applications.
    EXTENSION = 1 << 2,        // chrome-extension://*
    // Always add new items to the enum above ALL and add them to ALL.
    ALL = UNPROTECTED_WEB | PROTECTED_WEB | EXTENSION,
  };

  // Returns true iff the provided scheme is (really) web safe, and suitable
  // for treatment as "browsing data". This relies on the definition of web safe
  // in ChildProcessSecurityPolicy, but excluding schemes like
  // `chrome-extension`.
  static bool IsWebScheme(const std::string& scheme);
  static bool HasWebScheme(const GURL& origin);

  // Returns true iff the provided scheme is an extension.
  static bool IsExtensionScheme(const std::string& scheme);
  static bool HasExtensionScheme(const GURL& origin);

  // Returns true if the provided origin matches the provided mask.
  static bool DoesOriginMatchMask(const GURL& origin,
                                  int origin_set_mask,
                                  storage::SpecialStoragePolicy* policy);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowsingDataHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_HELPER_H_
