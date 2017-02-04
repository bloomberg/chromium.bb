// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_TEST_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_TEST_WEB_STATE_OBSERVER_H_

#include "ios/web/public/favicon_url.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web/public/web_state/web_state.h"

namespace web {

// Arguments passed to |webState:didStartProvisionalNavigationForURL:|.
struct TestStartProvisionalNavigationInfo {
  WebState* web_state;
  GURL url;
};

// Arguments passed to |webState:didCommitNavigationWithDetails:|.
struct TestCommitNavigationInfo {
  WebState* web_state;
  LoadCommittedDetails load_details;
};

// Arguments passed to |webState:didLoadPageWithSuccess:|.
struct TestLoadPageInfo {
  WebState* web_state;
  BOOL success;
};

// Arguments passed to |webStateDidDismissInterstitial:|.
struct TestDismissInterstitialInfo {
  WebState* web_state;
};

// Arguments passed to |webStateDidChangeURLHash:|.
struct TestChangeUrlHashInfo {
  WebState* web_state;
};

// Arguments passed to |webStateDidChangeHistoryState:|.
struct TestChangeHistoryStateInfo {
  WebState* web_state;
};

// Arguments passed to |webState:didChangeLoadingProgress:|.
struct TestChangeLoadingProgressInfo {
  WebState* web_state;
  double progress;
};

// Arguments passed to |webState:didSubmitDocumentWithFormNamed:userInitiated:|.
struct TestSubmitDocumentInfo {
  WebState* web_state;
  std::string form_name;
  BOOL user_initiated;
};

// Arguments passed to
// |webState:didRegisterFormActivityWithFormNamed:fieldName:type:value:|.
struct TestFormActivityInfo {
  TestFormActivityInfo();
  ~TestFormActivityInfo();
  WebState* web_state;
  std::string form_name;
  std::string field_name;
  std::string type;
  std::string value;
  bool input_missing;
};

// Arguments passed to |webState:didUpdateFaviconURLCandidates|.
struct TestUpdateFaviconUrlCandidatesInfo {
  TestUpdateFaviconUrlCandidatesInfo();
  ~TestUpdateFaviconUrlCandidatesInfo();
  WebState* web_state;
  std::vector<web::FaviconURL> candidates;
};

// Arguments passed to |webState:renderProcessGoneForWebState:|.
struct TestRenderProcessGoneInfo {
  WebState* web_state;
};

// Arguments passed to |webStateDestroyed:|.
struct TestWebStateDestroyedInfo {
  WebState* web_state;
};

// Arguments passed to |webStateDidStopLoading:|.
struct TestStopLoadingInfo {
  WebState* web_state;
};

// Arguments passed to |webStateDidStartLoading:|.
struct TestStartLoadingInfo {
  WebState* web_state;
};

}  // namespace web

// Test implementation of CRWWebStateObserver protocol.
@interface CRWTestWebStateObserver : NSObject<CRWWebStateObserver>

// Arguments passed to |webState:didStartProvisionalNavigationForURL:|.
@property(nonatomic, readonly)
    web::TestStartProvisionalNavigationInfo* startProvisionalNavigationInfo;
// Arguments passed to |webState:didCommitNavigationWithDetails:|.
@property(nonatomic, readonly)
    web::TestCommitNavigationInfo* commitNavigationInfo;
// Arguments passed to |webState:didLoadPageWithSuccess:|.
@property(nonatomic, readonly) web::TestLoadPageInfo* loadPageInfo;
// Arguments passed to |webStateDidDismissInterstitial:|.
@property(nonatomic, readonly)
    web::TestDismissInterstitialInfo* dismissInterstitialInfo;
// Arguments passed to |webStateDidChangeURLHash:|.
@property(nonatomic, readonly) web::TestChangeUrlHashInfo* changeUrlHashInfo;
// Arguments passed to |webStateDidChangeHistoryState:|.
@property(nonatomic, readonly)
    web::TestChangeHistoryStateInfo* changeHistoryStateInfo;
// Arguments passed to |webState:didChangeLoadingProgress:|.
@property(nonatomic, readonly)
    web::TestChangeLoadingProgressInfo* changeLoadingProgressInfo;
// Arguments passed to |webState:didSubmitDocumentWithFormNamed:userInitiated:|.
@property(nonatomic, readonly) web::TestSubmitDocumentInfo* submitDocumentInfo;
// Arguments passed to
// |webState:didRegisterFormActivityWithFormNamed:fieldName:type:value:|.
@property(nonatomic, readonly) web::TestFormActivityInfo* formActivityInfo;
// Arguments passed to |webState:didUpdateFaviconURLCandidates|.
@property(nonatomic, readonly)
    web::TestUpdateFaviconUrlCandidatesInfo* updateFaviconUrlCandidatesInfo;
// Arguments passed to |webState:renderProcessGoneForWebState:|.
@property(nonatomic, readonly)
    web::TestRenderProcessGoneInfo* renderProcessGoneInfo;
// Arguments passed to |webStateDestroyed:|.
@property(nonatomic, readonly)
    web::TestWebStateDestroyedInfo* webStateDestroyedInfo;
// Arguments passed to |webStateDidStopLoading:|.
@property(nonatomic, readonly) web::TestStopLoadingInfo* stopLoadingInfo;
// Arguments passed to |webStateDidStartLoading:|.
@property(nonatomic, readonly) web::TestStartLoadingInfo* startLoadingInfo;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_TEST_WEB_STATE_OBSERVER_H_
