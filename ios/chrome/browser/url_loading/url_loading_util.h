// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_

#import <Foundation/Foundation.h>

class GURL;
namespace ios {
class ChromeBrowserState;
}
namespace web {
class WebState;
}

// Loads |url| in |webState|, performing any necessary updates to
// |browserState|. It is an error to pass a value of GURL that doesn't have a
// javascript: scheme.
void LoadJavaScriptURL(const GURL& url,
                       ios::ChromeBrowserState* browserState,
                       web::WebState* webState);

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_
