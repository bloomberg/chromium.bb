// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_CHROME_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_TAB_CONTENTS_CHROME_INTERSTITIAL_PAGE_H_
#pragma once

#include <map>
#include <string>

#include "content/browser/tab_contents/interstitial_page.h"
#include "content/public/browser/notification_observer.h"
#include "googleurl/src/gurl.h"

class TabContents;

// This class adds the possibility to react to DOMResponse-messages sent by
// the RenderViewHost via ChromeRenderViewHostObserver to the InterstitialPage.
class ChromeInterstitialPage : public InterstitialPage {
 public:
  ChromeInterstitialPage(TabContents* tab,
                         bool new_navigation,
                         const GURL& url);
  virtual ~ChromeInterstitialPage();

  // Shows the interstitial page in the tab.
  virtual void Show() OVERRIDE;

 protected:
  // content::NotificationObserver method:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Invoked when the page sent a command through DOMAutomation.
  virtual void CommandReceived(const std::string& command) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeInterstitialPage);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_CHROME_INTERSTITIAL_PAGE_H_
