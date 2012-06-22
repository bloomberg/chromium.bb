// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_DELEGATE_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/search/search_model_observer.h"

class TabContents;

namespace chrome {
namespace search {

class SearchModel;

// The SearchDelegate class acts as a helper to the Browser class.
// It is responsible for routing the changes from the active tab's
// SearchModel through to the toolbar, tabstrip and other UI
// observers.
// Changes are propagated from the active tab's model via this class to the
// Browser-level model.
class SearchDelegate : public SearchModelObserver {
 public:
  explicit SearchDelegate(SearchModel* model);
  virtual ~SearchDelegate();

  // Overrides for SearchModelObserver:
  virtual void ModeChanged(const Mode& mode) OVERRIDE;

  // When the active tab is changed, the model state of this new active tab is
  // propagated to the browser.
  void OnTabActivated(TabContents* contents);

  // When a tab is deactivated, this class no longer observes changes to the
  // tab's model.
  void OnTabDeactivated(TabContents* contents);

  // When a tab is dettached, this class no longer observes changes to the
  // tab's model.
  void OnTabDetached(TabContents* contents);

 private:
  // Stop observing tab.
  void StopObserveringTab(TabContents* contents);

  // Weak.  The Browser class owns this.  The active |tab_model_| state is
  // propagated to the |browser_model_|.
  SearchModel* browser_model_;

  // Weak.  The TabContents owns this.  It is the model of the active
  // tab.  Changes to this model are propagated through to the |browser_model_|.
  SearchModel* tab_model_;

  DISALLOW_COPY_AND_ASSIGN(SearchDelegate);
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_DELEGATE_H_
