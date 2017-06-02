// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_REQUEST_INFO_IMPL_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_REQUEST_INFO_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_states.h"

namespace content {
class DetachableResourceHandler;
class ResourceContext;
struct GlobalRequestID;
struct GlobalRoutingID;

// Holds the data ResourceDispatcherHost associates with each request.
// Retrieve this data by calling ResourceDispatcherHost::InfoForRequest.
class ResourceRequestInfoImpl : public ResourceRequestInfo,
                                public base::SupportsUserData::Data {
 public:
  using TransferCallback =
      base::Callback<void(mojom::URLLoaderAssociatedRequest,
                          mojom::URLLoaderClientPtr)>;

  // Returns the ResourceRequestInfoImpl associated with the given URLRequest.
  CONTENT_EXPORT static ResourceRequestInfoImpl* ForRequest(
      net::URLRequest* request);

  // And, a const version for cases where you only need read access.
  CONTENT_EXPORT static const ResourceRequestInfoImpl* ForRequest(
      const net::URLRequest* request);

  CONTENT_EXPORT ResourceRequestInfoImpl(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      int route_id,
      int frame_tree_node_id,
      int origin_pid,
      int request_id,
      int render_frame_id,
      bool is_main_frame,
      bool parent_is_main_frame,
      ResourceType resource_type,
      ui::PageTransition transition_type,
      bool should_replace_current_entry,
      bool is_download,
      bool is_stream,
      bool allow_download,
      bool has_user_gesture,
      bool enable_load_timing,
      bool enable_upload_progress,
      bool do_not_prompt_for_login,
      blink::WebReferrerPolicy referrer_policy,
      blink::WebPageVisibilityState visibility_state,
      ResourceContext* context,
      bool report_raw_headers,
      bool is_async,
      PreviewsState previews_state,
      const scoped_refptr<ResourceRequestBodyImpl> body,
      bool initiated_in_secure_context);
  ~ResourceRequestInfoImpl() override;

  // ResourceRequestInfo implementation:
  WebContentsGetter GetWebContentsGetterForRequest() const override;
  FrameTreeNodeIdGetter GetFrameTreeNodeIdGetterForRequest() const override;
  ResourceContext* GetContext() const override;
  int GetChildID() const override;
  int GetRouteID() const override;
  GlobalRequestID GetGlobalRequestID() const override;
  int GetOriginPID() const override;
  int GetRenderFrameID() const override;
  int GetFrameTreeNodeId() const override;
  bool IsMainFrame() const override;
  bool ParentIsMainFrame() const override;
  ResourceType GetResourceType() const override;
  int GetProcessType() const override;
  blink::WebReferrerPolicy GetReferrerPolicy() const override;
  blink::WebPageVisibilityState GetVisibilityState() const override;
  ui::PageTransition GetPageTransition() const override;
  bool HasUserGesture() const override;
  bool WasIgnoredByHandler() const override;
  bool GetAssociatedRenderFrame(int* render_process_id,
                                int* render_frame_id) const override;
  bool IsAsync() const override;
  bool IsDownload() const override;
  // Returns a bitmask of potentially several Previews optimizations.
  PreviewsState GetPreviewsState() const override;
  bool ShouldReportRawHeaders() const;
  NavigationUIData* GetNavigationUIData() const override;

  CONTENT_EXPORT void AssociateWithRequest(net::URLRequest* request);

  CONTENT_EXPORT int GetRequestID() const;
  GlobalRoutingID GetGlobalRoutingID() const;

  // PlzNavigate
  // The id of the FrameTreeNode that initiated this request (for a navigation
  // request).
  int frame_tree_node_id() const { return frame_tree_node_id_; }

  ResourceRequesterInfo* requester_info() { return requester_info_.get(); }

  // Updates the data associated with this request after it is is transferred
  // to a new renderer process.  Not all data will change during a transfer.
  // We do not expect the ResourceContext to change during navigation, so that
  // does not need to be updated.
  void UpdateForTransfer(int route_id,
                         int render_frame_id,
                         int origin_pid,
                         int request_id,
                         ResourceRequesterInfo* requester_info,
                         mojom::URLLoaderAssociatedRequest url_loader_request,
                         mojom::URLLoaderClientPtr url_loader_client);

  // Whether this request is part of a navigation that should replace the
  // current session history entry. This state is shuffled up and down the stack
  // for request transfers.
  bool should_replace_current_entry() const {
    return should_replace_current_entry_;
  }

  // DetachableResourceHandler for this request.  May be NULL.
  DetachableResourceHandler* detachable_handler() const {
    return detachable_handler_;
  }
  void set_detachable_handler(DetachableResourceHandler* h) {
    detachable_handler_ = h;
  }

  // Downloads are allowed only as a top level request.
  bool allow_download() const { return allow_download_; }

  // Whether this is a download.
  void set_is_download(bool download) { is_download_ = download; }

  // Whether this is a stream.
  bool is_stream() const { return is_stream_; }
  void set_is_stream(bool stream) { is_stream_ = stream; }

  void set_was_ignored_by_handler(bool value) {
    was_ignored_by_handler_ = value;
  }

  // Whether this request has been counted towards the number of in flight
  // requests, which is only true for requests that require a file descriptor
  // for their shared memory buffer.
  bool counted_as_in_flight_request() const {
    return counted_as_in_flight_request_;
  }
  void set_counted_as_in_flight_request(bool was_counted) {
    counted_as_in_flight_request_ = was_counted;
  }

  // The approximate in-memory size (bytes) that we credited this request
  // as consuming in |outstanding_requests_memory_cost_map_|.
  int memory_cost() const { return memory_cost_; }
  void set_memory_cost(int cost) { memory_cost_ = cost; }

  bool is_load_timing_enabled() const { return enable_load_timing_; }

  bool is_upload_progress_enabled() const { return enable_upload_progress_; }

  bool do_not_prompt_for_login() const { return do_not_prompt_for_login_; }
  void set_do_not_prompt_for_login(bool do_not_prompt) {
    do_not_prompt_for_login_ = do_not_prompt;
  }

  const scoped_refptr<ResourceRequestBodyImpl>& body() const { return body_; }
  void ResetBody();

  bool initiated_in_secure_context() const {
    return initiated_in_secure_context_;
  }
  void set_initiated_in_secure_context_for_testing(bool secure) {
    initiated_in_secure_context_ = secure;
  }

  void set_navigation_ui_data(
      std::unique_ptr<NavigationUIData> navigation_ui_data) {
    navigation_ui_data_ = std::move(navigation_ui_data);
  }

  void set_on_transfer(const TransferCallback& on_transfer) {
    on_transfer_ = on_transfer;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           DeletedFilterDetached);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           DeletedFilterDetachedRedirect);
  // Non-owning, may be NULL.
  DetachableResourceHandler* detachable_handler_;

