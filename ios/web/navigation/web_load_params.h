// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WEB_LOAD_PARAMS_H_
#define IOS_WEB_NAVIGATION_WEB_LOAD_PARAMS_H_

#import "ios/web/public/navigation_manager.h"

namespace web {

// TODO(crbug.com/597990) Remove this alias once all clients are switched to
// use web::NavigationManager::WebLoadParams.
using WebLoadParams = web::NavigationManager::WebLoadParams;

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WEB_LOAD_PARAMS_H_
