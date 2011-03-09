// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_SEARCH_HINT_H_
#define CHROME_BROWSER_OMNIBOX_SEARCH_HINT_H_
#pragma once

#include <map>
#include <string>

#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;
class TabContents;

// This class is responsible for showing an info-bar that tells the user she
// can type her search query directly in the omnibox.
// It is displayed when the user visits a known search engine URL and has not
// searched from the omnibox before, or has not previously dismissed a similar
// info-bar.
class OmniboxSearchHint : public NotificationObserver {
 public:
  explicit OmniboxSearchHint(TabContents* tab);
  ~OmniboxSearchHint();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Focus the location bar and displays a message instructing that search
  // queries can be typed directly in there.
  void ShowEnteringQuery();

  TabContents* tab() { return tab_; }

  // Disables the hint infobar permanently, so that it does not show ever again.
  void DisableHint();

  // Returns true if the profile and current environment make the showing of the
  // hint infobar possible.
  static bool IsEnabled(Profile* profile);

 private:
  void ShowInfoBar();

  NotificationRegistrar notification_registrar_;

  // The tab we are associated with.
  TabContents* tab_;

  // A map containing the URLs of the search engine for which we want to
  // trigger the hint.
  std::map<std::string, int> search_engine_urls_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxSearchHint);
};

#endif  // CHROME_BROWSER_OMNIBOX_SEARCH_HINT_H_
