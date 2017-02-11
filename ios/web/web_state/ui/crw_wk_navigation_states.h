// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_NAVIGATION_STATES_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_NAVIGATION_STATES_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {

// State of in-flight WKNavigation objects.
enum class WKNavigationState : int {
  // Navigation does not exist.
  NONE = 0,
  // WKNavigation returned from |loadRequest:|, |goToBackForwardListItem:|,
  // |loadFileURL:allowingReadAccessToURL:|, |loadHTMLString:baseURL:|,
  // |loadData:MIMEType:characterEncodingName:baseURL:|, |goBack|, |goForward|,
  // |reload| or |reloadFromOrigin|.
  REQUESTED,
  // WKNavigation passed to |webView:didStartProvisionalNavigation:|.
  STARTED,
  // WKNavigation passed to
  // |webView:didReceiveServerRedirectForProvisionalNavigation:|.
  REDIRECTED,
  // WKNavigation passed to |webView:didFailProvisionalNavigation:|.
  PROVISIONALY_FAILED,
  // WKNavigation passed to |webView:didCommitNavigation:|.
  COMMITTED,
  // WKNavigation passed to |webView:didFinishNavigation:|.
  FINISHED,
  // WKNavigation passed to |webView:didFailNavigation:withError:|.
  FAILED,
};

}  // namespace web

// Stores states for WKNavigation objects. Allows lookign up for last added
// navigation object.
@interface CRWWKNavigationStates : NSObject

// Adds a new navigation if it was not added yet. If navigation was already
// added then updates state for existing navigation. Updating state does not
// affect the result of |lastAddedNavigation| method. New added navigations
// should have WKNavigationState::REQUESTED, WKNavigationState::STARTED or
// WKNavigationState::COMMITTED state. Passed |navigation| will be help as weak
// reference and will not be retained. No-op if |navigation| is null.
- (void)setState:(web::WKNavigationState)state
    forNavigation:(WKNavigation*)navigation;

// WKNavigation which was added the most recently via |setState:forNavigation:|.
// Updating navigation state via |setState:forNavigation:| does not change the
// last added navigation. Returns nil if there are no stored navigations.
- (WKNavigation*)lastAddedNavigation;

// State of WKNavigation which was added the most recently via
// |setState:forNavigation:|.
- (web::WKNavigationState)lastAddedNavigationState;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_NAVIGATION_STATES_H_
