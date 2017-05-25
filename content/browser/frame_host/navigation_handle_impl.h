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
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/request_context_type.h"
#include "third_party/WebKit/public/platform/WebMixedContentContextType.h"
#include "url/gurl.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class AppCacheNavigationHandle;
class ChromeAppCacheService;
class NavigationUIData;
class NavigatorDelegate;
class ResourceRequestBodyImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;

// This class keeps track of a single navigation. It is created upon receipt of
// a DidStartProvisionalLoad IPC in a RenderFrameHost. The RenderFrameHost owns
// the newly created NavigationHandleImpl as long as the navigation is ongoing.
// The NavigationHandleImpl in the RenderFrameHost will be reset when the
// navigation stops, that is if one of the following events happen:
//   - The RenderFrameHost receives a DidStartProvisionalLoad IPC for a new
//   navigation (see below for special cases where the DidStartProvisionalLoad
//   message does not indicate the start of a new navigation).
//   - The RenderFrameHost stops loading.
//   - The RenderFrameHost receives a DidDropNavigation IPC.
//
// When the navigation encounters an error, the DidStartProvisionalLoad marking
// the start of the load of the error page will not be considered as marking a
// new navigation. It will not reset the NavigationHandleImpl in the
// RenderFrameHost.
//
// If the navigation needs a cross-site transfer, then the NavigationHandleImpl
// will briefly be held by the RenderFrameHostManager, until a suitable
// RenderFrameHost for the navigation has been found. The ownership of the
// NavigationHandleImpl will then be transferred to the new RenderFrameHost.
// The DidStartProvisionalLoad received by the new RenderFrameHost for the
// transferring navigation will not reset the NavigationHandleImpl, as it does
// not mark the start of a new navigation.
//
// PlzNavigate: the NavigationHandleImpl is created just after creating a new
// NavigationRequest. It is then owned by the NavigationRequest until the
// navigation is ready to commit. The NavigationHandleImpl ownership is then
// transferred to the RenderFrameHost in which the navigation will commit.
//
// When PlzNavigate is enabled, the NavigationHandleImpl will never be reset
// following the receipt of a DidStartProvisionalLoad IPC. There are also no
// transferring navigations. The other causes of NavigationHandleImpl reset in
// the RenderFrameHost still apply.
class CONTENT_EXPORT NavigationHandleImpl : public NavigationHandle {
 public:
  // If |redirect_chain| is empty, then the redirect chain will be created to
  // start with |url|. Otherwise |redirect_chain| is used as the starting point.
  // |navigation_start| comes from the DidStartProvisionalLoad IPC, which tracks
  // both renderer-initiated and browser-initiated navigation start.
  // PlzNavigate: This value always comes from the CommonNavigationParams
  // associated with this navigation.
  static std::unique_ptr<NavigationHandleImpl> Create(
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      FrameTreeNode* frame_tree_node,
      bool is_renderer_initiated,
      bool is_same_page,
      const base::TimeTicks& navigation_start,
      int pending_nav_entry_id,
      bool started_from_context_menu,
      CSPDisposition should_check_main_world_csp,
      bool is_form_submission);
  ~NavigationHandleImpl() override;

  // Used to track the state the navigation is currently in.
  enum State {
    INITIAL = 0,
    WILL_SEND_REQUEST,
    DEFERRING_START,
    WILL_REDIRECT_REQUEST,
    DEFERRING_REDIRECT,
    CANCELING,
    WILL_PROCESS_RESPONSE,
    DEFERRING_RESPONSE,
    READY_TO_COMMIT,
    DID_COMMIT,
    DID_COMMIT_ERROR_PAGE,
  };

  // NavigationHandle implementation:
  const GURL& GetURL() override;
  SiteInstance* GetStartingSiteInstance() override;
  bool IsInMainFrame() override;
  bool IsParentMainFrame() override;
  bool IsRendererInitiated() override;
  bool WasServerRedirect() override;
  const std::vector<GURL>& GetRedirectChain() override;
  int GetFrameTreeNodeId() override;
  RenderFrameHostImpl* GetParentFrame() override;
  const base::TimeTicks& NavigationStart() override;
  bool IsPost() override;
  const Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
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
  net::HostPortPair GetSocketAddress() override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() override;
  void Resume() override;
  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  NavigationThrottle::ThrottleCheckResult CallWillStartRequestForTesting(
      bool is_post,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol) override;
  NavigationThrottle::ThrottleCheckResult CallWillRedirectRequestForTesting(
      const GURL& new_url,
      bool new_method_is_post,
      const GURL& new_referrer_url,
      bool new_is_external_protocol) override;
  NavigationThrottle::ThrottleCheckResult CallWillProcessResponseForTesting(
      RenderFrameHost* render_frame_host,
      const std::string& raw_response_header) override;
  void CallDidCommitNavigationForTesting(const GURL& url) override;
  bool WasStartedFromContextMenu() const override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  ReloadType GetReloadType() override;
  RestoreType GetRestoreType() override;
  const GURL& GetBaseURLForDataURL() override;
  const GlobalRequestID& GetGlobalRequestID() override;

