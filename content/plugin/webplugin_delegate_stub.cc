// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/webplugin_delegate_stub.h"

#include "build/build_config.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/webplugin_delegate_impl.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/child/plugin_messages.h"
#include "content/common/cursors/webcursor.h"
#include "content/plugin/plugin_channel.h"
#include "content/plugin/plugin_thread.h"
#include "content/plugin/webplugin_proxy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

using blink::WebBindings;
using blink::WebCursorInfo;

namespace content {

static void DestroyWebPluginAndDelegate(
    base::WeakPtr<NPObjectStub> scriptable_object,
    WebPluginDelegateImpl* delegate,
    WebPlugin* webplugin) {
  // The plugin may not expect us to try to release the scriptable object
  // after calling NPP_Destroy on the instance, so delete the stub now.
  if (scriptable_object.get())
    scriptable_object->DeleteSoon();

  if (delegate)
    delegate->PluginDestroyed();

  delete webplugin;
}

WebPluginDelegateStub::WebPluginDelegateStub(
    const std::string& mime_type, int instance_id, PluginChannel* channel) :
    mime_type_(mime_type),
    instance_id_(instance_id),
    channel_(channel),
    delegate_(NULL),
    webplugin_(NULL),
    in_destructor_(false) {
  DCHECK(channel);
}

WebPluginDelegateStub::~WebPluginDelegateStub() {
  in_destructor_ = true;
  GetContentClient()->SetActiveURL(page_url_);

  if (channel_->in_send()) {
    // The delegate or an npobject is in the callstack, so don't delete it
    // right away.
    base::MessageLoop::current()->PostNonNestableTask(
        FROM_HERE,
        base::Bind(&DestroyWebPluginAndDelegate,
                   plugin_scriptable_object_,
                   delegate_,
                   webplugin_));
  } else {
    // Safe to delete right away.
    DestroyWebPluginAndDelegate(
        plugin_scriptable_object_, delegate_, webplugin_);
  }

  // Remove the NPObject owner mapping for this instance.
  if (delegate_)
    channel_->RemoveMappingForNPObjectOwner(instance_id_);
}

bool WebPluginDelegateStub::OnMessageReceived(const IPC::Message& msg) {
  GetContentClient()->SetActiveURL(page_url_);

  // A plugin can execute a script to delete itself in any of its NPP methods.
  // Hold an extra reference to ourself so that if this does occur and we're
  // handling a sync message, we don't crash when attempting to send a reply.
  // The exception to this is when we're already in the destructor.
  if (!in_destructor_)
    AddRef();

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateStub, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_Init, OnInit)
    IPC_MESSAGE_HANDLER(PluginMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(PluginMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(PluginMsg_Paint, OnPaint)
    IPC_MESSAGE_HANDLER(PluginMsg_DidPaint, OnDidPaint)
    IPC_MESSAGE_HANDLER(PluginMsg_GetPluginScriptableObject,
                        OnGetPluginScriptableObject)
    IPC_MESSAGE_HANDLER(PluginMsg_GetFormValue, OnGetFormValue)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometry, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometrySync, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_SetContentAreaFocus, OnSetContentAreaFocus)
#if defined(OS_WIN) && !defined(USE_AURA)
    IPC_MESSAGE_HANDLER(PluginMsg_ImeCompositionUpdated,
                        OnImeCompositionUpdated)
    IPC_MESSAGE_HANDLER(PluginMsg_ImeCompositionCompleted,
                        OnImeCompositionCompleted)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginMsg_SetWindowFocus, OnSetWindowFocus)
    IPC_MESSAGE_HANDLER(PluginMsg_ContainerHidden, OnContainerHidden)
    IPC_MESSAGE_HANDLER(PluginMsg_ContainerShown, OnContainerShown)
    IPC_MESSAGE_HANDLER(PluginMsg_WindowFrameChanged, OnWindowFrameChanged)
    IPC_MESSAGE_HANDLER(PluginMsg_ImeCompositionCompleted,
                        OnImeCompositionCompleted)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!in_destructor_)
    Release();

  DCHECK(handled);
  return handled;
}

