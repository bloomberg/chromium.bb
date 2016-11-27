// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Per-tab search engine manager. Handles dealing search engine processing
// functionality.
class SearchEngineTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchEngineTabHelper> {
 public:
  ~SearchEngineTabHelper() override;

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  bool OnMessageReceived(const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* rfh) override;

 private:
  explicit SearchEngineTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SearchEngineTabHelper>;

  // Handles when a page specifies an OSDD (OpenSearch Description Document).
  void OnPageHasOSDD(const GURL& page_url, const GURL& osdd_url);

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(content::NavigationHandle* handle);

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
