// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_

#import "ios/chrome/browser/main/browser.h"

#include "base/gtest_prod_util.h"
#include "base/macros.h"

@class TabModel;
class WebStateList;

namespace ios {
class ChromeBrowserState;
}

// Browser is the model for a window containing multiple tabs. Instances
// are owned by a BrowserList to allow multiple windows for a single user
// session.
//
// See src/docs/ios/objects.md for more information.
class BrowserImpl : public Browser {
 public:
  // Constructs a BrowserImpl attached to |browser_state|.
  explicit BrowserImpl(ios::ChromeBrowserState* browser_state);
  ~BrowserImpl() override;

  // Browser.
  ios::ChromeBrowserState* GetBrowserState() const override;
  TabModel* GetTabModel() const override;
  WebStateList* GetWebStateList() const override;

 private:
  // Exposed to allow unittests to pass in a mock TabModel.
  FRIEND_TEST_ALL_PREFIXES(BrowserImplTest, TestAccessors);
  BrowserImpl(ios::ChromeBrowserState* browser_state, TabModel* tab_model);

  ios::ChromeBrowserState* browser_state_;
  __strong TabModel* tab_model_;
  WebStateList* web_state_list_;

  DISALLOW_COPY_AND_ASSIGN(BrowserImpl);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
