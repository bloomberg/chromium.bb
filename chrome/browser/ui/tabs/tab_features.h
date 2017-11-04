// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_TAB_FEATURES_H_

#include <memory>

#include "base/feature_list.h"

class TabStripModel;
class TabStripModelDelegate;
class Profile;

// Enables the tab controller for experimenting with different behavior.
extern const base::Feature kExperimentalTabControllerFeature;

bool IsExperimentalTabStripEnabled();

// Creates and returns the correct TabStripModel according to the feature flag.
std::unique_ptr<TabStripModel> CreateTabStripModel(
    TabStripModelDelegate* delegate,
    Profile* profile);

#endif  // CHROME_BROWSER_UI_TABS_TAB_FEATURES_H_
