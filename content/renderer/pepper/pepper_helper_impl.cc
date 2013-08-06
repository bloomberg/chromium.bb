// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_helper_impl.h"

#include <cmath>
#include <cstddef>
#include <map>
#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/strings/string_split.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "content/child/child_process.h"
#include "content/child/npapi/webplugin.h"
#include "content/common/child_process_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/pepper_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"
#include "content/renderer/pepper/host_dispatcher_wrapper.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_file_system_host.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_hung_plugin_filter.h"
#include "content/renderer/pepper/pepper_in_process_resource_creation.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/ppb_tcp_socket_private_impl.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/pepper/url_response_info_util.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/webplugin_delegate_proxy.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/video/capture/video_capture_proxy.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/url_loader_resource.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/platform_file.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/ppp_instance_combined.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_tcp_server_socket_private_api.h"
#include "third_party/WebKit/public/web/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

using WebKit::WebFrame;

namespace content {

namespace {

void CreateHostForInProcessModule(RenderViewImpl* render_view,
                                  PluginModule* module,
                                  const WebPluginInfo& webplugin_info) {
  // First time an in-process plugin was used, make a host for it.
  const PepperPluginInfo* info =
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(webplugin_info);
  DCHECK(!info->is_out_of_process);

  ppapi::PpapiPermissions perms(
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(
          webplugin_info)->permissions);
  RendererPpapiHostImpl* host_impl =
      RendererPpapiHostImpl::CreateOnModuleForInProcess(
          module, perms);
  render_view->PpapiPluginCreated(host_impl);
}

}  // namespace

PepperHelperImpl::PepperHelperImpl(RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      render_view_(render_view),
      focused_plugin_(NULL),
      last_mouse_event_target_(NULL) {
  new PepperBrowserConnection(render_view);
}

PepperHelperImpl::~PepperHelperImpl() {
}

WebKit::WebPlugin* PepperHelperImpl::CreatePepperWebPlugin(
    const WebPluginInfo& webplugin_info,
    const WebKit::WebPluginParams& params) {
  bool pepper_plugin_was_registered = false;
  scoped_refptr<PluginModule> pepper_module(
      CreatePepperPluginModule(webplugin_info, &pepper_plugin_was_registered));

  if (pepper_plugin_was_registered) {
    if (!pepper_module.get())
      return NULL;
    return new PepperWebPluginImpl(
        pepper_module.get(), params, AsWeakPtr(), render_view_->AsWeakPtr());
  }

  return NULL;
}

scoped_refptr<PluginModule> PepperHelperImpl::CreatePepperPluginModule(
    const WebPluginInfo& webplugin_info,
    bool* pepper_plugin_was_registered) {
  *pepper_plugin_was_registered = true;

  // See if a module has already been loaded for this plugin.
  base::FilePath path(webplugin_info.path);
  scoped_refptr<PluginModule> module =
      PepperPluginRegistry::GetInstance()->GetLiveModule(path);
  if (module.get()) {
    if (!module->renderer_ppapi_host()) {
      // If the module exists and no embedder state was associated with it,
      // then the module was one of the ones preloaded and is an in-process
      // plugin. We need to associate our host state with it.
      CreateHostForInProcessModule(render_view_, module.get(), webplugin_info);
    }
    return module;
  }

  // In-process plugins will have always been created up-front to avoid the
  // sandbox restrictions. So getting here implies it doesn't exist or should
  // be out of process.
  const PepperPluginInfo* info =
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(webplugin_info);
  if (!info) {
    *pepper_plugin_was_registered = false;
    return scoped_refptr<PluginModule>();
  } else if (!info->is_out_of_process) {
    // In-process plugin not preloaded, it probably couldn't be initialized.
    return scoped_refptr<PluginModule>();
  }

  ppapi::PpapiPermissions permissions =
      ppapi::PpapiPermissions::GetForCommandLine(info->permissions);

  // Out of process: have the browser start the plugin process for us.
  IPC::ChannelHandle channel_handle;
  base::ProcessId peer_pid;
  int plugin_child_id = 0;
  Send(new ViewHostMsg_OpenChannelToPepperPlugin(
      path, &channel_handle, &peer_pid, &plugin_child_id));
  if (channel_handle.name.empty()) {
    // Couldn't be initialized.
    return scoped_refptr<PluginModule>();
  }

  // AddLiveModule must be called before any early returns since the
  // module's destructor will remove itself.
  module = new PluginModule(info->name, path, permissions);
  PepperPluginRegistry::GetInstance()->AddLiveModule(path, module.get());

  if (!CreateOutOfProcessModule(module.get(),
                                path,
                                permissions,
                                channel_handle,
                                peer_pid,
                                plugin_child_id,
                                false))  // is_external = false
    return scoped_refptr<PluginModule>();

  return module;
}

RendererPpapiHost* PepperHelperImpl::CreateOutOfProcessModule(
    PluginModule* module,
    const base::FilePath& path,
    ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId peer_pid,
    int plugin_child_id,
    bool is_external) {
  scoped_refptr<PepperHungPluginFilter> hung_filter(
      new PepperHungPluginFilter(path, routing_id(), plugin_child_id));
  scoped_ptr<HostDispatcherWrapper> dispatcher(
      new HostDispatcherWrapper(module,
                                peer_pid,
                                plugin_child_id,
                                permissions,
                                is_external));
  if (!dispatcher->Init(
          channel_handle,
          PluginModule::GetLocalGetInterfaceFunc(),
          ppapi::Preferences(render_view_->webkit_preferences()),
          hung_filter.get()))
    return NULL;

  RendererPpapiHostImpl* host_impl =
      RendererPpapiHostImpl::CreateOnModuleForOutOfProcess(
          module, dispatcher->dispatcher(), permissions);
  render_view_->PpapiPluginCreated(host_impl);

  module->InitAsProxied(dispatcher.release());
  return host_impl;
}

void PepperHelperImpl::ViewWillInitiatePaint() {
  // Notify all of our instances that we started painting. This is used for
  // internal bookkeeping only, so we know that the set can not change under
  // us.
  for (std::set<PepperPluginInstanceImpl*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->ViewWillInitiatePaint();
}

void PepperHelperImpl::ViewInitiatedPaint() {
  // Notify all instances that we painted.  The same caveats apply as for
  // ViewFlushedPaint regarding instances closing themselves, so we take
  // similar precautions.
  std::set<PepperPluginInstanceImpl*> plugins = active_instances_;
  for (std::set<PepperPluginInstanceImpl*>::iterator i = plugins.begin();
       i != plugins.end(); ++i) {
    if (active_instances_.find(*i) != active_instances_.end())
      (*i)->ViewInitiatedPaint();
  }
}

void PepperHelperImpl::ViewFlushedPaint() {
  // Notify all instances that we flushed. This will call into the plugin, and
  // we it may ask to close itself as a result. This will, in turn, modify our
  // set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  std::set<PepperPluginInstanceImpl*> plugins = active_instances_;
  for (std::set<PepperPluginInstanceImpl*>::iterator i = plugins.begin();
       i != plugins.end(); ++i) {
    // The copy above makes sure our iterator is never invalid if some plugins
    // are destroyed. But some plugin may decide to close all of its views in
    // response to a paint in one of them, so we need to make sure each one is
    // still "current" before using it.
    //
    // It's possible that a plugin was destroyed, but another one was created
    // with the same address. In this case, we'll call ViewFlushedPaint on that
    // new plugin. But that's OK for this particular case since we're just
    // notifying all of our instances that the view flushed, and the new one is
    // one of our instances.
    //
    // What about the case where a new one is created in a callback at a new
    // address and we don't issue the callback? We're still OK since this
    // callback is used for flush callbacks and we could not have possibly
    // started a new paint (ViewWillInitiatePaint) for the new plugin while
    // processing a previous paint for an existing one.
    if (active_instances_.find(*i) != active_instances_.end())
      (*i)->ViewFlushedPaint();
  }
}

PepperPluginInstanceImpl* PepperHelperImpl::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
  for (std::set<PepperPluginInstanceImpl*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i) {
    PepperPluginInstanceImpl* instance = *i;
    // In Flash fullscreen , the plugin contents should be painted onto the
    // fullscreen widget instead of the web page.
    if (!instance->FlashIsFullscreenOrPending() &&
        instance->GetBitmapForOptimizedPluginPaint(paint_bounds, dib, location,
                                                   clip, scale_factor))
      return *i;
  }
  return NULL;
}

void PepperHelperImpl::PluginFocusChanged(
    PepperPluginInstanceImpl* instance,
    bool focused) {
  if (focused)
    focused_plugin_ = instance;
  else if (focused_plugin_ == instance)
    focused_plugin_ = NULL;
  if (render_view_)
    render_view_->PpapiPluginFocusChanged();
}

void PepperHelperImpl::PluginTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginTextInputTypeChanged();
}

void PepperHelperImpl::PluginCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginCaretPositionChanged();
}

