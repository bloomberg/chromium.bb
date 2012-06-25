// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class OmniboxEditModel;
class TabContents;

namespace chrome {
namespace search {

class SearchModel;

// Per-tab search "helper".  Acts as the owner and controller of the tab's
// search UI model.
class SearchTabHelper : public content::WebContentsObserver,
                        public content::NotificationObserver {
 public:
  SearchTabHelper(TabContents* contents, bool is_search_enabled);
  virtual ~SearchTabHelper();

  SearchModel* model() const {
    return model_.get();
  }

  // Invoked when the OmniboxEditModel changes state in some way that might
  // affect the search mode.
  void OmniboxEditModelChanged(OmniboxEditModel* edit_model);

  // content::WebContentsObserver overrides:
  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Sets the mode of the model based on |url|.
  void UpdateModel(const GURL& url);

  const bool is_search_enabled_;

  // Model object for UI that cares about search state.
  scoped_ptr<SearchModel> model_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SearchTabHelper);
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
