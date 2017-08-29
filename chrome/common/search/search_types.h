// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_SEARCH_TYPES_H_
#define CHROME_COMMON_SEARCH_SEARCH_TYPES_H_

// The Mode structure encodes the visual states encountered when interacting
// with the NTP and the Omnibox.
// TODO(treib): Replace this struct by just the enum Origin. crbug.com/627747
struct SearchMode {
  // The kind of page from which the user initiated the current search.
  enum Origin {
    // The user is searching from some random page.
    ORIGIN_DEFAULT = 0,

    // The user is searching from the NTP.
    ORIGIN_NTP,
  };

  SearchMode() : origin(ORIGIN_DEFAULT) {}

  explicit SearchMode(Origin in_origin) : origin(in_origin) {}

  bool operator==(const SearchMode& rhs) const { return origin == rhs.origin; }

  bool operator!=(const SearchMode& rhs) const {
    return !(*this == rhs);
  }

  bool is_origin_default() const {
    return origin == ORIGIN_DEFAULT;
  }

  bool is_origin_ntp() const {
    return origin == ORIGIN_NTP;
  }

  Origin origin;
};

#endif  // CHROME_COMMON_SEARCH_SEARCH_TYPES_H_
