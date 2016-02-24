// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_controller_factory.h"

#include <utility>

#include "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"
#include "ios/web/web_state/web_state_impl.h"

namespace web {

CRWWebController* CreateWebController(scoped_ptr<WebStateImpl> web_state) {
  return
      [[CRWWKWebViewWebController alloc] initWithWebState:std::move(web_state)];
}

CRWWebController* CreateWebController(BrowserState* browser_state) {
  DCHECK(browser_state);
  scoped_ptr<web::WebStateImpl> web_state(new web::WebStateImpl(browser_state));
  web_state->GetNavigationManagerImpl().InitializeSession(nil, nil, NO, -1);
  return CreateWebController(std::move(web_state));
}

}  // namespace web
