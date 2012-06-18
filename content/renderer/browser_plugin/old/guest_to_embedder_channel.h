// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_OLD_GUEST_TO_EMBEDDER_CHANNEL_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_OLD_GUEST_TO_EMBEDDER_CHANNEL_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace content {

class BrowserPluginChannelManager;

// GuestToEmbedderChannel is a Dispatcher that sends and receives ppapi messages
// from the browser plugin embedder. It is reference counted because it is held
// by a RenderViewImpl which (indirectly) owns a PpapiCommandBufferProxy through
// a WebGraphicsContext3DCommandBufferImpl which is owned by WebKit. Since the
// lifetime of this context is less than the lifetime of the RenderViewImpl, we
// keep the GuestToEmbedderChannel alive as long as a RenderViewImpl has access
// to it. If the context is lost, then the PpapiCommandBufferProxy is destroyed
// and we can safely release the reference to this GuestToEmbedderChannel held
// by RenderViewImpl.
class GuestToEmbedderChannel
    : public ppapi::proxy::Dispatcher,
      public base::RefCounted<GuestToEmbedderChannel> {
 public:
  GuestToEmbedderChannel(
      const std::string& embedder_channel_name,
      const IPC::ChannelHandle& embedder_channel_handle);

  // This must be called before anything else. Returns true on success.
  bool InitChannel(const IPC::ChannelHandle& channel_handle);

  // Creates a new WebGraphicsContext3DCommandBufferImpl and returns it.
  WebGraphicsContext3DCommandBufferImpl* CreateWebGraphicsContext3D(
      RenderViewImpl* render_view,
      const WebKit::WebGraphicsContext3D::Attributes& attributes,
      bool offscreen);

  // Inform the host to invalidate its plugin container after a swap buffer.
  void IssueSwapBuffers(const ppapi::HostResource& resource);

  // Request the receipt of events from the embedder renderer.
  void RequestInputEvents(PP_Instance instance);

  // Request a graphics context from the embedder renderer.
  bool CreateGraphicsContext(
      WebGraphicsContext3DCommandBufferImpl* context,
      const WebKit::WebGraphicsContext3D::Attributes& attributes,
      bool offscreen,
      RenderViewImpl* render_view);

  // Register the given RenderView with the given PP_Instance.
  void AddGuest(PP_Instance instance, RenderViewImpl* render_view);

  // Removes the guest with the given instance identifier from the
  // InstanceMap.
  void RemoveGuest(PP_Instance instance);

  const std::string& embedder_channel_name() const {
    return embedder_channel_name_;
  }

  const IPC::ChannelHandle& embedder_channel_handle() const {
    return embedder_channel_handle_;
  }

  // ppapi::proxy::Dispatcher implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual bool IsPlugin() const OVERRIDE;

 private:
  friend class base::RefCounted<GuestToEmbedderChannel>;
  typedef std::map<PP_Instance, base::WeakPtr<RenderViewImpl> > InstanceMap;
  typedef std::map<int, int> RoutingIDToInstanceMap;

  virtual ~GuestToEmbedderChannel();

  void OnSupportsInterface(const std::string& interface_name, bool* result);
  void OnSetPreferences(const ppapi::Preferences& prefs);
  void OnReserveInstanceId(PP_Instance instance, bool* usable);
  void OnDidCreate(PP_Instance instance,
                   const std::vector<std::string>& argn,
                   const std::vector<std::string>& argv,
                   PP_Bool* result);
  void OnDidDestroy(PP_Instance instance);
  void OnDidChangeView(PP_Instance instance,
                       const ppapi::ViewData& new_data,
                       PP_Bool flash_fullscreen);
  void OnDidChangeFocus(PP_Instance instance, PP_Bool has_focus);
  void OnGetInstanceObject(PP_Instance instance,
                           ppapi::proxy::SerializedVarReturnValue result);

  void OnHandleMessage(PP_Instance instance,
                       ppapi::proxy::SerializedVarReceiveInput data);

  void OnHandleFilteredInputEvent(PP_Instance instance,
                                  const ppapi::InputEventData& data,
                                  PP_Bool* result);

  void OnSwapBuffersACK(const ppapi::HostResource& context,
                        int32_t pp_error);

  void OnContextLost(PP_Instance instance);

  void OnGuestReady(PP_Instance instance, int embedder_container_id);

  base::WeakPtr<RenderViewImpl> render_view_;
  std::string embedder_channel_name_;
  IPC::ChannelHandle embedder_channel_handle_;
  PepperProxyChannelDelegateImpl delegate_;

  InstanceMap render_view_instances_;
  RoutingIDToInstanceMap routing_id_instance_map_;

  DISALLOW_COPY_AND_ASSIGN(GuestToEmbedderChannel);
};

}  // namespace content

#endif // CONTENT_RENDERER_BROWSER_PLUGIN_OLD_GUEST_TO_EMBEDDER_CHANNEL_H_
