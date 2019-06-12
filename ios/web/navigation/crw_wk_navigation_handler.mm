// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#import "ios/net/http_response_headers_util.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/navigation_manager_util.h"
#import "ios/web/navigation/web_kit_constants.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_action_policy_util.h"
#import "ios/web/navigation/wk_navigation_action_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/wk_web_view_util.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::wk_navigation_util::IsPlaceholderUrl;
using web::wk_navigation_util::CreatePlaceholderUrlForUrl;
using web::wk_navigation_util::kReferrerHeaderName;
using web::wk_navigation_util::IsRestoreSessionUrl;
using web::wk_navigation_util::IsWKInternalUrl;

@interface CRWWKNavigationHandler () {
  // Used to poll for a SafeBrowsing warning being displayed. This is created in
  // |decidePolicyForNavigationAction| and destroyed once any of the following
  // happens: 1) a SafeBrowsing warning is detected; 2) any WKNavigationDelegate
  // method is called; 3) |stopLoading| is called.
  base::RepeatingTimer _safeBrowsingWarningDetectionTimer;

  // Referrer for the current page; does not include the fragment.
  NSString* _currentReferrerString;

  // The WKNavigation captured when |stopLoading| was called. Used for reporting
  // WebController.EmptyNavigationManagerCausedByStopLoading UMA metric which
  // helps with diagnosing a navigation related crash (crbug.com/565457).
  __weak WKNavigation* _stoppedWKNavigation;
}

// Returns the WebStateImpl from self.delegate.
@property(nonatomic, readonly, assign) web::WebStateImpl* webStateImpl;
// Returns the NavigationManagerImpl from self.webStateImpl.
@property(nonatomic, readonly, assign)
    web::NavigationManagerImpl* navigationManagerImpl;
// Returns the UserInteractionState from self.delegate.
@property(nonatomic, readonly, assign)
    web::UserInteractionState* userInteractionState;
// Returns the CertVerificationErrorsCacheType from self.delegate.
@property(nonatomic, readonly, assign)
    web::CertVerificationErrorsCacheType* certVerificationErrors;

@end

@implementation CRWWKNavigationHandler

