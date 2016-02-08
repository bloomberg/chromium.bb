// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_

#include "content/public/browser/navigation_handle.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class NavigatorDelegate;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;
struct NavigationRequestInfo;

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
  // |navigation_start| comes from the DidStartProvisionalLoad IPC, which tracks
  // both renderer-initiated and browser-initiated navigation start.
  // PlzNavigate: This value always comes from the CommonNavigationParams
  // associated with this navigation.
  static scoped_ptr<NavigationHandleImpl> Create(
      const GURL& url,
      FrameTreeNode* frame_tree_node,
      const base::TimeTicks& navigation_start);
  ~NavigationHandleImpl() override;

  // NavigationHandle implementation:
  const GURL& GetURL() override;
  bool IsInMainFrame() override;
  const base::TimeTicks& NavigationStart() override;
  bool IsPost() override;
  const Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() override;
  bool IsSamePage() override;
  bool HasCommitted() override;
  bool IsErrorPage() override;
  void Resume() override;
  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) override;
  void RegisterThrottleForTesting(
      scoped_ptr<NavigationThrottle> navigation_throttle) override;
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

  NavigatorDelegate* GetDelegate() const;

  // Returns the response headers for the request. This can only be accessed
  // after a redirect was encountered or after the the navigation is ready to
  // commit. It should not be modified, as modifications will not be reflected
  // in the network stack.
  const net::HttpResponseHeaders* GetResponseHeaders();

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

  // PlzNavigate
  void InitServiceWorkerHandle(
      ServiceWorkerContextWrapper* service_worker_context);
  ServiceWorkerNavigationHandle* service_worker_handle() const {
    return service_worker_handle_.get();
  }

  typedef base::Callback<void(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  // Called when the URLRequest will start in the network stack.  |callback|
  // will be called when all throttle checks have completed. This will allow
  // the caller to cancel the navigation or let it proceed.
  void WillStartRequest(bool is_post,
                        const Referrer& sanitized_referrer,
                        bool has_user_gesture,
                        ui::PageTransition transition,
                        bool is_external_protocol,
                        const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest will be redirected in the network stack.
  // |callback| will be called when all throttles check have completed. This
  // will allow the caller to cancel the navigation or let it proceed.
  // This will also inform the delegate that the request was redirected.
  void WillRedirectRequest(
      const GURL& new_url,
      bool new_method_is_post,
      const GURL& new_referrer_url,
      bool new_is_external_protocol,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // with |render_frame_host| and |response_headers| just before calling
  // |callback|.
  void WillProcessResponse(
      RenderFrameHostImpl* render_frame_host,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const ThrottleChecksFinishedCallback& callback);

  // Returns the FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // Called when the navigation is ready to be committed in
  // |render_frame_host|. This will update the |state_| and inform the
  // delegate.
  void ReadyToCommitNavigation(
      RenderFrameHostImpl* render_frame_host,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Called when the navigation was committed in |render_frame_host|. This will
  // update the |state_|.
  void DidCommitNavigation(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool same_page,
      RenderFrameHostImpl* render_frame_host);

 private:
  friend class NavigationHandleImplTest;

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

  NavigationHandleImpl(const GURL& url,
                       FrameTreeNode* frame_tree_node,
                       const base::TimeTicks& navigation_start);

  NavigationThrottle::ThrottleCheckResult CheckWillStartRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillRedirectRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillProcessResponse();

  // Helper function to run and reset the |complete_callback_|. This marks the
  // end of a round of NavigationThrottleChecks.
  void RunCompleteCallback(NavigationThrottle::ThrottleCheckResult result);

  // Used in tests.
  State state() const { return state_; }

  // See NavigationHandle for a description of those member variables.
  GURL url_;
  bool is_post_;
  Referrer sanitized_referrer_;
  bool has_user_gesture_;
  ui::PageTransition transition_;
  bool is_external_protocol_;
  net::Error net_error_code_;
  RenderFrameHostImpl* render_frame_host_;
  bool is_same_page_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  // The state the navigation is in.
  State state_;

  // Whether the navigation is in the middle of a transfer. Set to false when
  // the DidStartProvisionalLoad is received from the new renderer.
  bool is_transferring_;

  // The FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node_;

  // A list of Throttles registered for this navigation.
  ScopedVector<NavigationThrottle> throttles_;

  // The index of the next throttle to check.
  size_t next_index_;

  // The time this navigation started.
  const base::TimeTicks navigation_start_;

  // This callback will be run when all throttle checks have been performed.
  ThrottleChecksFinishedCallback complete_callback_;

  // PlzNavigate
  // Manages the lifetime of a pre-created ServiceWorkerProviderHost until a
  // corresponding ServiceWorkerNetworkProvider is created in the renderer.
  scoped_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
