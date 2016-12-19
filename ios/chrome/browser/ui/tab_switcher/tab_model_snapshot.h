// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_MODEL_SNAPSHOT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_MODEL_SNAPSHOT_H_

#import <Foundation/Foundation.h>
#include <vector>

#import "base/ios/weak_nsobject.h"

@class TabModel;
@class Tab;

// Contains an approximate snapshot of the tab model passed to the initializer.
// Used to feed the cells to UICollectionViews, and to compute the changes
// required to update UICollectionViews.
// The changes are computed based on a list of hashes of the tabs.
// One resulting limitation is that there is no differenciation between a tab
// that is partially updated (because the snapshot changed), and a completely
// new tab replacing an older tab. This limitation has little impact because
// very few navigations occur when the tab switcher is shown.
class TabModelSnapshot {
 public:
  // Default constructor. |tabModel| can be nil.
  explicit TabModelSnapshot(TabModel* tabModel);
  ~TabModelSnapshot();
  // Returns the list of hashes for every tabs contained in the TabModel during
  // the initialization.
  std::vector<size_t> const& hashes() const;
  // Returns a list of weak pointers to the tabs.
  std::vector<base::WeakNSObject<Tab>> const& tabs() const;
  // Returns a hash of the properties of a tab that are visible in the tab
  // switcher's UI.
  static size_t hashOfTheVisiblePropertiesOfATab(Tab* tab);

 private:
  std::vector<base::WeakNSObject<Tab>> _tabs;
  std::vector<size_t> _hashes;
};

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_MODEL_SNAPSHOT_H_
