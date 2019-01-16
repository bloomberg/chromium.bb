// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_

#import "ios/chrome/browser/main/browser.h"

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
  // Constructs a BrowserImpl attached to |browser_state|. The |tab_model| must
  // be non-nil.
  BrowserImpl(ios::ChromeBrowserState* browser_state, TabModel* tab_model);
  ~BrowserImpl() override;

  // Accessor for the owning ChromeBrowserState.
  ios::ChromeBrowserState* GetBrowserState() const override;

  // Accessor for the TabModel. DEPRECATED: prefer web_state_list() whenever
  // possible.
  TabModel* GetTabModel() const override;

  // Accessor for the WebStateList.
  WebStateList* GetWebStateList() const override;

 private:
  ios::ChromeBrowserState* browser_state_;
  __strong TabModel* tab_model_;
  WebStateList* web_state_list_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserImpl);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
