// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_

#include "base/basictypes.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

namespace hotword_internal {
// Constants for the hotword field trial.
extern const char kHotwordFieldTrialName[];
extern const char kHotwordFieldTrialDisabledGroupName[];
}  // namespace hotword_internal

// Provides an interface for the Hotword component that does voice triggered
// search.
class HotwordService : public BrowserContextKeyedService {
 public:
  // Returns true if the hotword supports the current system language.
  static bool DoesHotwordSupportLanguage(Profile* profile);

  explicit HotwordService(Profile* profile);
  virtual ~HotwordService();

  bool ShouldShowOptInPopup();

  // Used in testing to ensure that the popup is not shown more than this
  // maximum number of times.
  int MaxNumberTimesToShowOptInPopup();

  // In addition to showing the popup, the preferences
  // kHotwordOptInPopupTimesShown is also incremented.
  void ShowOptInPopup();

  // Checks for whether all the necessary files have downloaded to allow for
  // using the extension.
  virtual bool IsServiceAvailable();

  // Determine if hotwording is allowed in this profile based on field trials
  // and language.
  virtual bool IsHotwordAllowed();

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(HotwordService);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
