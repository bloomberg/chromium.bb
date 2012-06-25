// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TYPES_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TYPES_H_
#pragma once

namespace chrome {
namespace search {

// The Mode structure encodes the visual states encountered when interacting
// with the NTP and the Omnibox.  State changes can be animated depending on the
// context.
struct Mode {
  enum Type {
    // The default state means anything but the following states.
    MODE_DEFAULT,

    // On the NTP page and the NTP is ready to be displayed.
    MODE_NTP,

    // Any of the following:
    // . on the NTP page and the Omnibox is modified in some way.
    // . on a search results page.
    // . the Omnibox has focus.
    MODE_SEARCH,
  };

  Mode() : mode(MODE_DEFAULT), animate(false) {
  }

  Mode(Type in_mode, bool in_animate) : mode(in_mode), animate(in_animate) {
  }

  bool operator==(const Mode& rhs) const {
    return mode == rhs.mode;
    // |animate| is transient.  It doesn't count.
  }

  bool is_default() const {
    return mode == MODE_DEFAULT;
  }

  bool is_ntp() const {
    return mode == MODE_NTP;
  }

  bool is_search() const {
    return mode == MODE_SEARCH;
  }

  Type mode;

  // Mode changes can be animated.  This is transient state, once a call to
  // |SearchModel::SetMode| has completed and its observers notified |animate|
  // is set to |false|.
  bool animate;
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TYPES_H_