  NavigationData* GetNavigationData() override;

  // Used in tests.
  State state_for_testing() const { return state_; }

  // Whether or not the navigation has been initiated by a form submission.
  // TODO(arthursonzogni): This value is correct only when PlzNavigate is
  // enabled. Make it work in both modes.
  bool is_form_submission() const { return is_form_submission_; }

  // Whether the navigation request is a download. This is useful when the
  // navigation hasn't committed yet, in which case HasCommitted() will return
  // false even if the navigation request is not a download.
  bool is_download() const { return is_download_; }

  // The NavigatorDelegate to notify/query for various navigation events.
  // Normally this is the WebContents, except if this NavigationHandle was
  // created during a navigation to an interstitial page. In this case it will
  // be the InterstitialPage itself.
  //
  // Note: due to the interstitial navigation case, all calls that can possibly
  // expose the NavigationHandle to code outside of content/ MUST go though the
  // NavigatorDelegate. In particular, the ContentBrowserClient should not be
  // called directly form the NavigationHandle code. Thus, these calls will not
  // expose the NavigationHandle when navigating to an InterstialPage.
  NavigatorDelegate* GetDelegate() const;

  RequestContextType request_context_type() const {
    DCHECK_GE(state_, WILL_SEND_REQUEST);
    return request_context_type_;
  }

  blink::WebMixedContentContextType mixed_content_context_type() const {
    DCHECK_GE(state_, WILL_SEND_REQUEST);
    return mixed_content_context_type_;
  }

  // Get the unique id from the NavigationEntry associated with this
  // NavigationHandle. Note that a synchronous, renderer-initiated navigation
  // will not have a NavigationEntry associated with it, and this will return 0.
  int pending_nav_entry_id() const { return pending_nav_entry_id_; }

  // Changes the pending NavigationEntry ID for this handle.  This is currently
  // required during transfer navigations.
  // TODO(creis): Remove this when transfer navigations do not require pending
  // entries.  See https://crbug.com/495161.
  void update_entry_id_for_transfer(int nav_entry_id) {
    pending_nav_entry_id_ = nav_entry_id;
  }

  void set_net_error_code(net::Error net_error_code) {
    net_error_code_ = net_error_code;
  }

  // Returns whether the navigation is currently being transferred from one
  // RenderFrameHost to another. In particular, a DidStartProvisionalLoad IPC
  // for the navigation URL, received in the new RenderFrameHost, should not
  // indicate the start of a new navigation in that case.
  bool is_transferring() const { return is_transferring_; }
  void set_is_transferring(bool is_transferring) {
    is_transferring_ = is_transferring;
  }

  // Updates the RenderFrameHost that is about to commit the navigation. This
  // is used during transfer navigations.
  void set_render_frame_host(RenderFrameHostImpl* render_frame_host) {
    render_frame_host_ = render_frame_host;
  }

  // Returns the POST body associated with this navigation.  This will be
  // null for GET and/or other non-POST requests (or if a response to a POST
  // request was a redirect that changed the method to GET - for example 302).
  const scoped_refptr<ResourceRequestBodyImpl>& resource_request_body() const {
    return resource_request_body_;
  }

  // PlzNavigate
  void InitServiceWorkerHandle(
      ServiceWorkerContextWrapper* service_worker_context);
  ServiceWorkerNavigationHandle* service_worker_handle() const {
    return service_worker_handle_.get();
  }

  // PlzNavigate
  void InitAppCacheHandle(ChromeAppCacheService* appcache_service);
  AppCacheNavigationHandle* appcache_handle() const {
    return appcache_handle_.get();
  }