bool WebPluginDelegateStub::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void WebPluginDelegateStub::OnInit(const PluginMsg_Init_Params& params,
                                   bool* transparent,
                                   bool* result) {
  page_url_ = params.page_url;
  GetContentClient()->SetActiveURL(page_url_);

  *transparent = false;
  *result = false;
  if (params.arg_names.size() != params.arg_values.size()) {
    NOTREACHED();
    return;
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::FilePath path =
      command_line.GetSwitchValuePath(switches::kPluginPath);

  webplugin_ = new WebPluginProxy(channel_.get(),
                                  instance_id_,
                                  page_url_,
                                  params.host_render_view_routing_id);
  delegate_ = WebPluginDelegateImpl::Create(webplugin_, path, mime_type_);
  if (delegate_) {
    if (delegate_->GetQuirks() &
        WebPluginDelegateImpl::PLUGIN_QUIRK_DIE_AFTER_UNLOAD) {
      PluginThread::current()->SetForcefullyTerminatePluginProcess();
    }

    webplugin_->set_delegate(delegate_);
    std::vector<std::string> arg_names = params.arg_names;
    std::vector<std::string> arg_values = params.arg_values;

    // Add an NPObject owner mapping for this instance, to support ownership
    // tracking in the renderer.
    channel_->AddMappingForNPObjectOwner(instance_id_,
                                         delegate_->GetPluginNPP());

    *result = delegate_->Initialize(params.url,
                                    arg_names,
                                    arg_values,
                                    params.load_manually);
    *transparent = delegate_->instance()->transparent();
  }
}

void WebPluginDelegateStub::OnSetFocus(bool focused) {
  delegate_->SetFocus(focused);
#if defined(OS_WIN) && !defined(USE_AURA)
  if (focused)
    webplugin_->UpdateIMEStatus();
#endif
}

void WebPluginDelegateStub::OnHandleInputEvent(
    const blink::WebInputEvent *event,
    bool* handled,
    WebCursor* cursor) {
  WebCursor::CursorInfo cursor_info;
  *handled = delegate_->HandleInputEvent(*event, &cursor_info);
  cursor->InitFromCursorInfo(cursor_info);
}

void WebPluginDelegateStub::OnPaint(const gfx::Rect& damaged_rect) {
  webplugin_->Paint(damaged_rect);
}

void WebPluginDelegateStub::OnDidPaint() {
  webplugin_->DidPaint();
}

void WebPluginDelegateStub::OnUpdateGeometry(
    const PluginMsg_UpdateGeometry_Param& param) {
  webplugin_->UpdateGeometry(
      param.window_rect, param.clip_rect,
      param.windowless_buffer0, param.windowless_buffer1,
      param.windowless_buffer_index);
}

void WebPluginDelegateStub::OnGetPluginScriptableObject(int* route_id) {
  NPObject* object = delegate_->GetPluginScriptableObject();
  if (!object) {
    *route_id = MSG_ROUTING_NONE;
    return;
  }

  *route_id = channel_->GenerateRouteID();
  // We will delete the stub immediately before calling PluginDestroyed on the
  // delegate. It will delete itself sooner if the proxy tells it that it has
  // been released, or if the channel to the proxy is closed.
  NPObjectStub* scriptable_stub = new NPObjectStub(
      object, channel_.get(), *route_id,
      webplugin_->host_render_view_routing_id(), page_url_);
  plugin_scriptable_object_ = scriptable_stub->AsWeakPtr();

  // Release ref added by GetPluginScriptableObject (our stub holds its own).
  WebBindings::releaseObject(object);
}

void WebPluginDelegateStub::OnGetFormValue(base::string16* value,
                                           bool* success) {
  *success = false;
  if (!delegate_)
    return;
  *success = delegate_->GetFormValue(value);
}

void WebPluginDelegateStub::OnSetContentAreaFocus(bool has_focus) {
  if (delegate_)
    delegate_->SetContentAreaHasFocus(has_focus);
}

#if defined(OS_WIN) && !defined(USE_AURA)
void WebPluginDelegateStub::OnImeCompositionUpdated(
    const base::string16& text,
    const std::vector<int>& clauses,
    const std::vector<int>& target,
    int cursor_position) {
  if (delegate_)
    delegate_->ImeCompositionUpdated(text, clauses, target, cursor_position);
  webplugin_->UpdateIMEStatus();
}

void WebPluginDelegateStub::OnImeCompositionCompleted(
    const base::string16& text) {
  if (delegate_)
    delegate_->ImeCompositionCompleted(text);
}
#endif

#if defined(OS_MACOSX)
void WebPluginDelegateStub::OnSetWindowFocus(bool has_focus) {
  if (delegate_)
    delegate_->SetWindowHasFocus(has_focus);
}

void WebPluginDelegateStub::OnContainerHidden() {
  if (delegate_)
    delegate_->SetContainerVisibility(false);
}

void WebPluginDelegateStub::OnContainerShown(gfx::Rect window_frame,
                                             gfx::Rect view_frame,
                                             bool has_focus) {
  if (delegate_) {
    delegate_->WindowFrameChanged(window_frame, view_frame);
    delegate_->SetContainerVisibility(true);
    delegate_->SetWindowHasFocus(has_focus);
  }
}

void WebPluginDelegateStub::OnWindowFrameChanged(const gfx::Rect& window_frame,
                                                 const gfx::Rect& view_frame) {
  if (delegate_)
    delegate_->WindowFrameChanged(window_frame, view_frame);
}

void WebPluginDelegateStub::OnImeCompositionCompleted(
    const base::string16& text) {
  if (delegate_)
    delegate_->ImeCompositionCompleted(text);
}
#endif  // OS_MACOSX

}  // namespace content
