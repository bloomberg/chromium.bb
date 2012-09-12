// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TYPES_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TYPES_H_

namespace chrome {
namespace search {

// The Mode structure encodes the visual states encountered when interacting
// with the NTP and the Omnibox.  State changes can be animated depending on the
// context.
//
// *** Legend ***
// |--------------+------------------------------------------------|
// | Abbreviation | User Action                                    |
// |--------------+------------------------------------------------|
// | S            | Start.  First navigation to NTP                |
// | N            | Navigation: Back/Forward/Direct                |
// | T            | Tab switch                                     |
// | I            | User input in Omnibox                          |
// | D            | Defocus the Omnibox                            |
// | E            | Escape out of Omnibox with suggestions showing |
// | C            | Choose a suggestion from suggestion list       |
//
//
// *** Transitions ***
// |----------+---------+-----+---------+-----+----------+-----+---------+-----|
// |          | To      |     |         |     |          |     |         |     |
// |----------+---------+-----+---------+-----+----------+-----+---------+-----|
// |          | NTP     |     | Search* |     | Search** |     | Default |     |
// |----------+---------+-----+---------+-----+----------+-----+---------+-----|
// | From     | Animate | Pop | Animate | Pop | Animate  | Pop | Animate | Pop |
// |----------+---------+-----+---------+-----+----------+-----+---------+-----|
// | Start    |         | S   | -       | -   | -        | -   | -       | -   |
// | NTP      | -       | -   | I       |     | N        | T   | N       | T   |
// | Search*  | N D E   | T   | -       | -   | D E C    | T   | N D E C | T   |
// | Search** | N       | T   | I       |     | -        | -   | N D E C | T   |
// | Default  | N       | T   | I       |     | N        | T   | -       | -   |
//
// * Search with suggestions showing.
// ** Search without suggestions showing.
struct Mode {
  enum Type {
    // The default state means anything but the following states.
    MODE_DEFAULT,

    // On the NTP page but the NTP web contents are not yet rendered.
    MODE_NTP_LOADING,

    // On the NTP page and the NTP is ready to be displayed.
    MODE_NTP,

    // On the NTP page and the Omnibox is modified in some way.
    MODE_SEARCH_SUGGESTIONS,

    // On a search results page.
    MODE_SEARCH_RESULTS,
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
    return mode == MODE_NTP || mode == MODE_NTP_LOADING;
  }

  bool is_search() const {
    return mode == MODE_SEARCH_SUGGESTIONS || mode == MODE_SEARCH_RESULTS;
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