- (instancetype)init {
  if (self = [super init]) {
    _navigationStates = [[CRWWKNavigationStates alloc] init];
    // Load phase when no WebView present is 'loaded' because this represents
    // the idle state.
    _navigationState = web::WKNavigationState::FINISHED;
  }
  return self;
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  [self didReceiveWKNavigationDelegateCallback];

  self.webProcessCrashed = NO;
  if ([self.delegate navigationHandlerWebViewBeingDestroyed:self]) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  GURL requestURL = net::GURLWithNSURL(action.request.URL);

  // Workaround for a WKWebView bug where the web content loaded using
  // |-loadHTMLString:baseURL| clobbers the next WKBackForwardListItem. It works
  // by detecting back/forward navigation to a clobbered item and replacing the
  // clobberred item and its forward history using a partial session restore in
  // the current web view. There is an unfortunate caveat: if the workaround is
  // triggered in a back navigation to a clobbered item, the restored forward
  // session is inserted after the current item before the back navigation, so
  // it doesn't fully replaces the "bad" history, even though user will be
  // navigated to the expected URL and may not notice the issue until they
  // review the back history by long pressing on "Back" button.
  //
  // TODO(crbug.com/887497): remove this workaround once iOS ships the fix.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      action.targetFrame.mainFrame) {
    GURL webViewURL = net::GURLWithNSURL(webView.URL);
    GURL currentWKItemURL =
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    GURL backItemURL = net::GURLWithNSURL(webView.backForwardList.backItem.URL);
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:webViewURL];
    bool willClobberHistory =
        action.navigationType == WKNavigationTypeBackForward &&
        requestURL == backItemURL && webView.backForwardList.currentItem &&
        requestURL != currentWKItemURL && currentWKItemURL == webViewURL &&
        context &&
        (context->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK);

    UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewClobberedHistory", willClobberHistory);

    if (willClobberHistory && base::FeatureList::IsEnabled(
                                  web::features::kHistoryClobberWorkaround)) {
      decisionHandler(WKNavigationActionPolicyCancel);
      self.navigationManagerImpl
          ->ApplyWKWebViewForwardHistoryClobberWorkaround();
      return;
    }
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationAction:action];

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      action.targetFrame.mainFrame &&
      action.navigationType == WKNavigationTypeBackForward) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    if (context) {
      // Context is null for renderer-initiated navigations.
      int index = web::GetCommittedItemIndexWithUniqueID(
          self.navigationManagerImpl, context->GetNavigationItemUniqueID());
      self.navigationManagerImpl->SetPendingItemIndex(index);
    }
  }

  // If this is a placeholder navigation, pass through.
  if (IsPlaceholderUrl(requestURL)) {
    decisionHandler(WKNavigationActionPolicyAllow);
    return;
  }

  ui::PageTransition transition =
      [self.delegate navigationHandler:self
          pageTransitionFromNavigationType:action.navigationType];
  BOOL isMainFrameNavigationAction = [self isMainFrameNavigationAction:action];
  if (isMainFrameNavigationAction) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    if (context) {
      DCHECK(!context->IsRendererInitiated() ||
             (context->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK));
      transition = context->GetPageTransition();
      if (context->IsLoadingErrorPage()) {
        // loadHTMLString: navigation which loads error page into WKWebView.
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
      }
    }
  }

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // WKBasedNavigationManager doesn't use |loadCurrentURL| for reload or back/
    // forward navigation. So this is the first point where a form repost would
    // be detected. Display the confirmation dialog.
    if ([action.request.HTTPMethod isEqual:@"POST"] &&
        (action.navigationType == WKNavigationTypeFormResubmitted)) {
      self.webStateImpl->ShowRepostFormWarningDialog(
          base::BindOnce(^(bool shouldContinue) {
            if (shouldContinue) {
              decisionHandler(WKNavigationActionPolicyAllow);
            } else {
              decisionHandler(WKNavigationActionPolicyCancel);
              if (action.targetFrame.mainFrame) {
                [self.pendingNavigationInfo setCancelled:YES];
                self.webStateImpl->SetIsLoading(false);
              }
            }
          }));
      return;
    }
  }

  // Invalid URLs should not be loaded.
  if (!requestURL.is_valid()) {
    decisionHandler(WKNavigationActionPolicyCancel);
    // The HTML5 spec indicates that window.open with an invalid URL should open
    // about:blank.
    BOOL isFirstLoadInOpenedWindow =
        self.webStateImpl->HasOpener() &&
        !self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
    BOOL isMainFrame = action.targetFrame.mainFrame;
    if (isFirstLoadInOpenedWindow && isMainFrame) {
      GURL aboutBlankURL(url::kAboutBlankURL);
      web::NavigationManager::WebLoadParams loadParams(aboutBlankURL);
      loadParams.referrer = self.currentReferrer;

      self.webStateImpl->GetNavigationManager()->LoadURLWithParams(loadParams);
    }
    return;
  }

  // First check if the navigation action should be blocked by the controller
  // and make sure to update the controller in the case that the controller
  // can't handle the request URL. Then use the embedders' policyDeciders to
  // either: 1- Handle the URL it self and return false to stop the controller
  // from proceeding with the navigation if needed. or 2- return true to allow
  // the navigation to be proceeded by the web controller.
  BOOL allowLoad = YES;
  if (web::GetWebClient()->IsAppSpecificURL(requestURL)) {
    allowLoad = [self shouldAllowAppSpecificURLNavigationAction:action
                                                     transition:transition];
    if (allowLoad && !self.webStateImpl->HasWebUI()) {
      [self.delegate navigationHandler:self createWebUIForURL:requestURL];
    }
  }

  BOOL webControllerCanShow =
      web::UrlHasWebScheme(requestURL) ||
      web::GetWebClient()->IsAppSpecificURL(requestURL) ||
      requestURL.SchemeIs(url::kFileScheme) ||
      requestURL.SchemeIs(url::kAboutScheme) ||
      requestURL.SchemeIs(url::kBlobScheme);

  if (allowLoad) {
    // If the URL doesn't look like one that can be shown as a web page, it may
    // handled by the embedder. In that case, update the web controller to
    // correctly reflect the current state.
    if (!webControllerCanShow) {
      if (!web::features::StorePendingItemInContext()) {
        if ([self isMainFrameNavigationAction:action]) {
          [self.delegate navigationHandlerStopLoading:self];
        }
      }

      // Purge web view if last committed URL is different from the document
      // URL. This can happen if external URL was added to the navigation stack
      // and was loaded using Go Back or Go Forward navigation (in which case
      // document URL will point to the previous page).  If this is the first
      // load for a NavigationManager, there will be no last committed item, so
      // check here.
      // TODO(crbug.com/850760): Check if this code is still needed. The current
      // implementation doesn't put external apps URLs in the history, so they
      // shouldn't be accessable by Go Back or Go Forward navigation.
      web::NavigationItem* lastCommittedItem =
          self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
      if (lastCommittedItem) {
        GURL lastCommittedURL = lastCommittedItem->GetURL();
        if (lastCommittedURL !=
            [self.delegate navigationHandlerDocumentURL:self]) {
          [self.delegate navigationHandlerRequirePageReconstruction:self];
          [self.delegate navigationHandler:self
                            setDocumentURL:lastCommittedURL
                                   context:nullptr];
        }
      }
    }
  }

  if (allowLoad) {
    BOOL userInteractedWithRequestMainFrame =
        self.userInteractionState->HasUserTappedRecently(webView) &&
        net::GURLWithNSURL(action.request.mainDocumentURL) ==
            self.userInteractionState->LastUserInteraction()->main_document_url;
    web::WebStatePolicyDecider::RequestInfo requestInfo(
        transition, isMainFrameNavigationAction,
        userInteractedWithRequestMainFrame);

    allowLoad =
        self.webStateImpl->ShouldAllowRequest(action.request, requestInfo);
    // The WebState may have been closed in the ShouldAllowRequest callback.
    if ([self.delegate navigationHandlerWebViewBeingDestroyed:self]) {
      decisionHandler(WKNavigationActionPolicyCancel);
      return;
    }
  }

  if (!webControllerCanShow && web::features::StorePendingItemInContext()) {
    allowLoad = NO;
  }

  if (allowLoad) {
    if ([[action.request HTTPMethod] isEqualToString:@"POST"]) {
      web::NavigationItemImpl* item =
          self.navigationManagerImpl->GetCurrentItemImpl();
      // TODO(crbug.com/570699): Remove this check once it's no longer possible
      // to have no current entries.
      if (item)
        [self cachePOSTDataForRequest:action.request inNavigationItem:item];
    }
  } else {
    if (action.targetFrame.mainFrame) {
      [self.pendingNavigationInfo setCancelled:YES];
      if (!web::features::StorePendingItemInContext() ||
          self.navigationManagerImpl->GetPendingItemIndex() == -1) {
        // Discard the new pending item to ensure that the current URL is not
        // different from what is displayed on the view. Discard only happens
        // if the last item was not a native view, to avoid ugly animation of
        // inserting the webview. There is no need to reset pending item index
        // for a different pending back-forward navigation.
        [self discardNonCommittedItemsIfLastCommittedWasNotNativeView];
      }

      web::NavigationContextImpl* context =
          [self contextForPendingMainFrameNavigationWithURL:requestURL];
      if (context) {
        // Destroy associated pending item, because this will be the last
        // WKWebView callback for this navigation context.
        context->ReleaseItem();
      }

      if (![self.delegate navigationHandlerWebViewBeingDestroyed:self] &&
          [self shouldClosePageOnNativeApplicationLoad]) {
        // Loading was started for user initiated navigations and should be
        // stopped because no other WKWebView callbacks are called.
        // TODO(crbug.com/767092): Loading should not start until
        // webView.loading is changed to YES.
        self.webStateImpl->SetIsLoading(false);
        self.webStateImpl->CloseWebState();
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
      }
    }

    if (![self.delegate navigationHandlerWebViewBeingDestroyed:self]) {
      // Loading was started for user initiated navigations and should be
      // stopped because no other WKWebView callbacks are called.
      // TODO(crbug.com/767092): Loading should not start until webView.loading
      // is changed to YES.
      self.webStateImpl->SetIsLoading(false);
    }
  }

  // Only try to detect a SafeBrowsing warning if one isn't already displayed,
  // since the detection logic won't be able to distinguish between the current
  // warning and a warning for the page that's about to be loaded. Also, since
  // the purpose of running this logic is to ensure that the right URL is
  // displayed in the omnibox, don't try to detect a SafeBrowsing warning for
  // iframe navigations, because the omnibox already shows the correct main
  // frame URL in that case.
  if (allowLoad && isMainFrameNavigationAction &&
      !web::IsSafeBrowsingWarningDisplayedInWebView(webView)) {
    __weak CRWWKNavigationHandler* weakSelf = self;
    __weak WKWebView* weakWebView = webView;
    const base::TimeDelta kDelayUntilSafeBrowsingWarningCheck =
        base::TimeDelta::FromMilliseconds(20);
    _safeBrowsingWarningDetectionTimer.Start(
        FROM_HERE, kDelayUntilSafeBrowsingWarningCheck, base::BindRepeating(^{
          __strong __typeof(weakSelf) strongSelf = weakSelf;
          __strong __typeof(weakWebView) strongWebView = weakWebView;
          if (web::IsSafeBrowsingWarningDisplayedInWebView(strongWebView)) {
            // Extract state from an existing navigation context if one exists.
            // Create a new context rather than just re-using the existing one,
            // since the existing context will continue to be used if the user
            // decides to proceed to the unsafe page. In that case, WebKit
            // continues the navigation with the same WKNavigation* that's
            // associated with the existing context.
            web::NavigationContextImpl* existingContext = [strongSelf
                contextForPendingMainFrameNavigationWithURL:requestURL];
            bool hasUserGesture =
                existingContext ? existingContext->HasUserGesture() : false;
            bool isRendererInitiated =
                existingContext ? existingContext->IsRendererInitiated() : true;
            std::unique_ptr<web::NavigationContextImpl> context =
                web::NavigationContextImpl::CreateNavigationContext(
                    strongSelf.webStateImpl, requestURL, hasUserGesture,
                    transition, isRendererInitiated);
            [strongSelf navigationManagerImpl] -> AddTransientItem(requestURL);
            strongSelf.webStateImpl->OnNavigationStarted(context.get());
            strongSelf.webStateImpl->OnNavigationFinished(context.get());
            strongSelf->_safeBrowsingWarningDetectionTimer.Stop();
            if (!existingContext) {
              // If there's an existing context, observers will already be aware
              // of a load in progress. Otherwise, observers need to be notified
              // here, so that if the user decides to go back to the previous
              // page (stopping the load), observers will be aware of a possible
              // URL change and the URL displayed in the omnibox will get
              // updated.
              DCHECK(strongWebView.loading);
              strongSelf.webStateImpl->SetIsLoading(true);
            }
          }
        }));
  }

  if (!allowLoad) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }
  BOOL isOffTheRecord = self.webStateImpl->GetBrowserState()->IsOffTheRecord();
  decisionHandler(web::GetAllowNavigationActionPolicy(isOffTheRecord));
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)WKResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  [self didReceiveWKNavigationDelegateCallback];

  // If this is a placeholder navigation, pass through.
  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (IsPlaceholderUrl(responseURL)) {
    handler(WKNavigationResponsePolicyAllow);
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> headers;
  if ([WKResponse.response isKindOfClass:[NSHTTPURLResponse class]]) {
    headers = net::CreateHeadersFromNSHTTPURLResponse(
        static_cast<NSHTTPURLResponse*>(WKResponse.response));
    // TODO(crbug.com/551677): remove |OnHttpResponseHeadersReceived| and attach
    // headers to web::NavigationContext.
    self.webStateImpl->OnHttpResponseHeadersReceived(headers.get(),
                                                     responseURL);
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationResponse:WKResponse];

  BOOL shouldRenderResponse = [self shouldRenderResponse:WKResponse];
  if (!shouldRenderResponse) {
    if (web::UrlHasWebScheme(responseURL)) {
      [self createDownloadTaskForResponse:WKResponse HTTPHeaders:headers.get()];
    } else {
      // DownloadTask only supports web schemes, so do nothing.
    }
    // Discard the pending item to ensure that the current URL is not different
    // from what is displayed on the view.
    [self discardNonCommittedItemsIfLastCommittedWasNotNativeView];
    if (!web::features::StorePendingItemInContext()) {
      // Loading will be stopped in webView:didFinishNavigation: callback. This
      // call is here to preserve the original behavior when pending item is not
      // stored in NavigationContext.
      self.webStateImpl->SetIsLoading(false);
    }
  } else {
    shouldRenderResponse = self.webStateImpl->ShouldAllowResponse(
        WKResponse.response, WKResponse.forMainFrame);
  }

  if (!shouldRenderResponse && WKResponse.canShowMIMEType &&
      WKResponse.forMainFrame) {
    self.pendingNavigationInfo.cancelled = YES;
  }

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      !WKResponse.forMainFrame && !webView.loading) {
    // This is the terminal callback for iframe navigation and there is no
    // pending main frame navigation. Last chance to flip IsLoading to false.
    self.webStateImpl->SetIsLoading(false);
  }

  handler(shouldRenderResponse ? WKNavigationResponsePolicyAllow
                               : WKNavigationResponsePolicyCancel);
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  [self.navigationStates setState:web::WKNavigationState::STARTED
                    forNavigation:navigation];

  if (webViewURL.is_empty()) {
    // May happen on iOS9, however in didCommitNavigation: callback the URL
    // will be "about:blank".
    webViewURL = GURL(url::kAboutBlankURL);
  }

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];

  if (context) {
    // This is already seen and registered navigation.

    if (context->IsLoadingErrorPage()) {
      // This is loadHTMLString: navigation to display error page in web view.
      self.navigationState = web::WKNavigationState::REQUESTED;
      return;
    }

    if (!context->IsPlaceholderNavigation() &&
        context->GetUrl() != webViewURL) {
      // Update last seen URL because it may be changed by WKWebView (f.e. by
      // performing characters escaping).
      web::NavigationItem* item =
          web::GetItemWithUniqueID(self.navigationManagerImpl, context);
      if (!IsWKInternalUrl(webViewURL)) {
        if (item) {
          item->SetURL(webViewURL);
        }
        context->SetUrl(webViewURL);
      }
    }
    self.webStateImpl->OnNavigationStarted(context);
    return;
  }

  // This is renderer-initiated navigation which was not seen before and
  // should be registered.

  // When using WKBasedNavigationManager, renderer-initiated app-specific loads
  // should be allowed in two specific cases:
  // 1) if |backForwardList.currentItem| is a placeholder URL for the
  //    provisional load URL (i.e. webView.URL), then this is an in-progress
  //    app-specific load and should not be restarted.
  // 2) back/forward navigation to an app-specific URL should be allowed.
  bool exemptedAppSpecificLoad = false;
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    bool currentItemIsPlaceholder =
        CreatePlaceholderUrlForUrl(webViewURL) ==
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    bool isBackForward = self.pendingNavigationInfo.navigationType ==
                         WKNavigationTypeBackForward;
    bool isRestoringSession =
        web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        IsRestoreSessionUrl([self.delegate navigationHandlerDocumentURL:self]);
    exemptedAppSpecificLoad =
        currentItemIsPlaceholder || isBackForward || isRestoringSession;
  }

  if (!web::GetWebClient()->IsAppSpecificURL(webViewURL) ||
      !exemptedAppSpecificLoad) {
    self.webStateImpl->ClearWebUI();
  }

  if (web::GetWebClient()->IsAppSpecificURL(webViewURL) &&
      !exemptedAppSpecificLoad) {
    // Restart app specific URL loads to properly capture state.
    // TODO(crbug.com/546347): Extract necessary tasks for app specific URL
    // navigation rather than restarting the load.

    // Renderer-initiated loads of WebUI can be done only from other WebUI
    // pages. WebUI pages may have increased power and using the same web
    // process (which may potentially be controller by an attacker) is
    // dangerous.
    if (web::GetWebClient()->IsAppSpecificURL(
            [self.delegate navigationHandlerDocumentURL:self])) {
      [self.delegate navigationHandlerAbortLoading:self];
      web::NavigationManager::WebLoadParams params(webViewURL);
      self.navigationManagerImpl->LoadURLWithParams(params);
    }
    return;
  }

  self.webStateImpl->GetNavigationManagerImpl()
      .OnRendererInitiatedNavigationStarted(webViewURL);

  // When a client-side redirect occurs while an interstitial warning is
  // displayed, clear the warning and its navigation item, so that a new
  // pending item is created for |context| in |registerLoadRequestForURL|. See
  // crbug.com/861836.
  self.webStateImpl->ClearTransientContent();

  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self.delegate navigationHandler:self
             registerLoadRequestForURL:webViewURL
                sameDocumentNavigation:NO
                        hasUserGesture:self.pendingNavigationInfo.hasUserGesture
                     rendererInitiated:YES
                 placeholderNavigation:IsPlaceholderUrl(webViewURL)];
  web::NavigationContextImpl* navigationContextPtr = navigationContext.get();
  // GetPendingItem which may be called inside OnNavigationStarted relies on
  // association between NavigationContextImpl and WKNavigation.
  [self.navigationStates setContext:std::move(navigationContext)
                      forNavigation:navigation];
  self.webStateImpl->OnNavigationStarted(navigationContextPtr);
  DCHECK_EQ(web::WKNavigationState::REQUESTED, self.navigationState);
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  // This callback should never be triggered for placeholder navigations.
  DCHECK(!(web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
           IsPlaceholderUrl(webViewURL)));

  [self.navigationStates setState:web::WKNavigationState::REDIRECTED
                    forNavigation:navigation];

  web::NavigationContextImpl* context =
      [self.navigationStates contextForNavigation:navigation];
  [self didReceiveRedirectForNavigation:context withURL:webViewURL];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];

  [self.navigationStates setState:web::WKNavigationState::PROVISIONALY_FAILED
                    forNavigation:navigation];

  // Ignore provisional navigation failure if a new navigation has been started,
  // for example, if a page is reloaded after the start of the provisional
  // load but before the load has been committed.
  if (![[self.navigationStates lastAddedNavigation] isEqual:navigation]) {
    return;
  }

  // TODO(crbug.com/570699): Remove this workaround once |stopLoading| does not
  // discard pending navigation items.
  if ((!self.webStateImpl ||
       !self.webStateImpl->GetNavigationManagerImpl().GetVisibleItem()) &&
      _stoppedWKNavigation &&
      [error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange) {
    // App is going to crash in this state (crbug.com/565457). Crash will occur
    // on dereferencing visible navigation item, which is null. This scenario is
    // possible after pending load was stopped for a child window. Early return
    // to prevent the crash and report UMA metric to check if crash happening
    // because the load was stopped.
    UMA_HISTOGRAM_BOOLEAN(
        "WebController.EmptyNavigationManagerCausedByStopLoading",
        [_stoppedWKNavigation isEqual:navigation]);
    return;
  }

  // Handle load cancellation for directly cancelled navigations without
  // handling their potential errors. Otherwise, handle the error.
  if (self.pendingNavigationInfo.cancelled) {
    [self.delegate navigationHandler:self
                handleCancelledError:error
                       forNavigation:navigation
                     provisionalLoad:YES];
  } else if (error.code == NSURLErrorUnsupportedURL &&
             self.webStateImpl->HasWebUI()) {
    // This is a navigation to WebUI page.
    DCHECK(web::GetWebClient()->IsAppSpecificURL(
        net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey])));
  } else {
    if (web::IsWKWebViewSSLCertError(error)) {
      [self.delegate navigationHandler:self
                    handleSSLCertError:error
                         forNavigation:navigation];
    } else {
      [self.delegate navigationHandler:self
                       handleLoadError:error
                         forNavigation:navigation
                       provisionalLoad:YES];
    }
  }

  [self.delegate navigationHandlerRemoveAllWebFrames:self];
  // This must be reset at the end, since code above may need information about
  // the pending load.
  self.pendingNavigationInfo = nil;
  self.certVerificationErrors->Clear();
  if (web::features::StorePendingItemInContext()) {
    // Remove the navigation to immediately get rid of pending item.
    if (web::WKNavigationState::NONE !=
        [self.navigationStates stateForNavigation:navigation]) {
      [self.navigationStates removeNavigation:navigation];
    }
  } else {
    [self forgetNullWKNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webView:(WKWebView*)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge
                    completionHandler:
                        (void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  [self didReceiveWKNavigationDelegateCallback];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [self didReceiveWKNavigationDelegateCallback];
}

#pragma mark - Private methods

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return &(self.webStateImpl->GetNavigationManagerImpl());
}

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForNavigationHandler:self];
}

