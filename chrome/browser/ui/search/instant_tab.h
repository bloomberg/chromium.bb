// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class WebContents;
}

// InstantTab is instantiated by InstantController for the current tab if it
// is a New Tab page.
class InstantTab : public content::WebContentsObserver {
 public:
  // InstantTab calls its delegate in response to messages received from the
  // page. Each method is called with the |contents| corresponding to the page
  // we are observing.
  class Delegate {
   public:
    // Called when the page is about to navigate to |url|.
    virtual void InstantTabAboutToNavigateMainFrame(
        const content::WebContents* contents,
        const GURL& url) = 0;

   protected:
    virtual ~Delegate();
  };

  InstantTab(Delegate* delegate, content::WebContents* web_contents);
  ~InstantTab() override;

  // Sets up communication with the WebContents passed to the constructor. Does
  // nothing if that WebContents was null. Should not be called more than once.
  void Init();

 private:
  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  Delegate* const delegate_;

  content::WebContents* pending_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(InstantTab);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_