void PepperHelperImpl::PluginRequestedCancelComposition(
    PepperPluginInstanceImpl* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginCancelComposition();
}

void PepperHelperImpl::PluginSelectionChanged(
    PepperPluginInstanceImpl* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginSelectionChanged();
}

void PepperHelperImpl::OnImeSetComposition(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  if (!IsPluginAcceptingCompositionEvents()) {
    composition_text_ = text;
  } else {
    // TODO(kinaba) currently all composition events are sent directly to
    // plugins. Use DOM event mechanism after WebKit is made aware about
    // plugins that support composition.
    // The code below mimics the behavior of WebCore::Editor::setComposition.

    // Empty -> nonempty: composition started.
    if (composition_text_.empty() && !text.empty())
      focused_plugin_->HandleCompositionStart(string16());
    // Nonempty -> empty: composition canceled.
    if (!composition_text_.empty() && text.empty())
       focused_plugin_->HandleCompositionEnd(string16());
    composition_text_ = text;
    // Nonempty: composition is ongoing.
    if (!composition_text_.empty()) {
      focused_plugin_->HandleCompositionUpdate(composition_text_, underlines,
                                               selection_start, selection_end);
    }
  }
}

void PepperHelperImpl::OnImeConfirmComposition(const string16& text) {
  // Here, text.empty() has a special meaning. It means to commit the last
  // update of composition text (see RenderWidgetHost::ImeConfirmComposition()).
  const string16& last_text = text.empty() ? composition_text_ : text;

  // last_text is empty only when both text and composition_text_ is. Ignore it.
  if (last_text.empty())
    return;

  if (!IsPluginAcceptingCompositionEvents()) {
    for (size_t i = 0; i < text.size(); ++i) {
      WebKit::WebKeyboardEvent char_event;
      char_event.type = WebKit::WebInputEvent::Char;
      char_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      char_event.modifiers = 0;
      char_event.windowsKeyCode = last_text[i];
      char_event.nativeKeyCode = last_text[i];
      char_event.text[0] = last_text[i];
      char_event.unmodifiedText[0] = last_text[i];
      if (render_view_->webwidget())
        render_view_->webwidget()->handleInputEvent(char_event);
    }
  } else {
    // Mimics the order of events sent by WebKit.
    // See WebCore::Editor::setComposition() for the corresponding code.
    focused_plugin_->HandleCompositionEnd(last_text);
    focused_plugin_->HandleTextInput(last_text);
  }
  composition_text_.clear();
}

