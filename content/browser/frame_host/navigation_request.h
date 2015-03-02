// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/navigation_params.h"

namespace content {

class FrameTreeNode;
class NavigationURLLoader;
class ResourceRequestBody;
class SiteInstanceImpl;
struct NavigationRequestInfo;

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

  // Helper function to determine if the navigation request to |url| should be
  // sent to the network stack.
  static bool ShouldMakeNetworkRequest(const GURL& url);

  // Creates a request for a browser-intiated navigation.
  static scoped_ptr<NavigationRequest> CreateBrowserInitiated(
      FrameTreeNode* frame_tree_node,
      const NavigationEntryImpl& entry,
      FrameMsg_Navigate_Type::Value navigation_type,
      base::TimeTicks navigation_start);

  // Creates a request for a renderer-intiated navigation.
  // Note: |body| is sent to the IO thread when calling BeginNavigation, and
  // should no longer be manipulated afterwards on the UI thread.
  static scoped_ptr<NavigationRequest> CreateRendererInitiated(
      FrameTreeNode* frame_tree_node,
      const CommonNavigationParams& common_params,
      const BeginNavigationParams& begin_params,
      scoped_refptr<ResourceRequestBody> body);

  ~NavigationRequest() override;

  // Called on the UI thread by the Navigator to start the navigation. Returns
  // whether a request was made on the IO thread.
  // TODO(clamy): see if ResourceRequestBody could be un-refcounted to avoid
  // threading subtleties.
  bool BeginNavigation();

  const CommonNavigationParams& common_params() const { return common_params_; }

  const BeginNavigationParams& begin_params() const { return begin_params_; }

  const CommitNavigationParams& commit_params() const { return commit_params_; }

  NavigationURLLoader* loader_for_testing() const { return loader_.get(); }

  NavigationState state() const { return state_; }

  SiteInstanceImpl* source_site_instance() const {
    return source_site_instance_.get();
  }

  SiteInstanceImpl* dest_site_instance() const {
    return dest_site_instance_.get();
  }

  NavigationEntryImpl::RestoreType restore_type() const {
    return restore_type_;
  };

  bool is_view_source() const { return is_view_source_; };

  int bindings() const { return bindings_; };

  bool browser_initiated() const { return browser_initiated_ ; }

  void SetWaitingForRendererResponse() {
    DCHECK(state_ == NOT_STARTED);
    state_ = WAITING_FOR_RENDERER_RESPONSE;
  }

 private:
  NavigationRequest(FrameTreeNode* frame_tree_node,
                    const CommonNavigationParams& common_params,
                    const BeginNavigationParams& begin_params,
                    const CommitNavigationParams& commit_params,
                    scoped_refptr<ResourceRequestBody> body,
                    bool browser_initiated,
                    const NavigationEntryImpl* navitation_entry);

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) override;
  void OnResponseStarted(const scoped_refptr<ResourceResponse>& response,
                         scoped_ptr<StreamHandle> body) override;
  void OnRequestFailed(int net_error) override;
  void OnRequestStarted(base::TimeTicks timestamp) override;

  FrameTreeNode* frame_tree_node_;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  CommonNavigationParams common_params_;
  const BeginNavigationParams begin_params_;
  const CommitNavigationParams commit_params_;
  const bool browser_initiated_;

  NavigationState state_;


  // The parameters to send to the IO thread. |loader_| takes ownership of
  // |info_| after calling BeginNavigation.
  scoped_ptr<NavigationRequestInfo> info_;

  scoped_ptr<NavigationURLLoader> loader_;

  // These next items are used in browser-initiated navigations to store
  // information from the NavigationEntryImpl that is required after request
  // creation time.
  scoped_refptr<SiteInstanceImpl> source_site_instance_;
  scoped_refptr<SiteInstanceImpl> dest_site_instance_;
  NavigationEntryImpl::RestoreType restore_type_;
  bool is_view_source_;
  int bindings_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
