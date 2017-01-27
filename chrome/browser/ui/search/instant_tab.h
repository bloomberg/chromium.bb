// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {
class WebContents;
}

// InstantTab is used to exchange messages with a page that implements the
// Instant/Embedded Search API (http://dev.chromium.org/embeddedsearch).
class InstantTab : public content::WebContentsObserver,
                   public SearchModelObserver {
 public:
  // InstantTab calls its delegate in response to messages received from the
  // page. Each method is called with the |contents| corresponding to the page
  // we are observing.
  class Delegate {
   public:
    // Called upon determination of Instant API support. Either in response to
    // the page loading or because we received some other message.
    virtual void InstantSupportDetermined(const content::WebContents* contents,
                                          bool supports_instant) = 0;

    // Called when the page is about to navigate to |url|.
    virtual void InstantTabAboutToNavigateMainFrame(
        const content::WebContents* contents,
        const GURL& url) = 0;

   protected:
    virtual ~Delegate();
  };

  explicit InstantTab(Delegate* delegate, content::WebContents* web_contents);

  ~InstantTab() override;

  // Sets up communication with the WebContents passed to the constructor. Does
  // nothing if that WebContents was null. Should not be called more than once.
  void Init();

 private:
  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Overridden from SearchModelObserver:
  void ModelChanged(const SearchModel::State& old_state,
                    const SearchModel::State& new_state) override;

  void InstantSupportDetermined(bool supports_instant);

  Delegate* const delegate_;

  content::WebContents* pending_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(InstantTab);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_TAB_H_
