// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/error_retry_state_machine.h"

#include "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ErrorRetryStateMachine::ErrorRetryStateMachine()
    : state_(ErrorRetryState::kNewRequest) {}

ErrorRetryStateMachine::ErrorRetryStateMachine(
    const ErrorRetryStateMachine& machine)
    : state_(machine.state_), url_(machine.url_) {}

void ErrorRetryStateMachine::SetURL(const GURL& url) {
  url_ = url;
}

ErrorRetryState ErrorRetryStateMachine::state() const {
  return state_;
}

void ErrorRetryStateMachine::ResetState() {
  state_ = ErrorRetryState::kNewRequest;
}

void ErrorRetryStateMachine::SetDisplayingNativeError() {
  DCHECK_EQ(ErrorRetryState::kReadyToDisplayErrorForFailedNavigation, state_);
  state_ = ErrorRetryState::kDisplayingNativeErrorForFailedNavigation;
}

ErrorRetryCommand ErrorRetryStateMachine::DidFailProvisionalNavigation(
    const GURL& web_view_url,
    const GURL& error_url) {
  switch (state_) {
    case ErrorRetryState::kNewRequest:
      if (web_view_url == wk_navigation_util::CreateRedirectUrl(error_url)) {
        // Client redirect in restore_session.html failed. A placeholder is not
        // needed here because a back/forward item already exists for
        // restore_session.html.
        state_ = ErrorRetryState::kReadyToDisplayErrorForFailedNavigation;
        return ErrorRetryCommand::kLoadErrorView;
      }
      // Provisional navigation failed on a new item.
      return ErrorRetryCommand::kLoadPlaceholder;

    // Reload of a previously successful load fails.
    case ErrorRetryState::kNoNavigationError:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kRetryFailedNavigationItem:
    // This case happens for the second back/forward navigation in offline mode
    // to a page that initially loaded successfully.
    case ErrorRetryState::kDisplayingNativeErrorForFailedNavigation:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kDisplayingWebErrorForFailedNavigation:
      return BackForwardOrReloadFailed(web_view_url, error_url);

    case ErrorRetryState::kReadyToDisplayErrorForFailedNavigation:
    case ErrorRetryState::kNavigatingToFailedNavigationItem:
      NOTREACHED() << "Unexpected error retry state: "
                   << static_cast<unsigned>(state_);
  }
  return ErrorRetryCommand::kDoNothing;
}

ErrorRetryCommand ErrorRetryStateMachine::DidFailNavigation(
    const GURL& web_view_url,
    const GURL& error_url) {
  switch (state_) {
    case ErrorRetryState::kNewRequest:
      state_ = ErrorRetryState::kReadyToDisplayErrorForFailedNavigation;
      return ErrorRetryCommand::kLoadErrorView;

    // Reload of a previously successful load fails.
    case ErrorRetryState::kNoNavigationError:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kRetryFailedNavigationItem:
    // Retry of a previous failure still fails.
    case ErrorRetryState::kDisplayingNativeErrorForFailedNavigation:
    case ErrorRetryState::kDisplayingWebErrorForFailedNavigation:
      return BackForwardOrReloadFailed(web_view_url, error_url);

    case ErrorRetryState::kReadyToDisplayErrorForFailedNavigation:
    case ErrorRetryState::kNavigatingToFailedNavigationItem:
      NOTREACHED() << "Unexpected error retry state: "
                   << static_cast<unsigned>(state_);
  }
  return ErrorRetryCommand::kDoNothing;
}

ErrorRetryCommand ErrorRetryStateMachine::DidFinishNavigation(
    const GURL& web_view_url) {
  switch (state_) {
    case ErrorRetryState::kNewRequest:
      // (1) Placeholder load for initial failure succeeded.
      if (!web::GetWebClient()->IsAppSpecificURL(url_) &&
          web_view_url ==
              wk_navigation_util::CreatePlaceholderUrlForUrl(url_)) {
        state_ = ErrorRetryState::kReadyToDisplayErrorForFailedNavigation;
        return ErrorRetryCommand::kLoadErrorView;
      }
      if (wk_navigation_util::IsRestoreSessionUrl(web_view_url)) {
        // (10) initial load of restore_session.html. Don't change state or
        // issue command. Wait for the client-side redirect.
      } else {
        // (2) Initial load succeeded.
        DCHECK(web::GetWebClient()->IsAppSpecificURL(url_) ||
               web_view_url == url_);
        state_ = ErrorRetryState::kNoNavigationError;
      }

      break;

    case ErrorRetryState::kReadyToDisplayErrorForFailedNavigation:
      // (3) Finished loading error in web view.
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kDisplayingWebErrorForFailedNavigation;
      break;

    case ErrorRetryState::kDisplayingNativeErrorForFailedNavigation:
      if (web_view_url ==
          wk_navigation_util::CreatePlaceholderUrlForUrl(url_)) {
        // (4) Back/forward to or reload of placeholder URL. Rewrite WebView URL
        // to prepare for retry.
        state_ = ErrorRetryState::kNavigatingToFailedNavigationItem;
        return ErrorRetryCommand::kRewriteWebViewURL;
      }

      // (5) Reload of original URL succeeded in WebView (either because it was
      // already in Page Cache or the network load succeded.
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kNoNavigationError;
      break;

    // (6) Successfully rewritten the WebView URL from placeholder URL to
    // original URL. Ready to try reload.
    case ErrorRetryState::kNavigatingToFailedNavigationItem:
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kRetryFailedNavigationItem;
      return ErrorRetryCommand::kReload;

    // (7) Retry loading succeeded.
    case ErrorRetryState::kRetryFailedNavigationItem:
    // (8) Back/forward to or reload of a previous WebUI error succeeds.
    case ErrorRetryState::kDisplayingWebErrorForFailedNavigation:
      DCHECK_EQ(web_view_url, url_);
      state_ = ErrorRetryState::kNoNavigationError;
      break;

    // (9) Back/forward to or reload of a previously successfull load succeeds
    // again.
    case ErrorRetryState::kNoNavigationError:
      break;
  }
  return ErrorRetryCommand::kDoNothing;
}

ErrorRetryCommand ErrorRetryStateMachine::BackForwardOrReloadFailed(
    const GURL& web_view_url,
    const GURL& error_url) {
  state_ = ErrorRetryState::kReadyToDisplayErrorForFailedNavigation;
  return ErrorRetryCommand::kLoadErrorView;
}

}  // namespace web