  scoped_refptr<ResourceRequesterInfo> requester_info_;
  int route_id_;
  const int frame_tree_node_id_;
  int origin_pid_;
  int request_id_;
  int render_frame_id_;
  bool is_main_frame_;
  bool parent_is_main_frame_;
  bool should_replace_current_entry_;
  bool is_download_;
  bool is_stream_;
  bool allow_download_;
  bool has_user_gesture_;
  bool enable_load_timing_;
  bool enable_upload_progress_;
  bool do_not_prompt_for_login_;
  bool was_ignored_by_handler_;
  bool counted_as_in_flight_request_;
  ResourceType resource_type_;
  ui::PageTransition transition_type_;
  int memory_cost_;
  blink::WebReferrerPolicy referrer_policy_;
  blink::WebPageVisibilityState visibility_state_;
  ResourceContext* context_;
  bool report_raw_headers_;
  bool is_async_;
  PreviewsState previews_state_;
  scoped_refptr<ResourceRequestBodyImpl> body_;
  bool initiated_in_secure_context_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // This callback is set by MojoAsyncResourceHandler to update its mojo binding
  // and remote endpoint. This callback will be removed once PlzNavigate is
  // shipped.
  TransferCallback on_transfer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestInfoImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_REQUEST_INFO_IMPL_H_
