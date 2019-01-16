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
class WebStateList;

// Possible results from calling LoadURL().
enum class URLLoadResult {
  // No load performed, calling code should switch to an existing tab with the
  // URL already loaded instead.
  SWITCH_TO_TAB,
  // A crash was intentionally induced as part of the URL load (typically this
  // means the URL was an 'induce crash' URL). Technically this result is
  // unlikely to actually be returned, but it identifies a specific result for
  // the LoadURL() call.
  INDUCED_CRASH,
  // The URL to be loaded was already prerendered, so the prerendered web_state
  // was swapped into the target tab model to perform the load.
  LOADED_PRERENDER,
  // No load performed, since the requested URL couldn't be loaded in incognito.
  // Calling code should instead load the URL in non-incognito.
  DISALLOWED_IN_INCOGNITO,
  // The requested URL load was a reload, and it was performed as a reload.
  RELOADED,
  // The URL load was initiated normally.
  NORMAL_LOAD
};

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

// Returns the result (as defined in the enum definition above) of initiating a
// URL load as defined in |chrome_params|, using |browser_state| and the active
// webState in |web_state_list|. |restorer| is provied for dependencies which
// may need to save the current session window.
URLLoadResult LoadURL(const ChromeLoadParams& chrome_params,
                      ios::ChromeBrowserState* browser_state,
                      WebStateList* web_state_list,
                      id<SessionWindowRestoring> restorer);

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_UTIL_H_
