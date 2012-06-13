// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_INFO_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_INFO_IMPL_H_
#pragma once

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

class SSLClientAuthHandler;

namespace webkit_blob {
class BlobData;
}

namespace content {
class CrossSiteResourceHandler;
class ResourceContext;
class ResourceDispatcherHostImpl;
class ResourceDispatcherHostLoginDelegate;
class ResourceHandler;

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

  // This will take a reference to the handler.
  CONTENT_EXPORT ResourceRequestInfoImpl(
      scoped_ptr<ResourceHandler> handler,
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
      uint64 upload_size,
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
  virtual uint64 GetUploadSize() const OVERRIDE;
  virtual bool GetAssociatedRenderView(int* render_process_id,
                                       int* render_view_id) const OVERRIDE;

  void AssociateWithRequest(net::URLRequest* request);

  // Top-level ResourceHandler servicing this request.
  ResourceHandler* resource_handler() { return resource_handler_.get(); }

  // Inserts a DoomedResourceHandler in front of the existing ResourceHandler.
  void InsertDoomedResourceHandler();

  // CrossSiteResourceHandler for this request, if it is a cross-site request.
  // (NULL otherwise.) This handler is part of the chain of ResourceHandlers
  // pointed to by resource_handler, and is not owned by this class.
  CrossSiteResourceHandler* cross_site_handler() {
    return cross_site_handler_;
  }
  void set_cross_site_handler(CrossSiteResourceHandler* h) {
    cross_site_handler_ = h;
  }

  // Pointer to the login delegate, or NULL if there is none for this request.
  ResourceDispatcherHostLoginDelegate* login_delegate() const {
    return login_delegate_.get();
  }
  CONTENT_EXPORT void set_login_delegate(
      ResourceDispatcherHostLoginDelegate* ld);

  // Pointer to the SSL auth, or NULL if there is none for this request.
  SSLClientAuthHandler* ssl_client_auth_handler() const {
    return ssl_client_auth_handler_.get();
  }
  void set_ssl_client_auth_handler(SSLClientAuthHandler* s);

  // Identifies the type of process (renderer, plugin, etc.) making the request.
  ProcessType process_type() const {
    return process_type_;
  }

  // Number of messages we've sent to the renderer that we haven't gotten an
  // ACK for. This allows us to avoid having too many messages in flight.
  int pending_data_count() const { return pending_data_count_; }
  void IncrementPendingDataCount() { pending_data_count_++; }
  void DecrementPendingDataCount() { pending_data_count_--; }

  // Downloads are allowed only as a top level request.
  bool allow_download() const { return allow_download_; }

  bool has_user_gesture() const { return has_user_gesture_; }

  // Whether this is a download.
  bool is_download() const { return is_download_; }
  void set_is_download(bool download) { is_download_ = download; }

  // The number of clients that have called pause on this request.
  int pause_count() const { return pause_count_; }
  void set_pause_count(int count) { pause_count_ = count; }

  PageTransition transition_type() const { return transition_type_; }

  void set_upload_size(uint64 upload_size) { upload_size_ = upload_size; }

  // When we're uploading data, this is the the byte offset into the uploaded
  // data that we've uploaded that we've send an upload progress update about.
  // The ResourceDispatcherHost will periodically update this value to track
  // upload progress and make sure it doesn't sent out duplicate updates.
  uint64 last_upload_position() const { return last_upload_position_; }
  void set_last_upload_position(uint64 p) { last_upload_position_ = p; }

  // Indicates when the ResourceDispatcherHost last update the upload
  // position. This is used to make sure we don't send too many updates.
  base::TimeTicks last_upload_ticks() const { return last_upload_ticks_; }
  void set_last_upload_ticks(base::TimeTicks t) { last_upload_ticks_ = t; }

  // Set when the ResourceDispatcherHost has sent out an upload progress, and
  // cleared whtn the ACK is received. This is used to throttle updates so
  // multiple updates aren't in flight at once.
  bool waiting_for_upload_progress_ack() const {
    return waiting_for_upload_progress_ack_;
  }
  void set_waiting_for_upload_progress_ack(bool waiting) {
    waiting_for_upload_progress_ack_ = waiting;
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
  friend class ResourceDispatcherHostImpl;

  // Request is temporarily not handling network data. Should be used only
  // by the ResourceDispatcherHost, not the event handlers (accessors are
  // provided for consistency with the rest of the interface).
  bool is_paused() const { return is_paused_; }
  void set_is_paused(bool paused) { is_paused_ = paused; }

  // Whether we called OnResponseStarted for this request or not. Should be used
  // only by the ResourceDispatcherHost, not the event handlers (accessors are
  // provided for consistency with the rest of the interface).
  bool called_on_response_started() const {
    return called_on_response_started_;
  }
  void set_called_on_response_started(bool called) {
    called_on_response_started_ = called;
  }

  // Whether this request has started reading any bytes from the response
  // yet. Will be true after the first (unpaused) call to Read. Should be used
  // only by the ResourceDispatcherHost, not the event handlers (accessors are
  // provided for consistency with the rest of the interface).
  bool has_started_reading() const { return has_started_reading_; }
  void set_has_started_reading(bool reading) { has_started_reading_ = reading; }

  // How many bytes have been read while this request has been paused. Should be
  // used only by the ResourceDispatcherHost, not the event handlers (accessors
  // are provided for consistency with the rest of the interface).
  int paused_read_bytes() const { return paused_read_bytes_; }
  void set_paused_read_bytes(int bytes) { paused_read_bytes_ = bytes; }

  scoped_ptr<ResourceHandler> resource_handler_;

  // Non-owning, may be NULL.
  CrossSiteResourceHandler* cross_site_handler_;

  scoped_refptr<ResourceDispatcherHostLoginDelegate> login_delegate_;
  scoped_refptr<SSLClientAuthHandler> ssl_client_auth_handler_;
  ProcessType process_type_;
  int child_id_;
  int route_id_;
  int origin_pid_;
  int request_id_;
  bool is_main_frame_;
  int64 frame_id_;
  bool parent_is_main_frame_;
  int64 parent_frame_id_;
  int pending_data_count_;
  bool is_download_;
  bool allow_download_;
  bool has_user_gesture_;
  int pause_count_;
  ResourceType::Type resource_type_;
  PageTransition transition_type_;
  uint64 upload_size_;
  uint64 last_upload_position_;
  base::TimeTicks last_upload_ticks_;
  bool waiting_for_upload_progress_ack_;
  int memory_cost_;
  scoped_refptr<webkit_blob::BlobData> requested_blob_data_;
  WebKit::WebReferrerPolicy referrer_policy_;
  ResourceContext* context_;

  // "Private" data accessible only to ResourceDispatcherHost (use the
  // accessors above for consistency).
  bool is_paused_;
  bool called_on_response_started_;
  bool has_started_reading_;
  int paused_read_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestInfoImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_INFO_IMPL_H_
