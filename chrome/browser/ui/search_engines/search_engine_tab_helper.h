// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/common/search_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SearchEngineTabHelperDelegate;
class TemplateURL;

// Per-tab search engine manager. Handles dealing search engine processing
// functionality.
class SearchEngineTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchEngineTabHelper> {
 public:
  virtual ~SearchEngineTabHelper();

  SearchEngineTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(SearchEngineTabHelperDelegate* d) { delegate_ = d; }

  // content::WebContentsObserver overrides.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  explicit SearchEngineTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SearchEngineTabHelper>;

  // Handles when a page specifies an OSDD (OpenSearch Description Document).
  void OnPageHasOSDD(const GURL& page_url,
                     const GURL& osdd_url,
                     const search_provider::OSDDType& msg_provider_type);

  // Handles when an OSDD is downloaded.
  void OnDownloadedOSDD(scoped_ptr<TemplateURL> template_url);

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const content::FrameNavigateParams& params);

  // Delegate for notifying our owner about stuff. Not owned by us.
  SearchEngineTabHelperDelegate* delegate_;

  base::WeakPtrFactory<SearchEngineTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
