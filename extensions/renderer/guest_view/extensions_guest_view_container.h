// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_

#include <queue>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/renderer/guest_view/guest_view_container.h"
#include "extensions/renderer/scoped_persistent.h"

namespace extensions {

class ExtensionsGuestViewContainer : public GuestViewContainer {
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

  explicit ExtensionsGuestViewContainer(content::RenderFrame* render_frame);
  ~ExtensionsGuestViewContainer() override;

  static ExtensionsGuestViewContainer* FromID(int render_view_routing_id,
                                              int element_instance_id);

  void AttachGuest(linked_ptr<AttachRequest> request);

  // BrowserPluginDelegate implementation.
  void SetElementInstanceID(int element_instance_id) override;
  void Ready() override;

  // GuestViewContainer override.
  bool HandlesMessage(const IPC::Message& message) override;
  bool OnMessage(const IPC::Message& message) override;

 private:
  void OnGuestAttached(int element_instance_id,
                       int guest_proxy_routing_id);

  void AttachGuestInternal(linked_ptr<AttachRequest> request);
  void EnqueueAttachRequest(linked_ptr<AttachRequest> request);
  void PerformPendingAttachRequest();
  void HandlePendingResponseCallback(int guest_proxy_routing_id);

  bool ready_;

  std::deque<linked_ptr<AttachRequest> > pending_requests_;
  linked_ptr<AttachRequest> pending_response_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsGuestViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_