- (web::UserInteractionState*)userInteractionState {
  return [self.delegate userInteractionStateForNavigationHandler:self];
}

- (web::CertVerificationErrorsCacheType*)certVerificationErrors {
  return [self.delegate certVerificationErrorsForNavigationHandler:self];
}

- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

// This method should be called on receiving WKNavigationDelegate callbacks. It
// will log a metric if the callback occurs after the reciever has already been
// closed. It also stops the SafeBrowsing warning detection timer, since after
// this point it's too late for a SafeBrowsing warning to be displayed for the
// navigation for which the timer was started.
- (void)didReceiveWKNavigationDelegateCallback {
  if ([self.delegate navigationHandlerWebViewBeingDestroyed:self]) {
    UMA_HISTOGRAM_BOOLEAN("Renderer.WKWebViewCallbackAfterDestroy", true);
  }
  _safeBrowsingWarningDetectionTimer.Stop();
}

// Extracts navigation info from WKNavigationAction and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationAction|, but must be in a pending state until
// |didgo/Navigation| where it becames current.
- (void)updatePendingNavigationInfoFromNavigationAction:
    (WKNavigationAction*)action {
  if (action.targetFrame.mainFrame) {
    self.pendingNavigationInfo = [[CRWPendingNavigationInfo alloc] init];
    self.pendingNavigationInfo.referrer =
        [action.request valueForHTTPHeaderField:kReferrerHeaderName];
    self.pendingNavigationInfo.navigationType = action.navigationType;
    self.pendingNavigationInfo.HTTPMethod = action.request.HTTPMethod;
    self.pendingNavigationInfo.hasUserGesture =
        web::GetNavigationActionInitiationType(action) ==
        web::NavigationActionInitiationType::kUserInitiated;
  }
}