  typedef base::Callback<void(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  // Called when the URLRequest will start in the network stack.  |callback|
  // will be called when all throttle checks have completed. This will allow
  // the caller to cancel the navigation or let it proceed.
  void WillStartRequest(
      const std::string& method,
      scoped_refptr<content::ResourceRequestBodyImpl> resource_request_body,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol,
      RequestContextType request_context_type,
      blink::WebMixedContentContextType mixed_content_context_type,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest will be redirected in the network stack.
  // |callback| will be called when all throttles check have completed. This
  // will allow the caller to cancel the navigation or let it proceed.
  // This will also inform the delegate that the request was redirected.
  void WillRedirectRequest(
      const GURL& new_url,
      const std::string& new_method,
      const GURL& new_referrer_url,
      bool new_is_external_protocol,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // with |render_frame_host| and |response_headers| just before calling
  // |callback|. Should a transfer navigation happen, |transfer_callback| will
  // be run on the IO thread.
  // PlzNavigate: transfer navigations are not possible.
  void WillProcessResponse(
      RenderFrameHostImpl* render_frame_host,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      const SSLStatus& ssl_status,
      const GlobalRequestID& request_id,
      bool should_replace_current_entry,
      bool is_download,
      bool is_stream,
      const base::Closure& transfer_callback,
      const ThrottleChecksFinishedCallback& callback);

  // Returns the FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // Called when the navigation is ready to be committed in
  // |render_frame_host|. This will update the |state_| and inform the
  // delegate.
  void ReadyToCommitNavigation(RenderFrameHostImpl* render_frame_host);

  // Called when the navigation was committed in |render_frame_host|. This will
  // update the |state_|.
  // |navigation_entry_committed| indicates whether the navigation changed which
  // NavigationEntry is current.
  // |did_replace_entry| is true if the committed entry has replaced the
  // existing one. A non-user initiated redirect causes such replacement.
  void DidCommitNavigation(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool navigation_entry_committed,
      bool did_replace_entry,
      const GURL& previous_url,
      NavigationType navigation_type,
      RenderFrameHostImpl* render_frame_host);

  // Called during commit. Takes ownership of the embedder's NavigationData
  // instance. This NavigationData may have been cloned prior to being added
  // here.
  void set_navigation_data(std::unique_ptr<NavigationData> navigation_data) {
    navigation_data_ = std::move(navigation_data);
  }

  SSLStatus ssl_status() { return ssl_status_; }

  // Called when the navigation is transferred to a different renderer.
  void Transfer();

  NavigationUIData* navigation_ui_data() const {
    return navigation_ui_data_.get();
  }

  const GURL& base_url() { return base_url_; }

  void set_searchable_form_url(const GURL& url) { searchable_form_url_ = url; }
  void set_searchable_form_encoding(const std::string& encoding) {
    searchable_form_encoding_ = encoding;
  }

  NavigationType navigation_type() {
    DCHECK_GE(state_, DID_COMMIT);
    return navigation_type_;
  }

  void set_response_headers_for_testing(
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    response_headers_ = response_headers;
  }

  void set_complete_callback_for_testing(
      const ThrottleChecksFinishedCallback& callback) {
    complete_callback_for_testing_ = callback;
  }

  CSPDisposition should_check_main_world_csp() const {
    return should_check_main_world_csp_;
  }

  const SourceLocation& source_location() const { return source_location_; }
  void set_source_location(const SourceLocation& source_location) {
    source_location_ = source_location;
  }

  // PlzNavigate
  // Sets ID of the RenderProcessHost we expect the navigation to commit in.
  // This is used to inform the RenderProcessHost to expect a navigation to the
  // url we're navigating to.
  void SetExpectedProcess(RenderProcessHost* expected_process);

 private:
  friend class NavigationHandleImplTest;

  NavigationHandleImpl(const GURL& url,
                       const std::vector<GURL>& redirect_chain,
                       FrameTreeNode* frame_tree_node,
                       bool is_renderer_initiated,
                       bool is_same_page,
                       const base::TimeTicks& navigation_start,
                       int pending_nav_entry_id,
                       bool started_from_context_menu,
                       CSPDisposition should_check_main_world_csp,
                       bool is_form_submission);

  NavigationThrottle::ThrottleCheckResult CheckWillStartRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillRedirectRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillProcessResponse();

  // Called when WillProcessResponse checks are done, to find the final
  // RenderFrameHost for the navigation. Checks whether the navigation should be
  // transferred. Returns false if the transfer attempt results in the
  // destruction of this NavigationHandle and the navigation should no longer
  // proceed. This can happen when the RenderFrameHostManager determines a
  // transfer is needed, but WebContentsDelegate::ShouldTransferNavigation
  // returns false.
  bool MaybeTransferAndProceed();

  // Helper method for MaybeTransferAndProceed. Returns false if the transfer
  // attempt results in the destruction of this NavigationHandle.
  bool MaybeTransferAndProceedInternal();

  // Helper function to run and reset the |complete_callback_|. This marks the
  // end of a round of NavigationThrottleChecks.
  void RunCompleteCallback(NavigationThrottle::ThrottleCheckResult result);

  // Used in tests.
  State state() const { return state_; }

  // Populates |throttles_| with the throttles for this navigation.
  void RegisterNavigationThrottles();

  // Checks for attempts to navigate to a page that is already referenced more
  // than once in the frame's ancestors.  This is a helper function used by
  // WillStartRequest and WillRedirectRequest to prevent the navigation.
  bool IsSelfReferentialURL();

  // Updates the destination site URL for this navigation. This is called on
  // redirects.
  // PlzNavigate: When redirected cross-site, the speculative RenderProcessHost
  // will stop expecting this navigation to commit.
  void UpdateSiteURL();

  // See NavigationHandle for a description of those member variables.
  GURL url_;
  scoped_refptr<SiteInstance> starting_site_instance_;
  Referrer sanitized_referrer_;
  bool has_user_gesture_;
  ui::PageTransition transition_;
  bool is_external_protocol_;
  net::Error net_error_code_;
  RenderFrameHostImpl* render_frame_host_;
  const bool is_renderer_initiated_;
  const bool is_same_page_;
  bool was_redirected_;
  bool did_replace_entry_;
  bool should_update_history_;
  bool subframe_entry_committed_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  net::HttpResponseInfo::ConnectionInfo connection_info_;

  // The original url of the navigation. This may differ from |url_| if the
  // navigation encounters redirects.
  const GURL original_url_;

  // The site URL of this navigation, as obtained from SiteInstance::GetSiteURL.
  GURL site_url_;

  // The HTTP method used for the navigation.
  std::string method_;

  // The POST body associated with this navigation.  This will be null for GET
  // and/or other non-POST requests (or if a response to a POST request was a
  // redirect that changed the method to GET - for example 302).
  scoped_refptr<ResourceRequestBodyImpl> resource_request_body_;

  // The state the navigation is in.
  State state_;

  // Whether the navigation is in the middle of a transfer. Set to false when
  // the DidStartProvisionalLoad is received from the new renderer.
  bool is_transferring_;

  // The FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node_;

  // A list of Throttles registered for this navigation.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;

  // The index of the next throttle to check.
  size_t next_index_;

  // The time this navigation started.
  const base::TimeTicks navigation_start_;

  // The unique id of the corresponding NavigationEntry.
  int pending_nav_entry_id_;

  // The fetch request context type.
  RequestContextType request_context_type_;

  // The mixed content context type for potential mixed content checks.
  blink::WebMixedContentContextType mixed_content_context_type_;

  // This callback will be run when all throttle checks have been performed. Be
  // careful about relying on it as the member may be removed as part of the
  // PlzNavigate refactoring.
  ThrottleChecksFinishedCallback complete_callback_;

  // This test-only callback will be run when all throttle checks have been
  // performed.
  // TODO(clamy): Revisit the unit test architecture when PlzNavigate ships.
  ThrottleChecksFinishedCallback complete_callback_for_testing_;

  // PlzNavigate
  // Manages the lifetime of a pre-created ServiceWorkerProviderHost until a
  // corresponding ServiceWorkerNetworkProvider is created in the renderer.
  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  // PlzNavigate
  // Manages the lifetime of a pre-created AppCacheHost until a browser side
  // navigation is ready to be committed, i.e we have a renderer process ready
  // to service the navigation request.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  // Embedder data from the IO thread tied to this navigation.
  std::unique_ptr<NavigationData> navigation_data_;

  // PlzNavigate
  // Embedder data from the UI thread tied to this navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  SSLStatus ssl_status_;

  // The id of the URLRequest tied to this navigation.
  GlobalRequestID request_id_;

  // Whether the current NavigationEntry should be replaced upon commit.
  bool should_replace_current_entry_;

  // The chain of redirects.
  std::vector<GURL> redirect_chain_;

  // A callback to run on the IO thread if the navigation transfers.
  base::Closure transfer_callback_;

  // Whether the navigation ended up being a download or a stream.
  bool is_download_;
  bool is_stream_;

  // False by default unless the navigation started within a context menu.
  bool started_from_context_menu_;

  // Stores the reload type, or NONE if it's not a reload.
  ReloadType reload_type_;

  // Stores the restore type, or NONE it it's not a restore.
  RestoreType restore_type_;

  GURL searchable_form_url_;
  std::string searchable_form_encoding_;

  GURL previous_url_;
  GURL base_url_;
  GURL base_url_for_data_url_;
  net::HostPortPair socket_address_;
  NavigationType navigation_type_;

  // Whether or not the CSP of the main world should apply. When the navigation
  // is initiated from a content script in an isolated world, the CSP defined
  // in the main world should not apply.
  CSPDisposition should_check_main_world_csp_;

  // Whether or not the navigation results from the submission of a form.
  bool is_form_submission_;

  // PlzNavigate
  // Information about the JavaScript that started the navigation. For
  // navigations initiated by Javascript.
  SourceLocation source_location_;

  // PlzNavigate
  // Used to inform a RenderProcessHost that we expect this navigation to commit
  // in it.
  int expected_render_process_host_id_;

  base::WeakPtrFactory<NavigationHandleImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
