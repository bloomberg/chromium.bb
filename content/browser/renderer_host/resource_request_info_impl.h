// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_INFO_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_INFO_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "base/time.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/process_type.h"
#include "content/public/common/referrer.h"
#include "net/base/load_states.h"
#include "webkit/glue/resource_type.h"

namespace webkit_blob {
class BlobData;
}

namespace content {
class AsyncResourceHandler;
class CrossSiteResourceHandler;
class ResourceContext;
struct GlobalRequestID;

// Holds the data ResourceDispatcherHost associates with each request.
// Retrieve this data by calling ResourceDispatcherHost::InfoForRequest.
class ResourceRequestInfoImpl : public ResourceRequestInfo,
                                public base::SupportsUserData::Data {
 public:
  // Returns the ResourceRequestInfoImpl associated with the given URLRequest.
  CONTENT_EXPORT static ResourceRequestInfoImpl* ForRequest(
      net::URLRequest* request);

  // And, a const version for cases where you only need read access.
  static const ResourceRequestInfoImpl* ForRequest(
      const net::URLRequest* request);

  CONTENT_EXPORT ResourceRequestInfoImpl(
      ProcessType process_type,
      int child_id,
      int route_id,
      int origin_pid,
      int request_id,
      bool is_main_frame,
      int64 frame_id,
      bool parent_is_main_frame,
      int64 parent_frame_id,
      ResourceType::Type resource_type,
      PageTransition transition_type,
      bool is_download,
      bool allow_download,
      bool has_user_gesture,
      WebKit::WebReferrerPolicy referrer_policy,
      ResourceContext* context);
  virtual ~ResourceRequestInfoImpl();

  // ResourceRequestInfo implementation:
  virtual ResourceContext* GetContext() const OVERRIDE;
  virtual int GetChildID() const OVERRIDE;
  virtual int GetRouteID() const OVERRIDE;
  virtual int GetOriginPID() const OVERRIDE;
  virtual int GetRequestID() const OVERRIDE;
  virtual bool IsMainFrame() const OVERRIDE;
  virtual int64 GetFrameID() const OVERRIDE;
  virtual bool ParentIsMainFrame() const OVERRIDE;
  virtual int64 GetParentFrameID() const OVERRIDE;
  virtual ResourceType::Type GetResourceType() const OVERRIDE;
  virtual WebKit::WebReferrerPolicy GetReferrerPolicy() const OVERRIDE;
  virtual bool HasUserGesture() const OVERRIDE;
  virtual bool WasIgnoredByHandler() const OVERRIDE;
  virtual bool GetAssociatedRenderView(int* render_process_id,
                                       int* render_view_id) const OVERRIDE;


  void AssociateWithRequest(net::URLRequest* request);

  GlobalRequestID GetGlobalRequestID() const;

  // CrossSiteResourceHandler for this request.  May be null.
  CrossSiteResourceHandler* cross_site_handler() {
    return cross_site_handler_;
  }
  void set_cross_site_handler(CrossSiteResourceHandler* h) {
    cross_site_handler_ = h;
  }

  // AsyncResourceHandler for this request.  May be null.
  AsyncResourceHandler* async_handler() {
    return async_handler_;
  }
  void set_async_handler(AsyncResourceHandler* h) {
    async_handler_ = h;
  }

  // Identifies the type of process (renderer, plugin, etc.) making the request.
  ProcessType process_type() const {
    return process_type_;
  }

  // Downloads are allowed only as a top level request.
  bool allow_download() const { return allow_download_; }

  // Whether this is a download.
  bool is_download() const { return is_download_; }
  void set_is_download(bool download) { is_download_ = download; }

  PageTransition transition_type() const { return transition_type_; }

  void set_was_ignored_by_handler(bool value) {
    was_ignored_by_handler_ = value;
  }

  // The approximate in-memory size (bytes) that we credited this request
  // as consuming in |outstanding_requests_memory_cost_map_|.
  int memory_cost() const { return memory_cost_; }
  void set_memory_cost(int cost) { memory_cost_ = cost; }

  // We hold a reference to the requested blob data to ensure it doesn't
  // get finally released prior to the net::URLRequestJob being started.
  webkit_blob::BlobData* requested_blob_data() const {
    return requested_blob_data_.get();
  }
  void set_requested_blob_data(webkit_blob::BlobData* data);

 private:
  // Non-owning, may be NULL.
  CrossSiteResourceHandler* cross_site_handler_;
  AsyncResourceHandler* async_handler_;

  ProcessType process_type_;
  int child_id_;
  int route_id_;
  int origin_pid_;
  int request_id_;
  bool is_main_frame_;
  int64 frame_id_;
  bool parent_is_main_frame_;
  int64 parent_frame_id_;
  bool is_download_;
  bool allow_download_;
  bool has_user_gesture_;
  bool was_ignored_by_handler_;
  ResourceType::Type resource_type_;
  PageTransition transition_type_;
  int memory_cost_;
  scoped_refptr<webkit_blob::BlobData> requested_blob_data_;
  WebKit::WebReferrerPolicy referrer_policy_;
  ResourceContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestInfoImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_INFO_IMPL_H_
