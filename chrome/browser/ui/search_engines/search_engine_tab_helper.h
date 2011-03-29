// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#pragma once

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/common/search_provider.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class SearchEngineTabHelperDelegate;
class TabContentsWrapper;

// Per-tab search engine manager. Handles dealing search engine processing
// functionality.
class SearchEngineTabHelper : public TabContentsObserver {
 public:
  explicit SearchEngineTabHelper(TabContents* tab_contents);
  virtual ~SearchEngineTabHelper();

  SearchEngineTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(SearchEngineTabHelperDelegate* d) { delegate_ = d; }

  // TabContentsObserver overrides.
  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Handles when a page specifies an OSDD (OpenSearch Description Document).
  void OnPageHasOSDD(int32 page_id,
                     const GURL& doc_url,
                     const search_provider::OSDDType& msg_provider_type);

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Delegate for notifying our owner about stuff. Not owned by us.
  SearchEngineTabHelperDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
