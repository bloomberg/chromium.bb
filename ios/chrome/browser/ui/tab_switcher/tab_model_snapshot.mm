// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_model_snapshot.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/tabs/tab.h"

TabModelSnapshot::TabModelSnapshot(TabModel* tabModel) {
  for (Tab* tab in tabModel) {
    _hashes.push_back(hashOfTheVisiblePropertiesOfATab(tab));
    _tabs.push_back(base::WeakNSObject<Tab>(tab));
  }
}

TabModelSnapshot::~TabModelSnapshot() {}

std::vector<size_t> const& TabModelSnapshot::hashes() const {
  DCHECK_EQ(_tabs.size(), _hashes.size());
  return _hashes;
}

std::vector<base::WeakNSObject<Tab>> const& TabModelSnapshot::tabs() const {
  DCHECK_EQ(_tabs.size(), _hashes.size());
  return _tabs;
}

size_t TabModelSnapshot::hashOfTheVisiblePropertiesOfATab(Tab* tab) {
  DCHECK(tab);
  std::stringstream ss;
  // lastVisitedTimestamp is used as an approximation for whether the tab's
  // snapshot changed.
  ss << tab.tabId << std::endl
     << base::SysNSStringToUTF8(tab.urlDisplayString) << std::endl
     << std::hexfloat << tab.lastVisitedTimestamp << std::endl;
  return std::hash<std::string>()(ss.str());
}
