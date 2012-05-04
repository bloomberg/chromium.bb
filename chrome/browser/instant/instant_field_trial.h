// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
#define CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_

#include <string>

#include "base/basictypes.h"

class Profile;

// This class manages the Instant field trial, which is in one of these modes:
//
// INSTANT: The default search engine is preloaded when the omnibox gets focus.
//     Queries are issued as the user types. Predicted queries are inline
//     autocompleted into the omnibox. Result previews are shown.
//
// SUGGEST: Same as INSTANT, without the previews.
//
// HIDDEN: Same as SUGGEST, without the inline autocompletion.
//
// SILENT: Same as HIDDEN, without issuing queries as the user types. The query
//     is sent only after the user presses <Enter>.
//
// CONTROL: Instant is disabled.
class InstantFieldTrial {
 public:
  enum Mode {
    INSTANT,
    SUGGEST,
    HIDDEN,
    SILENT,
    CONTROL
  };

  // Activate the field trial and choose a mode at random. Without this, most
  // calls to GetMode() will return CONTROL. See GetMode() for more details.
  // *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void Activate();

  // Return the field trial mode for this profile. This will usually be the
  // mode selected by Activate(), but can sometimes be different, depending on
  // the profile. See the implementation in the .cc file for specific details.
  static Mode GetMode(Profile* profile);

  // Return a string describing the mode. Can be added to histogram names, to
  // split histograms by modes.
  static std::string GetModeAsString(Profile* profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstantFieldTrial);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
