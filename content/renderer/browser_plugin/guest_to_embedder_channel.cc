// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/guest_to_embedder_channel.h"

#include "base/process_util.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/child_process.h"
#include "content/renderer/browser_plugin/browser_plugin_channel_manager.h"
#include "content/renderer/browser_plugin/browser_plugin_var_serialization_rules.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/proxy/ppapi_command_buffer_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "webkit/plugins/ppapi/event_conversion.h"

namespace content {

GuestToEmbedderChannel::GuestToEmbedderChannel(
    const std::string& embedder_channel_name,
    const IPC::ChannelHandle& embedder_channel_handle)
    : Dispatcher(NULL),
      embedder_channel_name_(embedder_channel_name),
      embedder_channel_handle_(embedder_channel_handle) {
  SetSerializationRules(new BrowserPluginVarSerializationRules());
}

GuestToEmbedderChannel::~GuestToEmbedderChannel() {
}

bool GuestToEmbedderChannel::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GuestToEmbedderChannel, message)
    IPC_MESSAGE_HANDLER(PpapiMsg_SupportsInterface, OnSupportsInterface)
    IPC_MESSAGE_HANDLER(PpapiMsg_SetPreferences, OnSetPreferences)
    IPC_MESSAGE_HANDLER(PpapiMsg_ReserveInstanceId, OnReserveInstanceId)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidCreate,
                        OnDidCreate)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidDestroy,
                        OnDidDestroy)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidChangeView,
                        OnDidChangeView)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidChangeFocus,
                        OnDidChangeFocus)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPMessaging_HandleMessage,
                        OnHandleMessage)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInputEvent_HandleFilteredInputEvent,
                        OnHandleFilteredInputEvent)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPGraphics3D_ContextLost,
                        OnContextLost)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestReady,
                        OnGuestReady)
    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool GuestToEmbedderChannel::Send(IPC::Message* message) {
  // We always want guest->host messages to arrive in-order. If some sync
  // and some async messages are sent in response to a synchronous
  // host->guest call, the sync reply will be processed before the async
  // reply, and everything will be confused.
  //
  // Allowing all async messages to unblock the renderer means more reentrancy
  // there but gives correct ordering.
  message->set_unblock(true);
  return ProxyChannel::Send(message);
}

void GuestToEmbedderChannel::OnChannelError() {
  // We cannot destroy the GuestToEmbedderChannel here because a
  // PpapiCommandBufferProxy may still refer to this object.
  // However, we should not be using this channel again once we get a
  // channel error so we remove it from the channel manager.
  RenderThreadImpl::current()->browser_plugin_channel_manager()->
      RemoveChannelByName(embedder_channel_name_);
}

bool GuestToEmbedderChannel::IsPlugin() const {
  return true;
}

WebGraphicsContext3DCommandBufferImpl*
    GuestToEmbedderChannel::CreateWebGraphicsContext3D(
        RenderViewImpl* render_view,
        const WebKit::WebGraphicsContext3D::Attributes& attributes,
        bool offscreen) {
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          0, GURL(), NULL,
          render_view->AsWeakPtr()));

  // Special case: RenderView initialization has not yet completed.
  if (!render_view->guest_pp_instance())
    return context.release();

  if (CreateGraphicsContext(context.get(),
                            attributes,
                            offscreen,
                            render_view))
    return context.release();

  return NULL;
}

void GuestToEmbedderChannel::IssueSwapBuffers(
    const ppapi::HostResource& resource) {
  Send(new PpapiHostMsg_PPBGraphics3D_SwapBuffers(
       ppapi::API_ID_PPB_GRAPHICS_3D, resource));
}

bool GuestToEmbedderChannel::InitChannel(
    const IPC::ChannelHandle& channel_handle) {
  return ProxyChannel::InitWithChannel(&delegate_, channel_handle, false);
}

void GuestToEmbedderChannel::OnSupportsInterface(
    const std::string& interface_name,
    bool* result) {
  // TODO(fsamuel): This is a hack to avoid getting GetInstanceObject messages
  // and failing a CHECK. A more correct solution is to implement
  // VarSerializationRules for GuestToEmbedderChannel.
  *result = interface_name.find("PPP_Instance_Private") == std::string::npos;
}

void GuestToEmbedderChannel::OnSetPreferences(const ppapi::Preferences& prefs) {
  // TODO(fsamuel): Do we care about these preferences?
  // These look like some font stuff from WebPreferences.
  // Perhaps this should be plumbed into our associated RenderView?
  NOTIMPLEMENTED();
}

void GuestToEmbedderChannel::OnReserveInstanceId(PP_Instance instance,
                                                 bool* usable) {
  *usable =
      render_view_instances_.find(instance) == render_view_instances_.end();
}

void GuestToEmbedderChannel::RequestInputEvents(PP_Instance instance) {
  // Request receipt of input events
  Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      ppapi::API_ID_PPB_INSTANCE, instance, true,
      PP_INPUTEVENT_CLASS_MOUSE |
      PP_INPUTEVENT_CLASS_KEYBOARD |
      PP_INPUTEVENT_CLASS_WHEEL |
      PP_INPUTEVENT_CLASS_TOUCH));
}