gfx::Rect PepperHelperImpl::GetCaretBounds() const {
  if (!focused_plugin_)
    return gfx::Rect(0, 0, 0, 0);
  return focused_plugin_->GetCaretBounds();
}

ui::TextInputType PepperHelperImpl::GetTextInputType() const {
  if (!focused_plugin_)
    return ui::TEXT_INPUT_TYPE_NONE;
  return focused_plugin_->text_input_type();
}

void PepperHelperImpl::GetSurroundingText(string16* text,
                                          ui::Range* range) const {
  if (!focused_plugin_)
    return;
  return focused_plugin_->GetSurroundingText(text, range);
}

bool PepperHelperImpl::IsPluginAcceptingCompositionEvents() const {
  if (!focused_plugin_)
    return false;
  return focused_plugin_->IsPluginAcceptingCompositionEvents();
}

bool PepperHelperImpl::CanComposeInline() const {
  return IsPluginAcceptingCompositionEvents();
}

void PepperHelperImpl::InstanceCreated(
    PepperPluginInstanceImpl* instance) {
  active_instances_.insert(instance);

  // Set the initial focus.
  instance->SetContentAreaFocus(render_view_->has_focus());

  if (!instance->module()->IsProxied()) {
    PepperBrowserConnection* browser_connection =
        PepperBrowserConnection::Get(render_view_);
    browser_connection->DidCreateInProcessInstance(
        instance->pp_instance(),
        render_view_->GetRoutingID(),
        instance->container()->element().document().url(),
        instance->GetPluginURL());
  }
}

