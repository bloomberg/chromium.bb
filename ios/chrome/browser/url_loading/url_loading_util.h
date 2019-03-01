// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_

#import <Foundation/Foundation.h>

#include "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ui/chrome_load_params.h"
#include "ui/base/window_open_disposition.h"

class GURL;
namespace ios {
class ChromeBrowserState;
}
@protocol SessionWindowRestoring;
namespace web {
class WebState;
}

// true if |url| can be loaded in an incognito tab.
bool IsURLAllowedInIncognito(const GURL& url);

// Loads |url| in |web_state|, performing any necessary updates to
// |browser_state|. It is an error to pass a value of GURL that doesn't have a
// javascript: scheme.
void LoadJavaScriptURL(const GURL& url,
                       ios::ChromeBrowserState* browser_state,
                       web::WebState* web_state);

// Restores the closed tab identified by |session_id|, using |disposition|,
// into |browser_state|.
void RestoreTab(const SessionID session_id,
                WindowOpenDisposition disposition,
                ios::ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_
