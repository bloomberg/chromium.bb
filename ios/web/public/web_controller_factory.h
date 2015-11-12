// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_CONTROLLER_FACTORY_H_
#define IOS_WEB_PUBLIC_WEB_CONTROLLER_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "ios/web/public/web_view_type.h"

@class CRWWebController;

namespace web {

class BrowserState;
class WebStateImpl;

// Returns a new instance of CRWWebViewController.
// Note: Callers are responsible for releasing the returned web controller.
CRWWebController* CreateWebController(WebViewType web_view_type,
                                      scoped_ptr<WebStateImpl> web_state);

// Returns a new instance of CRWWebViewController.
// Temporary factory method for use in components that require a web controller.
// By requiring only the BrowserState, this eliminates the dependency on
// WebStateImpl from components.
// Note: Callers are responsible for releasing the returned web controller.
// TODO(crbug.com/546221): Move factory method to WebState once the ownership of
// WebState and CRWWebController is reversed.
CRWWebController* CreateWebController(WebViewType web_view_type,
                                      BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_CONTROLLER_FACTORY_H_
