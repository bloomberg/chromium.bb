// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_SEARCH_HINT_H_
#define CHROME_BROWSER_OMNIBOX_SEARCH_HINT_H_

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace content {
class WebContents;
}

// This class is responsible for showing an info-bar that tells the user she
// can type her search query directly in the omnibox.
// It is displayed when the user visits a known search engine URL and has not
// searched from the omnibox before, or has not previously dismissed a similar
// info-bar.
class OmniboxSearchHint
    : public content::NotificationObserver,
      public content::WebContentsUserData<OmniboxSearchHint> {
 public:
  virtual ~OmniboxSearchHint();

  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Focus the location bar and displays a message instructing that search
  // queries can be typed directly in there.
  void ShowEnteringQuery();

  // Disables the hint infobar permanently, so that it does not show ever again.
  void DisableHint();

  // Returns true if the profile and current environment make the showing of the
  // hint infobar possible.
  static bool IsEnabled(Profile* profile);

 private:
  explicit OmniboxSearchHint(content::WebContents* web_contents);
  friend class content::WebContentsUserData<OmniboxSearchHint>;

  content::NotificationRegistrar notification_registrar_;

  // The contents we are associated with.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxSearchHint);
};

#endif  // CHROME_BROWSER_OMNIBOX_SEARCH_HINT_H_
