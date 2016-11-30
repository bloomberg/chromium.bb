// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/navigation_test_util.h"

#import "ios/web/public/navigation_manager.h"

using web::NavigationManager;

namespace web {
namespace test {

void LoadUrl(web::WebState* web_state, const GURL& url) {
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  navigation_manager->LoadURLWithParams(params);
}

}  // namespace test
}  // namespace web
