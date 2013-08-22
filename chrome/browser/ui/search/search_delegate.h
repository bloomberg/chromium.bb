// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_DELEGATE_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/search/search_model_observer.h"

namespace content {
class WebContents;
}

class SearchModel;

// The SearchDelegate class acts as a helper to the Browser class.
// It is responsible for routing the changes from the active tab's
// SearchModel through to the toolbar, tabstrip and other UI
// observers.
// Changes are propagated from the active tab's model via this class to the
// Browser-level model.
class SearchDelegate : public SearchModelObserver {
 public:
  explicit SearchDelegate(SearchModel* browser_search_model);
  virtual ~SearchDelegate();

  // Overrides for SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // When the active tab is changed, the model state of this new active tab is
  // propagated to the browser.
  void OnTabActivated(content::WebContents* web_contents);

  // When a tab is deactivated, this class no longer observes changes to the
  // tab's model.
  void OnTabDeactivated(content::WebContents* web_contents);

  // When a tab is detached, this class no longer observes changes to the
  // tab's model.
  void OnTabDetached(content::WebContents* web_contents);

 private:
  // Stop observing tab.
  void StopObservingTab(content::WebContents* web_contents);

  // Weak.  The Browser class owns this.  The active |tab_model_| state is
  // propagated to the |browser_model_|.
  SearchModel* browser_model_;

  // Weak.  The WebContents owns this.  It is the model of the active
  // tab.  Changes to this model are propagated through to the |browser_model_|.
  SearchModel* tab_model_;

  DISALLOW_COPY_AND_ASSIGN(SearchDelegate);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_DELEGATE_H_
