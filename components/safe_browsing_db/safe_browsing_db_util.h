// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing DB code.

#ifndef COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_DB_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_DB_UTIL_H_

#include "base/basictypes.h"
#include "base/strings/string_piece.h"

// A truncated hash's type.
typedef uint32 SBPrefix;

// A full hash.
union SBFullHash {
  char full_hash[32];
  SBPrefix prefix;
};

inline bool SBFullHashEqual(const SBFullHash& a, const SBFullHash& b) {
  return !memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash));
}

inline bool SBFullHashLess(const SBFullHash& a, const SBFullHash& b) {
  return memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash)) < 0;
}

// Generate full hash for the given string.
SBFullHash SBFullHashForString(const base::StringPiece& str);

// Different types of threats that SafeBrowsing protects against.
enum SBThreatType {
  // No threat at all.
  SB_THREAT_TYPE_SAFE,

  // The URL is being used for phishing.
  SB_THREAT_TYPE_URL_PHISHING,

  // The URL hosts malware.
  SB_THREAT_TYPE_URL_MALWARE,

  // The URL hosts unwanted programs.
  SB_THREAT_TYPE_URL_UNWANTED,

  // The download URL is malware.
  SB_THREAT_TYPE_BINARY_MALWARE_URL,

  // Url detected by the client-side phishing model.  Note that unlike the
  // above values, this does not correspond to a downloaded list.
  SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,

  // The Chrome extension or app (given by its ID) is malware.
  SB_THREAT_TYPE_EXTENSION,

  // Url detected by the client-side malware IP list. This IP list is part
  // of the client side detection model.
  SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,
};

#endif  // COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_DB_UTIL_H_
