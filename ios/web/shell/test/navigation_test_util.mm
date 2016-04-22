// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/test/navigation_test_util.h"

#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/shell/test/web_shell_test_util.h"

using web::NavigationManager;

namespace web {
namespace navigation_test_util {
// TODO(crbug.com/604902): Refactor these utilities into objects that can be
// shared accross the web shell and Chrome.

void LoadUrl(GURL url) {
  ViewController* view_controller =
      web::web_shell_test_util::GetCurrentViewController();

  NavigationManager* navigation_manager =
      [view_controller webState]->GetNavigationManager();
  NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  navigation_manager->LoadURLWithParams(params);
}

}  // namespace navigation_test_util
}  // namespace web