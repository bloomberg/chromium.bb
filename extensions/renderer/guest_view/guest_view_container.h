// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
#define CHROME_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/scoped_persistent.h"

namespace extensions {

class GuestViewContainer : public content::BrowserPluginDelegate,
                           public content::RenderFrameObserver {
 public:
  GuestViewContainer(content::RenderFrame* render_frame,
                     const std::string& mime_type);
  virtual ~GuestViewContainer();

  static GuestViewContainer* FromID(int render_view_routing_id,
                                    int element_instance_id);

  void AttachGuest(int element_instance_id,
                   int guest_instance_id,
                   scoped_ptr<base::DictionaryValue> params,
                   v8::Handle<v8::Function> callback,
                   v8::Isolate* isolate);

  // BrowserPluginDelegate implementation.
  virtual void SetElementInstanceID(int element_instance_id) OVERRIDE;
  virtual void DidFinishLoading() OVERRIDE;
  virtual void DidReceiveData(const char* data, int data_length) OVERRIDE;

  // RenderFrameObserver override.
  virtual void OnDestruct() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnCreateMimeHandlerViewGuestACK(int element_instance_id);
  void OnGuestAttached(int element_instance_id, int guest_routing_id);

  static bool ShouldHandleMessage(const IPC::Message& mesage);

  const std::string mime_type_;
  int element_instance_id_;
  std::string html_string_;
  // Save the RenderView RoutingID here so that we can use it during
  // destruction.
  int render_view_routing_id_;

  bool attached_;
  bool attach_pending_;

  ScopedPersistent<v8::Function> callback_;
  v8::Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewContainer);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_GUEST_VIEW_GUEST_VIEW_CONTAINER_H_
