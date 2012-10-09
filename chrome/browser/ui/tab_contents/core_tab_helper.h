// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class CoreTabHelperDelegate;

// Per-tab class to handle functionality that is core to the operation of tabs.
class CoreTabHelper : public content::WebContentsObserver,
                      public content::WebContentsUserData<CoreTabHelper> {
 public:
  virtual ~CoreTabHelper();

  CoreTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(CoreTabHelperDelegate* d) { delegate_ = d; }

  // Initial title assigned to NavigationEntries from Navigate.
  static string16 GetDefaultTitle();

  // Returns a human-readable description the tab's loading state.
  string16 GetStatusText() const;

 private:
  explicit CoreTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<CoreTabHelper>;

  // content::WebContentsObserver overrides:
  virtual void WasShown() OVERRIDE;

  // Delegate for notifying our owner about stuff. Not owned by us.
  CoreTabHelperDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CoreTabHelper);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
