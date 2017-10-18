// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"

// Observes page lifecyle events from Objective-C. To use as a
// web::WebStateObserver, wrap in a web::WebStateObserverBridge.
@protocol CRWWebStateObserver<NSObject>
@optional

// Invoked by WebStateObserverBridge::WasShown.
- (void)webStateWasShown:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::WasHidden.
- (void)webStateWasHidden:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::NavigationItemsPruned.
- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count;

// Invoked by WebStateObserverBridge::NavigationItemCommitted.
- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:
        (const web::LoadCommittedDetails&)load_details;

// Invoked by WebStateObserverBridge::DidStartNavigation.
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation;

// Invoked by WebStateObserverBridge::DidFinishNavigation.
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation;

// Invoked by WebStateObserverBridge::PageLoaded.
- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success;

// Invoked by WebStateObserverBridge::InterstitialDismissed.
- (void)webStateDidDismissInterstitial:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::LoadProgressChanged.
- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress;

// Invoked by WebStateObserverBridge::TitleWasSet.
- (void)webStateDidChangeTitle:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidChangeVisibleSecurityState.
- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidSuppressDialog.
- (void)webStateDidSuppressDialog:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DocumentSubmitted.
- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                     userInitiated:(BOOL)userInitiated;

// Invoked by WebStateObserverBridge::FormActivityRegistered.
// TODO(ios): Method should take data transfer object rather than parameters.
- (void)webState:(web::WebState*)webState
    didRegisterFormActivityWithFormNamed:(const std::string&)formName
                               fieldName:(const std::string&)fieldName
                                    type:(const std::string&)type
                                   value:(const std::string&)value
                            inputMissing:(BOOL)inputMissing;

// Invoked by WebStateObserverBridge::FaviconUrlUpdated.
- (void)webState:(web::WebState*)webState
    didUpdateFaviconURLCandidates:
        (const std::vector<web::FaviconURL>&)candidates;

// Invoked by WebStateObserverBridge::RenderProcessGone.
- (void)renderProcessGoneForWebState:(web::WebState*)webState;

// Note: after |webStateDestroyed:| is invoked, the WebState being observed
// is no longer valid.
- (void)webStateDestroyed:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidStopLoading.
- (void)webStateDidStopLoading:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidStartLoading.
- (void)webStateDidStartLoading:(web::WebState*)webState;

@end

namespace web {

class WebState;

// Bridge to use an id<CRWWebStateObserver> as a web::WebStateObserver.
// Will be added/removed as an observer of the underlying WebState during
// construction/destruction. Instances should be owned by instances of the
// class they're bridging.
class WebStateObserverBridge : public web::WebStateObserver {
 public:
  WebStateObserverBridge(web::WebState* web_state,
                         id<CRWWebStateObserver> observer);
  ~WebStateObserverBridge() override;

  // web::WebStateObserver methods.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void NavigationItemsPruned(web::WebState* web_state,
                             size_t pruned_item_count) override;
  void NavigationItemCommitted(
      web::WebState* web_state,
      const LoadCommittedDetails& load_details) override;
  void DidStartNavigation(web::WebState* web_state,
                          NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void InterstitialDismissed(web::WebState* web_state) override;
  void LoadProgressChanged(web::WebState* web_state, double progress) override;
  void TitleWasSet(web::WebState* web_state) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  void DidSuppressDialog(web::WebState* web_state) override;
  void DocumentSubmitted(web::WebState* web_state,
                         const std::string& form_name,
                         bool user_initiated) override;
  void FormActivityRegistered(web::WebState* web_state,
                              const std::string& form_name,
                              const std::string& field_name,
                              const std::string& type,
                              const std::string& value,
                              bool input_missing) override;
  void FaviconUrlUpdated(web::WebState* web_state,
                         const std::vector<FaviconURL>& candidates) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;

 private:
  __weak id<CRWWebStateObserver> observer_ = nil;
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverBridge);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_BRIDGE_H_
