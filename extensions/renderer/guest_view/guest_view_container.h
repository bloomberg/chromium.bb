// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
#define CHROME_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_

#include <queue>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/scoped_persistent.h"

namespace extensions {

class GuestViewContainer : public content::BrowserPluginDelegate,
                           public content::RenderFrameObserver {
 public:
  // This class represents an AttachGuest request from Javascript. It includes
  // the input parameters and the callback function. The Attach operation may
  // not execute immediately, if the container is not ready or if there are
  // other attach operations in flight.
  class AttachRequest {
   public:
    AttachRequest(int element_instance_id,
                  int guest_instance_id,
                  scoped_ptr<base::DictionaryValue> params,
                  v8::Handle<v8::Function> callback,
                  v8::Isolate* isolate);
    ~AttachRequest();

    int element_instance_id() const { return element_instance_id_; }

    int guest_instance_id() const { return guest_instance_id_; }

    base::DictionaryValue* attach_params() const {
      return params_.get();
    }

    bool HasCallback() const;

    v8::Handle<v8::Function> GetCallback() const;

    v8::Isolate* isolate() const { return isolate_; }

   private:
    const int element_instance_id_;
    const int guest_instance_id_;
    scoped_ptr<base::DictionaryValue> params_;
    ScopedPersistent<v8::Function> callback_;
    v8::Isolate* const isolate_;
  };

  GuestViewContainer(content::RenderFrame* render_frame,
                     const std::string& mime_type);
  ~GuestViewContainer() override;

  static GuestViewContainer* FromID(int render_view_routing_id,
                                    int element_instance_id);

  void AttachGuest(linked_ptr<AttachRequest> request);

  // BrowserPluginDelegate implementation.
  void SetElementInstanceID(int element_instance_id) override;
  void DidFinishLoading() override;
  void DidReceiveData(const char* data, int data_length) override;
  void Ready() override;

  // RenderFrameObserver override.
  void OnDestruct() override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnCreateMimeHandlerViewGuestACK(int element_instance_id);
  void OnGuestAttached(int element_instance_id, int guest_proxy_routing_id);

  void AttachGuestInternal(linked_ptr<AttachRequest> request);
  void EnqueueAttachRequest(linked_ptr<AttachRequest> request);
  void PerformPendingAttachRequest();
  void HandlePendingResponseCallback(int guest_proxy_routing_id);

  static bool ShouldHandleMessage(const IPC::Message& mesage);

  const std::string mime_type_;
  int element_instance_id_;
  std::string html_string_;
  // Save the RenderView RoutingID here so that we can use it during
  // destruction.
  int render_view_routing_id_;

  bool attached_;
  bool ready_;

  std::deque<linked_ptr<AttachRequest> > pending_requests_;
  linked_ptr<AttachRequest> pending_response_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewContainer);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