// Returns YES if the navigation action is associated with a main frame request.
- (BOOL)isMainFrameNavigationAction:(WKNavigationAction*)action {
  if (action.targetFrame) {
    return action.targetFrame.mainFrame;
  }
  // According to WKNavigationAction documentation, in the case of a new window
  // navigation, target frame will be nil. In this case check if the
  // |sourceFrame| is the mainFrame.
  return action.sourceFrame.mainFrame;
}

// Returns YES if the given |action| should be allowed to continue for app
// specific URL. If this returns NO, the navigation should be cancelled.
// App specific pages have elevated privileges and WKWebView uses the same
// renderer process for all page frames. With that Chromium does not allow
// running App specific pages in the same process as a web site from the
// internet. Allows navigation to app specific URL in the following cases:
//   - last committed URL is app specific
//   - navigation not a new navigation (back-forward or reload)
//   - navigation is typed, generated or bookmark
//   - navigation is performed in iframe and main frame is app-specific page
- (BOOL)shouldAllowAppSpecificURLNavigationAction:(WKNavigationAction*)action
                                       transition:
                                           (ui::PageTransition)pageTransition {
  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  DCHECK(web::GetWebClient()->IsAppSpecificURL(requestURL));
  if (web::GetWebClient()->IsAppSpecificURL(
          self.webStateImpl->GetLastCommittedURL())) {
    // Last committed page is also app specific and navigation should be
    // allowed.
    return YES;
  }

  if (!ui::PageTransitionIsNewNavigation(pageTransition)) {
    // Allow reloads and back-forward navigations.
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(pageTransition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(
          pageTransition, ui::PAGE_TRANSITION_GENERATED)) {
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(
          pageTransition, ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return YES;
  }

  // If the session is being restored, allow the navigation.
  if (IsRestoreSessionUrl([self.delegate navigationHandlerDocumentURL:self])) {
    return YES;
  }

  GURL mainDocumentURL = net::GURLWithNSURL(action.request.mainDocumentURL);
  if (web::GetWebClient()->IsAppSpecificURL(mainDocumentURL) &&
      !action.sourceFrame.mainFrame) {
    // AppSpecific URLs are allowed inside iframe if the main frame is also
    // app specific page.
    return YES;
  }

  return NO;
}

// Caches request POST data in the given session entry.
- (void)cachePOSTDataForRequest:(NSURLRequest*)request
               inNavigationItem:(web::NavigationItemImpl*)item {
  NSUInteger maxPOSTDataSizeInBytes = 4096;
  NSString* cookieHeaderName = @"cookie";

  DCHECK(item);
  const bool shouldUpdateEntry =
      ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) &&
      ![request HTTPBodyStream] &&  // Don't cache streams.
      !item->HasPostData() &&
      item->GetURL() == net::GURLWithNSURL([request URL]);
  const bool belowSizeCap =
      [[request HTTPBody] length] < maxPOSTDataSizeInBytes;
  DLOG_IF(WARNING, shouldUpdateEntry && !belowSizeCap)
      << "Data in POST request exceeds the size cap (" << maxPOSTDataSizeInBytes
      << " bytes), and will not be cached.";

  if (shouldUpdateEntry && belowSizeCap) {
    item->SetPostData([request HTTPBody]);
    item->ResetHttpRequestHeaders();
    item->AddHttpRequestHeaders([request allHTTPHeaderFields]);
    // Don't cache the "Cookie" header.
    // According to NSURLRequest documentation, |-valueForHTTPHeaderField:| is
    // case insensitive, so it's enough to test the lower case only.
    if ([request valueForHTTPHeaderField:cookieHeaderName]) {
      // Case insensitive search in |headers|.
      NSSet* cookieKeys = [item->GetHttpRequestHeaders()
          keysOfEntriesPassingTest:^(id key, id obj, BOOL* stop) {
            NSString* header = (NSString*)key;
            const BOOL found =
                [header caseInsensitiveCompare:cookieHeaderName] ==
                NSOrderedSame;
            *stop = found;
            return found;
          }];
      DCHECK_EQ(1u, [cookieKeys count]);
      item->RemoveHttpRequestHeaderForKey([cookieKeys anyObject]);
    }
  }
}

// Discards non committed items, only if the last committed URL was not loaded
// in native view. But if it was a native view, no discard will happen to avoid
// an ugly animation where the web view is inserted and quickly removed.
- (void)discardNonCommittedItemsIfLastCommittedWasNotNativeView {
  GURL lastCommittedURL = self.webStateImpl->GetLastCommittedURL();
  BOOL previousItemWasLoadedInNativeView =
      [self.delegate navigationHandler:self
             shouldLoadURLInNativeView:lastCommittedURL];
  if (!previousItemWasLoadedInNativeView)
    self.navigationManagerImpl->DiscardNonCommittedItems();
}

// If YES, the page should be closed if it successfully redirects to a native
// application, for example if a new tab redirects to the App Store.
- (BOOL)shouldClosePageOnNativeApplicationLoad {
  // The page should be closed if it was initiated by the DOM and there has been
  // no user interaction with the page since the web view was created, or if
  // the page has no navigation items, as occurs when an App Store link is
  // opened from another application.
  BOOL rendererInitiatedWithoutInteraction =
      self.webStateImpl->HasOpener() &&
      !self.userInteractionState
           ->UserInteractionRegisteredSinceWebViewCreated();
  BOOL noNavigationItems = !(self.navigationManagerImpl->GetItemCount());
  return rendererInitiatedWithoutInteraction || noNavigationItems;
}

// Extracts navigation info from WKNavigationResponse and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationResponse|, but must be in a pending state until
// |didCommitNavigation| where it becames current.
- (void)updatePendingNavigationInfoFromNavigationResponse:
    (WKNavigationResponse*)response {
  if (response.isForMainFrame) {
    if (!self.pendingNavigationInfo) {
      self.pendingNavigationInfo = [[CRWPendingNavigationInfo alloc] init];
    }
    self.pendingNavigationInfo.MIMEType = response.response.MIMEType;
  }
}

// Returns YES if response should be rendered in WKWebView.
- (BOOL)shouldRenderResponse:(WKNavigationResponse*)WKResponse {
  if (!WKResponse.canShowMIMEType) {
    return NO;
  }

  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (responseURL.SchemeIs(url::kDataScheme) && WKResponse.forMainFrame) {
    // Block rendering data URLs for renderer-initiated navigations in main
    // frame to prevent abusive behavior (crbug.com/890558).
    web::NavigationContext* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    if (context->IsRendererInitiated()) {
      return NO;
    }
  }

  return YES;
}

// Creates DownloadTask for the given navigation response. Headers are passed
// as argument to avoid extra NSDictionary -> net::HttpResponseHeaders
// conversion.
- (void)createDownloadTaskForResponse:(WKNavigationResponse*)WKResponse
                          HTTPHeaders:(net::HttpResponseHeaders*)headers {
  const GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  const int64_t contentLength = WKResponse.response.expectedContentLength;
  const std::string MIMEType =
      base::SysNSStringToUTF8(WKResponse.response.MIMEType);

  std::string contentDisposition;
  if (headers) {
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  }

  ui::PageTransition transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  if (WKResponse.forMainFrame) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    context->SetIsDownload(true);
    context->ReleaseItem();
    // Navigation callbacks can only be called for the main frame.
    self.webStateImpl->OnNavigationFinished(context);
    transition = context->GetPageTransition();
    bool transitionIsLink = ui::PageTransitionTypeIncludingQualifiersIs(
        transition, ui::PAGE_TRANSITION_LINK);
    if (transitionIsLink && !context->HasUserGesture()) {
      // Link click is not possible without user gesture, so this transition
      // was incorrectly classified and should be "client redirect" instead.
      // TODO(crbug.com/549301): Remove this workaround when transition
      // detection is fixed.
      transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
    }
  }
  web::DownloadController::FromBrowserState(
      self.webStateImpl->GetBrowserState())
      ->CreateDownloadTask(self.webStateImpl, [NSUUID UUID].UUIDString,
                           responseURL, contentDisposition, contentLength,
                           MIMEType, transition);
}

