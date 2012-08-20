// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_INFOBARS_INFOBAR_TAB_SERVICE_H_
#define CHROME_BROWSER_API_INFOBARS_INFOBAR_TAB_SERVICE_H_

namespace content {
class WebContents;
}

class InfoBarDelegate;
class TabContents;

// Provides access to creating, removing and enumerating info bars
// attached to a tab.
class InfoBarTabService {
 public:
  // Retrieves the InfoBarTabService for a given tab.
  static InfoBarTabService* ForTab(TabContents* tab_contents);

  virtual ~InfoBarTabService() {}

  // Adds an InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab or the tab already has a delegate
  // which returns true for InfoBarDelegate::EqualsDelegate(delegate),
  // |delegate| is closed immediately without being added.
  //
  // Returns whether |delegate| was successfully added.
  virtual bool AddInfoBar(InfoBarDelegate* delegate) = 0;

  // Removes the InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab, this will do nothing, on the
  // assumption that the matching AddInfoBar() call will have already closed the
  // delegate (see above).
  virtual void RemoveInfoBar(InfoBarDelegate* delegate) = 0;

  // Replaces one infobar with another, without any animation in between.
  //
  // If infobars are disabled for this tab, |new_delegate| is closed immediately
  // without being added, and nothing else happens.
  //
  // Returns whether |new_delegate| was successfully added.
  //
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  virtual bool ReplaceInfoBar(InfoBarDelegate* old_delegate,
                              InfoBarDelegate* new_delegate) = 0;

  // Returns the number of infobars for this tab.
  virtual size_t GetInfoBarCount() const = 0;

  // Returns the infobar at the given |index|.
  //
  // Warning: Does not sanity check |index|.
  virtual InfoBarDelegate* GetInfoBarDelegateAt(size_t index) = 0;

  // Retrieve the WebContents for the tab this service is associated with.
  virtual content::WebContents* GetWebContents() = 0;
};

#endif  // CHROME_BROWSER_API_INFOBARS_INFOBAR_TAB_SERVICE_H_
