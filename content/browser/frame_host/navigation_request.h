// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/previews_state.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace content {

class FrameNavigationEntry;
class FrameTreeNode;
class NavigationControllerImpl;
class NavigationHandleImpl;
class NavigationURLLoader;
class NavigationData;
class ResourceRequestBody;
class SiteInstanceImpl;
class StreamHandle;

// PlzNavigate
// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class CONTENT_EXPORT NavigationRequest : public NavigationURLLoaderDelegate {
 public:
  // Keeps track of the various stages of a NavigationRequest.
  enum NavigationState {
    // Initial state.
    NOT_STARTED = 0,

    // Waiting for a BeginNavigation IPC from the renderer in a
    // browser-initiated navigation. If there is no live renderer when the
    // request is created, this stage is skipped.
    WAITING_FOR_RENDERER_RESPONSE,

    // The request was sent to the IO thread.
    STARTED,

    // The response started on the IO thread and is ready to be committed. This
    // is one of the two final states for the request.
    RESPONSE_STARTED,

    // The request failed on the IO thread and an error page should be
    // displayed. This is one of the two final states for the request.
    FAILED,
  };

  // The SiteInstance currently associated with the navigation. Note that the
  // final value will only be known when the response is received, or the
  // navigation fails, as server redirects can modify the SiteInstance to use
  // for the navigation.
  enum class AssociatedSiteInstanceType {
    NONE = 0,
    CURRENT,
    SPECULATIVE,
  };

  // Creates a request for a browser-intiated navigation.
  static std::unique_ptr<NavigationRequest> CreateBrowserInitiated(
      FrameTreeNode* frame_tree_node,
      const GURL& dest_url,
      const Referrer& dest_referrer,
      const FrameNavigationEntry& frame_entry,
      const NavigationEntryImpl& entry,
      FrameMsg_Navigate_Type::Value navigation_type,
      PreviewsState previews_state,
      bool is_same_document_history_load,
      bool is_history_navigation_in_new_child,
      const scoped_refptr<ResourceRequestBody>& post_body,
      const base::TimeTicks& navigation_start,
      NavigationControllerImpl* controller);

  // Creates a request for a renderer-intiated navigation.
  // Note: |body| is sent to the IO thread when calling BeginNavigation, and
  // should no longer be manipulated afterwards on the UI thread.
  // TODO(clamy): see if ResourceRequestBody could be un-refcounted to avoid
  // threading subtleties.
  static std::unique_ptr<NavigationRequest> CreateRendererInitiated(
      FrameTreeNode* frame_tree_node,
      NavigationEntryImpl* entry,
      const CommonNavigationParams& common_params,
      const BeginNavigationParams& begin_params,
      int current_history_list_offset,
      int current_history_list_length,
      bool override_user_agent);

  ~NavigationRequest() override;

  // Called on the UI thread by the Navigator to start the navigation.
  void BeginNavigation();

  const CommonNavigationParams& common_params() const { return common_params_; }

  const BeginNavigationParams& begin_params() const { return begin_params_; }

  const RequestNavigationParams& request_params() const {
    return request_params_;
  }

  // Updates the navigation start time.
  void set_navigation_start_time(const base::TimeTicks& time) {
    common_params_.navigation_start = time;
  }

  NavigationURLLoader* loader_for_testing() const { return loader_.get(); }

  NavigationState state() const { return state_; }

  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  SiteInstanceImpl* source_site_instance() const {
    return source_site_instance_.get();
  }

  SiteInstanceImpl* dest_site_instance() const {
    return dest_site_instance_.get();
  }

  RestoreType restore_type() const { return restore_type_; };

  bool is_view_source() const { return is_view_source_; };

  int bindings() const { return bindings_; };

  bool browser_initiated() const { return browser_initiated_ ; }

  bool from_begin_navigation() const { return from_begin_navigation_; }

  AssociatedSiteInstanceType associated_site_instance_type() const {
    return associated_site_instance_type_;
  }
  void set_associated_site_instance_type(AssociatedSiteInstanceType type) {
    associated_site_instance_type_ = type;
  }

  NavigationHandleImpl* navigation_handle() const {
    return navigation_handle_.get();
  }

  void SetWaitingForRendererResponse();

  // Creates a NavigationHandle. This should be called after any previous
  // NavigationRequest for the FrameTreeNode has been destroyed.
  void CreateNavigationHandle();

  // Transfers the ownership of the NavigationHandle to |render_frame_host|.
  // This should be called when the navigation is ready to commit, because the
  // NavigationHandle outlives the NavigationRequest. The NavigationHandle's
  // lifetime is the entire navigation, while the NavigationRequest is
  // destroyed when a navigation is ready for commit.
  void TransferNavigationHandleOwnership(
      RenderFrameHostImpl* render_frame_host);

  void set_on_start_checks_complete_closure_for_testing(
      const base::Closure& closure) {
    on_start_checks_complete_closure_ = closure;
  }

  int nav_entry_id() const { return nav_entry_id_; }

 private:
  // This enum describes the result of a Content Security Policy (CSP) check for
  // the request.
  enum ContentSecurityPolicyCheckResult {
    // The request should be allowed to continue. PASSED could mean that the
    // request did not violate any CSP, or that it violated a report-only CSP.
    CONTENT_SECURITY_POLICY_CHECK_PASSED,
    // The request should be blocked because it violated an enforced CSP.
    CONTENT_SECURITY_POLICY_CHECK_FAILED,
  };

  NavigationRequest(FrameTreeNode* frame_tree_node,
                    const CommonNavigationParams& common_params,
                    const BeginNavigationParams& begin_params,
                    const RequestNavigationParams& request_params,
                    bool browser_initiated,
                    bool from_begin_navigation,
                    const FrameNavigationEntry* frame_navigation_entry,
                    const NavigationEntryImpl* navitation_entry);

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) override;
  void OnResponseStarted(const scoped_refptr<ResourceResponse>& response,
                         std::unique_ptr<StreamHandle> body,
                         mojo::ScopedDataPipeConsumerHandle consumer_handle,
                         const SSLStatus& ssl_status,
                         std::unique_ptr<NavigationData> navigation_data,
                         const GlobalRequestID& request_id,
                         bool is_download,
                         bool is_stream,
                         mojom::URLLoaderFactoryPtrInfo
                             subresource_url_loader_factory_info) override;
  void OnRequestFailed(bool has_stale_copy_in_cache,
                       int net_error,
                       const base::Optional<net::SSLInfo>& ssl_info,
                       bool should_ssl_errors_be_fatal) override;
  void OnRequestStarted(base::TimeTicks timestamp) override;

  // Called when the NavigationThrottles have been checked by the
  // NavigationHandle.
  void OnStartChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnRedirectChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseChecksComplete(
      NavigationThrottle::ThrottleCheckResult result);

  // Have a RenderFrameHost commit the navigation. The NavigationRequest will
  // be destroyed after this call.
  void CommitNavigation();

  // Check whether a request should be allowed to continue or should be blocked
  // because it violates a CSP. This method can have two side effects:
  // - If a CSP is configured to send reports and the request violates the CSP,
  //   a report will be sent.
  // - The navigation request may be upgraded from HTTP to HTTPS if a CSP is
  //   configured to upgrade insecure requests.
  ContentSecurityPolicyCheckResult CheckContentSecurityPolicyFrameSrc(
      bool is_redirect);

  // This enum describes the result of the credentialed subresource check for
  // the request.
  enum class CredentialedSubresourceCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };

  // Chrome blocks subresource requests whose URLs contain embedded credentials
  // (e.g. `https://user:pass@example.com/page.html`). Check whether the
  // request should be allowed to continue or should be blocked.
  CredentialedSubresourceCheckResult CheckCredentialedSubresource() const;

  // This enum describes the result of the legacy protocol check for
  // the request.
  enum class LegacyProtocolInSubresourceCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };

  // Block subresources requests that target "legacy" protocol (like "ftp") when
  // the main document is not served from a "legacy" protocol.
  LegacyProtocolInSubresourceCheckResult CheckLegacyProtocolInSubresource()
      const;

  FrameTreeNode* frame_tree_node_;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  // Note: |common_params_| and |begin_params_| are not const as they can be
  // modified during redirects.
  // Note: |request_params_| is not const because service_worker_provider_id
  // and should_create_service_worker will be set in OnResponseStarted.
  CommonNavigationParams common_params_;
  BeginNavigationParams begin_params_;
  RequestNavigationParams request_params_;
  const bool browser_initiated_;

  NavigationState state_;

  std::unique_ptr<NavigationURLLoader> loader_;

  // These next items are used in browser-initiated navigations to store
  // information from the NavigationEntryImpl that is required after request
  // creation time.
  scoped_refptr<SiteInstanceImpl> source_site_instance_;
  scoped_refptr<SiteInstanceImpl> dest_site_instance_;
  RestoreType restore_type_;
  bool is_view_source_;
  int bindings_;
  int nav_entry_id_ = 0;

  // Whether the navigation should be sent to a renderer a process. This is
  // true, except for 204/205 responses and downloads.
  bool response_should_be_rendered_;

  // The type of SiteInstance associated with this navigation.
  AssociatedSiteInstanceType associated_site_instance_type_;

  // Stores the SiteInstance created on redirects to check if there is an
  // existing RenderProcessHost that can commit the navigation so that the
  // renderer process is not deleted while the navigation is ongoing. If the
  // SiteInstance was a brand new SiteInstance, it is not stored.
  scoped_refptr<SiteInstance> speculative_site_instance_;

  // Whether the NavigationRequest was created after receiving a BeginNavigation
  // IPC. When true, main frame navigations should not commit in a different
  // process (unless asked by the content/ embedder). When true, the renderer
  // process expects to be notified if the navigation is aborted.
  bool from_begin_navigation_;

  std::unique_ptr<NavigationHandleImpl> navigation_handle_;

  // Holds the ResourceResponse and the StreamHandle (or
  // DataPipeConsumerHandle) for the navigation while the WillProcessResponse
  // checks are performed by the NavigationHandle.
  scoped_refptr<ResourceResponse> response_;
  std::unique_ptr<StreamHandle> body_;
  mojo::ScopedDataPipeConsumerHandle handle_;
  SSLStatus ssl_status_;
  bool is_download_;

  base::Closure on_start_checks_complete_closure_;

  // Used in the network service world to pass the subressource loader factory
  // to the renderer. Currently only used by AppCache.
  mojom::URLLoaderFactoryPtrInfo subresource_loader_factory_info_;

  base::WeakPtrFactory<NavigationRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