// Updates URL for navigation context and navigation item.
- (void)didReceiveRedirectForNavigation:(web::NavigationContextImpl*)context
                                withURL:(const GURL&)URL {
  context->SetUrl(URL);
  web::NavigationItemImpl* item =
      web::GetItemWithUniqueID(self.navigationManagerImpl, context);

  // Associated item can be a pending item, previously discarded by another
  // navigation. WKWebView allows multiple provisional navigations, while
  // Navigation Manager has only one pending navigation.
  if (item) {
    if (!IsWKInternalUrl(URL)) {
      item->SetVirtualURL(URL);
      item->SetURL(URL);
    }
    // Redirects (3xx response code), must change POST requests to GETs.
    item->SetPostData(nil);
    item->ResetHttpRequestHeaders();
  }

  self.userInteractionState->ResetLastTransferTime();
}

#pragma mark - Public methods

- (void)stopLoading {
  _stoppedWKNavigation = [self.navigationStates lastAddedNavigation];
  self.pendingNavigationInfo.cancelled = YES;
  _safeBrowsingWarningDetectionTimer.Stop();
  [self loadCancelled];
}

- (base::RepeatingTimer*)safeBrowsingWarningDetectionTimer {
  return &_safeBrowsingWarningDetectionTimer;
}

- (void)loadCancelled {
  // TODO(crbug.com/821995):  Check if this function should be removed.
  if (self.navigationState != web::WKNavigationState::FINISHED) {
    self.navigationState = web::WKNavigationState::FINISHED;
    if (![self.delegate navigationHandlerWebViewIsHalted:self]) {
      self.webStateImpl->SetIsLoading(false);
    }
  }
}