bool GuestToEmbedderChannel::CreateGraphicsContext(
    WebGraphicsContext3DCommandBufferImpl* context,
    const WebKit::WebGraphicsContext3D::Attributes& attributes,
    bool offscreen,
    RenderViewImpl* render_view) {
  std::vector<int32_t> attribs;
  attribs.push_back(PP_GRAPHICS3DATTRIB_NONE);

  ppapi::HostResource resource;
  DCHECK(render_view->guest_pp_instance());
  // TODO(fsamuel): Support shared contexts.
  bool success = Send(new PpapiHostMsg_PPBGraphics3D_Create(
      ppapi::API_ID_PPB_GRAPHICS_3D,
      render_view->guest_pp_instance(),
      ppapi::HostResource(),
      attribs,
      &resource));
  if (!success || resource.is_null())
    return false;
  if (!offscreen) {
    PP_Bool result = PP_FALSE;
    Send(new PpapiHostMsg_PPBInstance_BindGraphics(
            ppapi::API_ID_PPB_INSTANCE,
            render_view->guest_pp_instance(),
            resource,
            &result));
    if (result != PP_TRUE)
      return false;
  }

  CommandBufferProxy* command_buffer =
      new ppapi::proxy::PpapiCommandBufferProxy(resource, this);
  command_buffer->Initialize();
  context->InitializeWithCommandBuffer(
      command_buffer,
      attributes,
      false /* bind generates resources */);
  render_view->set_guest_graphics_resource(resource);
  return true;
}

void GuestToEmbedderChannel::AddGuest(
    PP_Instance instance,
    RenderViewImpl* render_view) {
  DCHECK(instance);
  DCHECK(render_view_instances_.find(instance) == render_view_instances_.end());
  render_view_instances_[instance] = render_view->AsWeakPtr();
}


void GuestToEmbedderChannel::RemoveGuest(PP_Instance instance) {
  DCHECK(render_view_instances_.find(instance) != render_view_instances_.end());
  render_view_instances_.erase(instance);
}

void GuestToEmbedderChannel::OnDidCreate(
    PP_Instance instance,
    const std::vector<std::string>& argn,
    const std::vector<std::string>& argv,
    PP_Bool* result) {
  DCHECK(render_view_instances_.find(instance) == render_view_instances_.end());
  RequestInputEvents(instance);
  *result = PP_TRUE;
}

void GuestToEmbedderChannel::OnDidDestroy(PP_Instance instance) {
  InstanceMap::iterator it = render_view_instances_.find(instance);
  DCHECK(it != render_view_instances_.end());
  RenderViewImpl* render_view = it->second;
  render_view->SetGuestToEmbedderChannel(NULL);
  render_view->set_guest_pp_instance(0);
  RemoveGuest(instance);
}

void GuestToEmbedderChannel::OnDidChangeView(
    PP_Instance instance,
    const ppapi::ViewData& new_data,
    PP_Bool flash_fullscreen) {
  // We can't do anything with this message if we don't have a render view
  // yet. If we do have a RenderView then we need to tell the associated
  // WebContentsObserver to resize.
  if (render_view_instances_.find(instance) != render_view_instances_.end()) {
    RenderViewImpl* render_view = render_view_instances_[instance];
        render_view->Send(
            new BrowserPluginHostMsg_ResizeGuest(
                render_view->GetRoutingID(),
                new_data.rect.size.width,
                new_data.rect.size.height));
  }
}

void GuestToEmbedderChannel::OnDidChangeFocus(PP_Instance instance,
                                              PP_Bool has_focus) {
  InstanceMap::iterator it = render_view_instances_.find(instance);
  if (it == render_view_instances_.end())
    return;
  RenderViewImpl* render_view = it->second;
  render_view->GetWebView()->setFocus(PP_ToBool(has_focus));
}

void GuestToEmbedderChannel::OnHandleMessage(
    PP_Instance instance,
    ppapi::proxy::SerializedVarReceiveInput message_data) {
  InstanceMap::iterator it = render_view_instances_.find(instance);
  if (it == render_view_instances_.end())
    return;

  PP_Var received_var(message_data.Get(this));
  DCHECK(received_var.type == PP_VARTYPE_STRING);
  ppapi::VarTracker* tracker = ppapi::PpapiGlobals::Get()->GetVarTracker();
  ppapi::StringVar* var = tracker->GetVar(received_var)->AsStringVar();
  DCHECK(var);

  RenderViewImpl* render_view = it->second;
  render_view->Send(
      new BrowserPluginHostMsg_NavigateFromGuest(
          render_view->GetRoutingID(),
          instance,
          var->value()));
}

void GuestToEmbedderChannel::OnHandleFilteredInputEvent(
    PP_Instance instance,
    const ppapi::InputEventData& data,
    PP_Bool* result) {
  if (render_view_instances_.find(instance) == render_view_instances_.end())
    return;

  RenderViewImpl* render_view = render_view_instances_[instance];
  scoped_ptr<WebKit::WebInputEvent> web_input_event(
      webkit::ppapi::CreateWebInputEvent(data));
  *result = PP_FromBool(
      render_view->GetWebView()->handleInputEvent(*web_input_event));
}

void GuestToEmbedderChannel::OnContextLost(PP_Instance instance) {
  DCHECK(render_view_instances_.find(instance) != render_view_instances_.end());
  RenderViewImpl* render_view = render_view_instances_[instance];
    // TODO(fsamuel): This is test code. Need to find a better way to tell
    // a WebView to drop its context.
  render_view->GetWebView()->loseCompositorContext(1);
}

void GuestToEmbedderChannel::OnGuestReady(PP_Instance instance,
                                          int embedder_container_id) {
  RenderThreadImpl::current()->browser_plugin_channel_manager()->
      GuestReady(instance, embedder_channel_name(), embedder_container_id);
}

}  // namespace content