void PepperHelperImpl::InstanceDeleted(
    PepperPluginInstanceImpl* instance) {
  active_instances_.erase(instance);

  if (last_mouse_event_target_ == instance)
    last_mouse_event_target_ = NULL;
  if (focused_plugin_ == instance)
    PluginFocusChanged(instance, false);

  if (!instance->module()->IsProxied()) {
    PepperBrowserConnection* browser_connection =
        PepperBrowserConnection::Get(render_view_);
    browser_connection->DidDeleteInProcessInstance(
        instance->pp_instance());
  }
}

void PepperHelperImpl::OnSetFocus(bool has_focus) {
  for (std::set<PepperPluginInstanceImpl*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->SetContentAreaFocus(has_focus);
}

void PepperHelperImpl::PageVisibilityChanged(bool is_visible) {
  for (std::set<PepperPluginInstanceImpl*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->PageVisibilityChanged(is_visible);
}

bool PepperHelperImpl::IsPluginFocused() const {
  return focused_plugin_ != NULL;
}

void PepperHelperImpl::WillHandleMouseEvent() {
  // This method is called for every mouse event that the render view receives.
  // And then the mouse event is forwarded to WebKit, which dispatches it to the
  // event target. Potentially a Pepper plugin will receive the event.
  // In order to tell whether a plugin gets the last mouse event and which it
  // is, we set |last_mouse_event_target_| to NULL here. If a plugin gets the
  // event, it will notify us via DidReceiveMouseEvent() and set itself as
  // |last_mouse_event_target_|.
  last_mouse_event_target_ = NULL;
}

RendererPpapiHost* PepperHelperImpl::CreateExternalPluginModule(
    scoped_refptr<PluginModule> module,
    const base::FilePath& path,
    ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId peer_pid,
    int plugin_child_id) {
  // We don't call PepperPluginRegistry::AddLiveModule, as this module is
  // managed externally.
  return CreateOutOfProcessModule(module.get(),
                                  path,
                                  permissions,
                                  channel_handle,
                                  peer_pid,
                                  plugin_child_id,
                                  true);  // is_external = true
}

void PepperHelperImpl::DidChangeCursor(PepperPluginInstanceImpl* instance,
                                       const WebKit::WebCursorInfo& cursor) {
  // Update the cursor appearance immediately if the requesting plugin is the
  // one which receives the last mouse event. Otherwise, the new cursor won't be
  // picked up until the plugin gets the next input event. That is bad if, e.g.,
  // the plugin would like to set an invisible cursor when there isn't any user
  // input for a while.
  if (instance == last_mouse_event_target_)
    render_view_->didChangeCursor(cursor);
}

void PepperHelperImpl::DidReceiveMouseEvent(
    PepperPluginInstanceImpl* instance) {
  last_mouse_event_target_ = instance;
}

void PepperHelperImpl::OnDestruct() {
  // Nothing to do here. Default implementation in RenderViewObserver does
  // 'delete this' but it's not suitable for PepperHelperImpl because
  // it's non-pointer member in RenderViewImpl.
}

}  // namespace content