// Returns context for pending navigation that has |URL|. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL {
  // Here the enumeration variable |navigation| is __strong to allow setting it
  // to nil.
  for (__strong id navigation in [self.navigationStates pendingNavigations]) {
    if (navigation == [NSNull null]) {
      // null is a valid navigation object passed to WKNavigationDelegate
      // callbacks and represents window opening action.
      navigation = nil;
    }

    web::NavigationContextImpl* context =
        [self.navigationStates contextForNavigation:navigation];
    if (context && context->GetUrl() == URL) {
      return context;
    }
  }
  return nullptr;
}

- (BOOL)isCurrentNavigationBackForward {
  if (!self.currentNavItem)
    return NO;
  WKNavigationType currentNavigationType =
      self.currentBackForwardListItemHolder->navigation_type();
  return currentNavigationType == WKNavigationTypeBackForward;
}

- (BOOL)isCurrentNavigationItemPOST {
  // |self.navigationHandler.pendingNavigationInfo| will be nil if the
  // decidePolicy* delegate methods were not called.
  NSString* HTTPMethod =
      self.pendingNavigationInfo
          ? self.pendingNavigationInfo.HTTPMethod
          : self.currentBackForwardListItemHolder->http_method();
  if ([HTTPMethod isEqual:@"POST"]) {
    return YES;
  }
  if (!self.currentNavItem) {
    return NO;
  }
  return self.currentNavItem->HasPostData();
}

