// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_

#include "content/public/browser/navigation_handle.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_throttle_runner.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/restore_type.h"
#include "net/base/ip_endpoint.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/navigation_handle_proxy.h"
#endif

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class AppCacheNavigationHandle;
class ChromeAppCacheService;
class NavigationUIData;
class NavigatorDelegate;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;
class SiteInstanceImpl;

// This class keeps track of a single navigation. It is created after the
// BeforeUnload for the navigation has run. It is then owned by the
// NavigationRequest until the navigation is ready to commit. The
// NavigationHandleImpl ownership is then transferred to the RenderFrameHost in
// which the navigation will commit. It is finaly destroyed when the navigation
// commits.
class CONTENT_EXPORT NavigationHandleImpl : public NavigationHandle,
                                            NavigationThrottleRunner::Delegate {
 public:
  ~NavigationHandleImpl() override;

  // Used to track the state the navigation is currently in.
  // Note: the states named PROCESSING_* indicate that NavigationThrottles are
  // currently processing the correspondant event. When they are done, the state
  // will move to the next in the list.
  // TODO(https://crbug.com/1377855): Remove the PROCESSING_* states once the
  // NavigationThrottleRunner is owned by the NavigationRequest.
  enum State {
    INITIAL = 0,
    PROCESSING_WILL_START_REQUEST,
    WILL_START_REQUEST,
    PROCESSING_WILL_REDIRECT_REQUEST,
    WILL_REDIRECT_REQUEST,
    PROCESSING_WILL_FAIL_REQUEST,
    WILL_FAIL_REQUEST,
    CANCELING,
    PROCESSING_WILL_PROCESS_RESPONSE,
    WILL_PROCESS_RESPONSE,
    READY_TO_COMMIT,
    DID_COMMIT,
    DID_COMMIT_ERROR_PAGE,
  };

  // NavigationHandle implementation:
  int64_t GetNavigationId() const override;
  const GURL& GetURL() override;
  SiteInstanceImpl* GetStartingSiteInstance() override;
  bool IsInMainFrame() override;
  bool IsParentMainFrame() override;
  bool IsRendererInitiated() override;
  bool WasServerRedirect() override;
  const std::vector<GURL>& GetRedirectChain() override;
  int GetFrameTreeNodeId() override;
  RenderFrameHostImpl* GetParentFrame() override;
  base::TimeTicks NavigationStart() override;
  base::TimeTicks NavigationInputStart() override;
  bool IsPost() override;
  const scoped_refptr<network::ResourceRequestBody>& GetResourceRequestBody()
      override;
  const Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  const NavigationUIData* GetNavigationUIData() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() override;
  bool IsSameDocument() override;
  bool HasCommitted() override;
  bool IsErrorPage() override;
  bool HasSubframeNavigationEntryCommitted() override;
  bool DidReplaceEntry() override;
  bool ShouldUpdateHistory() override;
  const GURL& GetPreviousURL() override;
  net::IPEndPoint GetSocketAddress() override;
  const net::HttpRequestHeaders& GetRequestHeaders() override;
  void RemoveRequestHeader(const std::string& header_name) override;
  void SetRequestHeader(const std::string& header_name,
                        const std::string& header_value) override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() override;
  const base::Optional<net::SSLInfo> GetSSLInfo() override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  bool IsDeferredForTesting() override;
  bool WasStartedFromContextMenu() const override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  ReloadType GetReloadType() override;
  RestoreType GetRestoreType() override;
  const GURL& GetBaseURLForDataURL() override;
  const GlobalRequestID& GetGlobalRequestID() override;
  bool IsDownload() override;
  bool IsFormSubmission() override;
  bool IsSignedExchangeInnerResponse() override;
  bool WasResponseCached() override;
  const net::ProxyServer& GetProxyServer() override;
  const std::string& GetHrefTranslate() override;
  const base::Optional<url::Origin>& GetInitiatorOrigin() override;

  // Returns the NavigationRequest which owns this NavigationHandle.
  NavigationRequest* navigation_request() { return navigation_request_; }

  const std::string& GetOriginPolicy() const;

  // Resume and CancelDeferredNavigation must only be called by the
  // NavigationThrottle that is currently deferring the navigation.
  // |resuming_throttle| and |cancelling_throttle| are the throttles calling
  // these methods.
  void Resume(NavigationThrottle* resuming_throttle);
  void CancelDeferredNavigation(NavigationThrottle* cancelling_throttle,
                                NavigationThrottle::ThrottleCheckResult result);

  // Simulates the navigation resuming. Most callers should just let the
  // deferring NavigationThrottle do the resuming.
  void CallResumeForTesting();

  NavigationData* GetNavigationData() override;
  void RegisterSubresourceOverride(
      mojom::TransferrableURLLoaderPtr transferrable_loader) override;

#if defined(OS_ANDROID)
  // Returns a reference to this NavigationHandle Java counterpart. It is used
  // by Java WebContentsObservers.
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_handle();
#endif

  // Used in tests.
  State state_for_testing() const { return state_; }

  // The NavigatorDelegate to notify/query for various navigation events.
  // Normally this is the WebContents, except if this NavigationHandle was
  // created during a navigation to an interstitial page. In this case it will
  // be the InterstitialPage itself.
  //
  // Note: due to the interstitial navigation case, all calls that can possibly
  // expose the NavigationHandle to code outside of content/ MUST go though the
  // NavigatorDelegate. In particular, the ContentBrowserClient should not be
  // called directly from the NavigationHandle code. Thus, these calls will not
  // expose the NavigationHandle when navigating to an InterstitialPage.
  NavigatorDelegate* GetDelegate() const;

  blink::mojom::RequestContextType request_context_type() const {
    DCHECK_GE(state_, PROCESSING_WILL_START_REQUEST);
    return navigation_request_->begin_params()->request_context_type;
  }

  blink::WebMixedContentContextType mixed_content_context_type() const {
    DCHECK_GE(state_, PROCESSING_WILL_START_REQUEST);
    return navigation_request_->begin_params()->mixed_content_context_type;
  }

  // Get the unique id from the NavigationEntry associated with this
  // NavigationHandle. Note that a synchronous, renderer-initiated navigation
  // will not have a NavigationEntry associated with it, and this will return 0.
  int pending_nav_entry_id() const { return pending_nav_entry_id_; }

  void set_net_error_code(net::Error net_error_code) {
    net_error_code_ = net_error_code;
  }

  void InitServiceWorkerHandle(
      ServiceWorkerContextWrapper* service_worker_context);
  ServiceWorkerNavigationHandle* service_worker_handle() const {
    return service_worker_handle_.get();
  }

  void InitAppCacheHandle(ChromeAppCacheService* appcache_service);
  AppCacheNavigationHandle* appcache_handle() const {
    return appcache_handle_.get();
  }

  typedef base::OnceCallback<void(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  // Called when the URLRequest will start in the network stack.  |callback|
  // will be called when all throttle checks have completed. This will allow
  // the caller to cancel the navigation or let it proceed.
  void WillStartRequest(ThrottleChecksFinishedCallback callback);

  // Updates the state of the navigation handle after encountering a server
  // redirect.
  void UpdateStateFollowingRedirect(
      const GURL& new_referrer_url,
      ThrottleChecksFinishedCallback callback);

  // Called when the URLRequest will be redirected in the network stack.
  // |callback| will be called when all throttles check have completed. This
  // will allow the caller to cancel the navigation or let it proceed.
  // This will also inform the delegate that the request was redirected.
  //
  // |post_redirect_process| is the renderer process we expect to
  // use to commit the navigation now that it has been redirected. It can be
  // null if there is no live process that can be used. In that case, a suitable
  // renderer process will be created at commit time.
  void WillRedirectRequest(
      const GURL& new_referrer_url,
      RenderProcessHost* post_redirect_process,
      ThrottleChecksFinishedCallback callback);

  // Called when the URLRequest will fail. |callback| will be
  // called when all throttles check have completed. This will allow the caller
  // to explicitly cancel the navigation (with a custom error code and/or
  // custom error page HTML) or let the failure proceed as normal.
  void WillFailRequest(ThrottleChecksFinishedCallback callback);

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // just before calling |callback|.
  void WillProcessResponse(
      ThrottleChecksFinishedCallback callback);

  // Returns the FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node() const {
    return navigation_request_->frame_tree_node();
  }

  // Called when the navigation is ready to be committed. This will update the
  // |state_| and inform the delegate.
  void ReadyToCommitNavigation(bool is_error);

  // Called when the navigation was committed. This will update the |state_|.
  // |navigation_entry_committed| indicates whether the navigation changed which
  // NavigationEntry is current.
  // |did_replace_entry| is true if the committed entry has replaced the
  // existing one. A non-user initiated redirect causes such replacement.
  void DidCommitNavigation(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool navigation_entry_committed,
      bool did_replace_entry,
      const GURL& previous_url,
      NavigationType navigation_type);

  // Called during commit. Takes ownership of the embedder's NavigationData
  // instance. This NavigationData may have been cloned prior to being added
  // here.
  void set_navigation_data(std::unique_ptr<NavigationData> navigation_data) {
    navigation_data_ = std::move(navigation_data);
  }

  NavigationUIData* navigation_ui_data() const {
    return navigation_ui_data_.get();
  }

  const GURL& base_url() { return base_url_; }

  NavigationType navigation_type() {
    DCHECK_GE(state_, DID_COMMIT);
    return navigation_type_;
  }

  void set_response_headers_for_testing(
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    response_headers_for_testing_ = response_headers;
  }

  void set_complete_callback_for_testing(
      ThrottleChecksFinishedCallback callback) {
    complete_callback_for_testing_ = std::move(callback);
  }

  CSPDisposition should_check_main_world_csp() const {
    return navigation_request_->common_params()
        .initiator_csp_info.should_check_main_world_csp;
  }

  const base::Optional<SourceLocation>& source_location() const {
    return navigation_request_->common_params().source_location;
  }

  void set_proxy_server(const net::ProxyServer& proxy_server) {
    proxy_server_ = proxy_server;
  }

  std::vector<std::string> TakeRemovedRequestHeaders() {
    return std::move(removed_request_headers_);
  }

  net::HttpRequestHeaders TakeModifiedRequestHeaders() {
    return std::move(modified_request_headers_);
  }

  NavigationThrottle* GetDeferringThrottleForTesting() const {
    return throttle_runner_.GetDeferringThrottle();
  }

  // Whether the navigation was sent to be committed in a renderer by the
  // RenderFrameHost. This can either be for the commit of a successful
  // navigation or an error page.
  bool IsWaitingToCommit();

  // Sets the READY_TO_COMMIT -> DID_COMMIT timeout.  Resets the timeout to the
  // default value if |timeout| is zero.
  static void SetCommitTimeoutForTesting(const base::TimeDelta& timeout);

 private:
  // TODO(clamy): Transform NavigationHandleImplTest into NavigationRequestTest
  // once NavigationHandleImpl has become a wrapper around NavigationRequest.
  // Then remove them from friends.
  friend class NavigationHandleImplTest;
  friend class NavigationRequest;

  // If |redirect_chain| is empty, then the redirect chain will be created to
  // start with |url|. Otherwise |redirect_chain| is used as the starting point.
  // |navigation_start| comes from the CommonNavigationParams associated with
  // this navigation.
  NavigationHandleImpl(NavigationRequest* navigation_request,
                       const std::vector<GURL>& redirect_chain,
                       int pending_nav_entry_id,
                       std::unique_ptr<NavigationUIData> navigation_ui_data,
                       net::HttpRequestHeaders request_headers,
                       const Referrer& sanitized_referrer);

  // NavigationThrottleRunner::Delegate:
  void OnNavigationEventProcessed(
      NavigationThrottleRunner::Event event,
      NavigationThrottle::ThrottleCheckResult result) override;

  void OnWillStartRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillRedirectRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillFailRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseProcessed(
      NavigationThrottle::ThrottleCheckResult result);

  void CancelDeferredNavigationInternal(
      NavigationThrottle::ThrottleCheckResult result);

  // Helper function to run and reset the |complete_callback_|. This marks the
  // end of a round of NavigationThrottleChecks.
  void RunCompleteCallback(NavigationThrottle::ThrottleCheckResult result);

  // Used in tests.
  State state() const { return state_; }

  // Checks for attempts to navigate to a page that is already referenced more
  // than once in the frame's ancestors.  This is a helper function used by
  // WillStartRequest and WillRedirectRequest to prevent the navigation.
  bool IsSelfReferentialURL();

  // Called if READY_TO_COMMIT -> COMMIT state transition takes an unusually
  // long time.
  void OnCommitTimeout();

  // Called by the RenderProcessHost to handle the case when the process
  // changed its state of being blocked.
  void RenderProcessBlockedStateChanged(bool blocked);

  void StopCommitTimeout();
  void RestartCommitTimeout();

  // The NavigationRequest that owns this NavigationHandle.
  NavigationRequest* navigation_request_;

  // See NavigationHandle for a description of those member variables.
  Referrer sanitized_referrer_;
  net::Error net_error_code_;
  bool was_redirected_;
  bool did_replace_entry_;
  bool should_update_history_;
  bool subframe_entry_committed_;

  // The headers used for the request.
  net::HttpRequestHeaders request_headers_;

  // Used to update the request's headers. When modified during the navigation
  // start, the headers will be applied to the initial network request. When
  // modified during a redirect, the headers will be applied to the redirected
  // request.
  std::vector<std::string> removed_request_headers_;
  net::HttpRequestHeaders modified_request_headers_;

  // The state the navigation is in.
  State state_;

  // The time this naviagtion was ready to commit.
  base::TimeTicks ready_to_commit_time_;

  // Timer for detecting an unexpectedly long time to commit a navigation.
  base::OneShotTimer commit_timeout_timer_;

  // The subscription to the notification of the changing of the render
  // process's blocked state.
  std::unique_ptr<base::CallbackList<void(bool)>::Subscription>
      render_process_blocked_state_changed_subscription_;

  // The unique id of the corresponding NavigationEntry.
  int pending_nav_entry_id_;

  // This callback will be run when all throttle checks have been performed. Be
  // careful about relying on it as the member may be removed as part of the
  // PlzNavigate refactoring.
  ThrottleChecksFinishedCallback complete_callback_;

  // This test-only callback will be run when all throttle checks have been
  // performed.
  // TODO(clamy): Revisit the unit test architecture when PlzNavigate ships.
  ThrottleChecksFinishedCallback complete_callback_for_testing_;

  // Manages the lifetime of a pre-created ServiceWorkerProviderHost until a
  // corresponding provider is created in the renderer.
  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  // Manages the lifetime of a pre-created AppCacheHost until a browser side
  // navigation is ready to be committed, i.e we have a renderer process ready
  // to service the navigation request.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  // Embedder data from the IO thread tied to this navigation.
  std::unique_ptr<NavigationData> navigation_data_;

  // Embedder data from the UI thread tied to this navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // The unique id to identify this to navigation with.
  int64_t navigation_id_;

  // The chain of redirects.
  std::vector<GURL> redirect_chain_;

  // Stores the reload type, or NONE if it's not a reload.
  ReloadType reload_type_;

  // Stores the restore type, or NONE it it's not a restore.
  RestoreType restore_type_;

  GURL previous_url_;
  GURL base_url_;
  NavigationType navigation_type_;

  // Which proxy server was used for this navigation, if any.
  net::ProxyServer proxy_server_;

  // Set in ReadyToCommitNavigation.
  bool is_same_process_;

  // Allows to override response_headers_ in tests.
  // TODO(clamy): Clean this up once the architecture of unit tests is better.
  scoped_refptr<net::HttpResponseHeaders> response_headers_for_testing_;

  // Owns the NavigationThrottles associated with this navigation, and is
  // responsible for notifying them about the various navigation events.
  NavigationThrottleRunner throttle_runner_;

#if defined(OS_ANDROID)
  // For each C++ NavigationHandle, there is a Java counterpart. It is the JNI
  // bridge in between the two.
  std::unique_ptr<NavigationHandleProxy> navigation_handle_proxy_;
#endif

  base::WeakPtrFactory<NavigationHandleImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
