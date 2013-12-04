// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/search/instant_page.h"

class Profile;

// InstantTab represents a committed page (i.e. an actual tab on the tab strip)
// that supports the Instant API.
class InstantTab : public InstantPage {
 public:
  InstantTab(InstantPage::Delegate* delegate, Profile* profile);
  virtual ~InstantTab();

  // Start observing |contents| for messages. Sends a message to determine if
  // the page supports the Instant API.
  void Init(content::WebContents* contents);

  // Logs a number of statistics regarding the NTP.
  static void EmitNtpStatistics(content::WebContents* contents);

 private:
  // Overridden from InstantPage:
  virtual bool ShouldProcessAboutToNavigateMainFrame() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InstantTab);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_