// Returns the WKBackForwardListItemHolder for the current navigation item.
- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder {
  web::NavigationItem* item = self.currentNavItem;
  DCHECK(item);
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  DCHECK(holder);
  return holder;
}

- (void)commitPendingNavigationInfoInWebView:(WKWebView*)webView {
  if (self.pendingNavigationInfo.referrer) {
    _currentReferrerString = [self.pendingNavigationInfo.referrer copy];
  }
  [self updateCurrentBackForwardListItemHolderInWebView:webView];

  self.pendingNavigationInfo = nil;
}

// Updates the WKBackForwardListItemHolder navigation item.
- (void)updateCurrentBackForwardListItemHolderInWebView:(WKWebView*)webView {
  if (!self.currentNavItem) {
    // TODO(crbug.com/925304): Pending item (which stores the holder) should be
    // owned by NavigationContext object. Pending item should never be null.
    return;
  }

  web::WKBackForwardListItemHolder* holder =
      self.currentBackForwardListItemHolder;

  WKNavigationType navigationType =
      self.pendingNavigationInfo ? self.pendingNavigationInfo.navigationType
                                 : WKNavigationTypeOther;
  holder->set_back_forward_list_item(webView.backForwardList.currentItem);
  holder->set_navigation_type(navigationType);
  holder->set_http_method(self.pendingNavigationInfo.HTTPMethod);

  // Only update the MIME type in the holder if there was MIME type information
  // as part of this pending load. It will be nil when doing a fast
  // back/forward navigation, for instance, because the callback that would
  // populate it is not called in that flow.
  if (self.pendingNavigationInfo.MIMEType)
    holder->set_mime_type(self.pendingNavigationInfo.MIMEType);
}

- (web::Referrer)currentReferrer {
  // Referrer string doesn't include the fragment, so in cases where the
  // previous URL is equal to the current referrer plus the fragment the
  // previous URL is returned as current referrer.
  NSString* referrerString = _currentReferrerString;

  // In case of an error evaluating the JavaScript simply return empty string.
  if (referrerString.length == 0)
    return web::Referrer();

  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  NSString* previousURLString = base::SysUTF8ToNSString(navigationURL.spec());
  // Check if the referrer is equal to the previous URL minus the hash symbol.
  // L'#' is used to convert the char '#' to a unichar.
  if ([previousURLString length] > referrerString.length &&
      [previousURLString hasPrefix:referrerString] &&
      [previousURLString characterAtIndex:referrerString.length] == L'#') {
    referrerString = previousURLString;
  }
  // Since referrer is being extracted from the destination page, the correct
  // policy from the origin has *already* been applied. Since the extracted URL
  // is the post-policy value, and the source policy is no longer available,
  // the policy is set to Always so that whatever WebKit decided to send will be
  // re-sent when replaying the entry.
  // TODO(crbug.com/227769): When possible, get the real referrer and policy in
  // advance and use that instead.
  return web::Referrer(GURL(base::SysNSStringToUTF8(referrerString)),
                       web::ReferrerPolicyAlways);
}

- (void)forgetNullWKNavigation:(WKNavigation*)navigation {
  if (!navigation)
    [self.navigationStates removeNavigation:navigation];
}

@end
