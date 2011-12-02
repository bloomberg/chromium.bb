// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/webplugin_delegate_stub.h"

#include "build/build_config.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "content/common/npobject_stub.h"
#include "content/common/plugin_messages.h"
#include "content/plugin/plugin_channel.h"
#include "content/plugin/plugin_thread.h"
#include "content/plugin/webplugin_proxy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/glue/webcursor.h"

using WebKit::WebBindings;
using WebKit::WebCursorInfo;
using webkit::npapi::WebPlugin;
using webkit::npapi::WebPluginResourceClient;

void FinishDestructionCallback(webkit::npapi::WebPluginDelegateImpl* delegate,
                               WebPlugin* webplugin) {
  // WebPlugin must outlive WebPluginDelegate.
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
  content::GetContentClient()->SetActiveURL(page_url_);

  if (channel_->in_send()) {
    // The delegate or an npobject is in the callstack, so don't delete it
    // right away.
    MessageLoop::current()->PostNonNestableTask(FROM_HERE,
        base::Bind(&FinishDestructionCallback, delegate_, webplugin_));
  } else {
    // Safe to delete right away.
    if (delegate_)
      delegate_->PluginDestroyed();

    delete webplugin_;
  }
}

bool WebPluginDelegateStub::OnMessageReceived(const IPC::Message& msg) {
  content::GetContentClient()->SetActiveURL(page_url_);

  // A plugin can execute a script to delete itself in any of its NPP methods.
  // Hold an extra reference to ourself so that if this does occur and we're
  // handling a sync message, we don't crash when attempting to send a reply.
  // The exception to this is when we're already in the destructor.
  if (!in_destructor_)
    AddRef();

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateStub, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_Init, OnInit)
    IPC_MESSAGE_HANDLER(PluginMsg_WillSendRequest, OnWillSendRequest)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveResponse, OnDidReceiveResponse)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveData, OnDidReceiveData)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFinishLoading, OnDidFinishLoading)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFail, OnDidFail)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFinishLoadWithReason,
                        OnDidFinishLoadWithReason)
    IPC_MESSAGE_HANDLER(PluginMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(PluginMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(PluginMsg_Paint, OnPaint)
    IPC_MESSAGE_HANDLER(PluginMsg_DidPaint, OnDidPaint)
    IPC_MESSAGE_HANDLER(PluginMsg_GetPluginScriptableObject,
                        OnGetPluginScriptableObject)
    IPC_MESSAGE_HANDLER(PluginMsg_GetFormValue, OnGetFormValue)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometry, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometrySync, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_SendJavaScriptStream,
                        OnSendJavaScriptStream)
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
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveManualResponse,
                        OnDidReceiveManualResponse)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveManualData, OnDidReceiveManualData)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFinishManualLoading,
                        OnDidFinishManualLoading)
    IPC_MESSAGE_HANDLER(PluginMsg_DidManualLoadFail, OnDidManualLoadFail)
    IPC_MESSAGE_HANDLER(PluginMsg_HandleURLRequestReply,
                        OnHandleURLRequestReply)
    IPC_MESSAGE_HANDLER(PluginMsg_HTTPRangeRequestReply,
                        OnHTTPRangeRequestReply)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginMsg_SetFakeAcceleratedSurfaceWindowHandle,
                        OnSetFakeAcceleratedSurfaceWindowHandle)
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
                                   bool* result) {
  page_url_ = params.page_url;
  content::GetContentClient()->SetActiveURL(page_url_);

  *result = false;
  if (params.arg_names.size() != params.arg_values.size()) {
    NOTREACHED();
    return;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  FilePath path =
      command_line.GetSwitchValuePath(switches::kPluginPath);

  gfx::PluginWindowHandle parent = gfx::kNullPluginWindow;
#if defined(USE_AURA)
  // Nothing.
#elif defined(OS_WIN)
  parent = gfx::NativeViewFromId(params.containing_window);
#elif defined(OS_LINUX)
  // This code is disabled, See issue 17110.
  // The problem is that the XID can change at arbitrary times (e.g. when the
  // tab is detached then reattached), so we need to be able to track these
  // changes, and let the PluginInstance know.
  // PluginThread::current()->Send(new PluginProcessHostMsg_MapNativeViewId(
  //    params.containing_window, &parent));
#endif

  webplugin_ = new WebPluginProxy(
      channel_, instance_id_, page_url_, params.containing_window,
      params.host_render_view_routing_id);
  delegate_ = webkit::npapi::WebPluginDelegateImpl::Create(
      path, mime_type_, parent);
  if (delegate_) {
    webplugin_->set_delegate(delegate_);
    std::vector<std::string> arg_names = params.arg_names;
    std::vector<std::string> arg_values = params.arg_values;

    if (path.value() == webkit::npapi::kDefaultPluginLibraryName) {
      // Add the renderer process id and Render view routing id to the list of
      // parameters passed to the plugin.
      arg_names.push_back(content::kDefaultPluginRenderViewId);
      arg_values.push_back(base::IntToString(
          params.host_render_view_routing_id));

      arg_names.push_back(content::kDefaultPluginRenderProcessId);
      arg_values.push_back(base::IntToString(channel_->renderer_id()));
    }
    *result = delegate_->Initialize(params.url,
                                    arg_names,
                                    arg_values,
                                    webplugin_,
                                    params.load_manually);
  }
}

void WebPluginDelegateStub::OnWillSendRequest(int id, const GURL& url,
                                              int http_status_code) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->WillSendRequest(url, http_status_code);
}

void WebPluginDelegateStub::OnDidReceiveResponse(
    const PluginMsg_DidReceiveResponseParams& params) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(params.id);
  if (!client)
    return;

  client->DidReceiveResponse(params.mime_type,
                             params.headers,
                             params.expected_length,
                             params.last_modified,
                             params.request_is_seekable);
}

void WebPluginDelegateStub::OnDidReceiveData(int id,
                                             const std::vector<char>& buffer,
                                             int data_offset) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->DidReceiveData(&buffer.front(), static_cast<int>(buffer.size()),
                         data_offset);
}

void WebPluginDelegateStub::OnDidFinishLoading(int id) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->DidFinishLoading();
}

void WebPluginDelegateStub::OnDidFail(int id) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->DidFail();
}

void WebPluginDelegateStub::OnDidFinishLoadWithReason(
    const GURL& url, int reason, int notify_id) {
  delegate_->DidFinishLoadWithReason(url, reason, notify_id);
}

void WebPluginDelegateStub::OnSetFocus(bool focused) {
  delegate_->SetFocus(focused);
}

void WebPluginDelegateStub::OnHandleInputEvent(
    const WebKit::WebInputEvent *event,
    bool* handled,
    WebCursor* cursor) {
  WebCursorInfo cursor_info;
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
      param.windowless_buffer_index, param.background_buffer,
      param.transparent);
}

void WebPluginDelegateStub::OnGetPluginScriptableObject(int* route_id) {
  NPObject* object = delegate_->GetPluginScriptableObject();
  if (!object) {
    *route_id = MSG_ROUTING_NONE;
    return;
  }

  *route_id = channel_->GenerateRouteID();
  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  new NPObjectStub(
      object, channel_.get(), *route_id, webplugin_->containing_window(),
      page_url_);

  // Release ref added by GetPluginScriptableObject (our stub holds its own).
  WebBindings::releaseObject(object);
}

void WebPluginDelegateStub::OnGetFormValue(string16* value, bool* success) {
  *success = false;
  if (!delegate_)
    return;
  *success = delegate_->GetFormValue(value);
}

void WebPluginDelegateStub::OnSendJavaScriptStream(const GURL& url,
                                                   const std::string& result,
                                                   bool success,
                                                   int notify_id) {
  delegate_->SendJavaScriptStream(url, result, success, notify_id);
}

void WebPluginDelegateStub::OnSetContentAreaFocus(bool has_focus) {
  if (delegate_)
    delegate_->SetContentAreaHasFocus(has_focus);
}

#if defined(OS_WIN) && !defined(USE_AURA)
void WebPluginDelegateStub::OnImeCompositionUpdated(
    const string16& text,
    const std::vector<int>& clauses,
    const std::vector<int>& target,
    int cursor_position) {
  if (delegate_)
    delegate_->ImeCompositionUpdated(text, clauses, target, cursor_position);
#if defined(OS_WIN) && !defined(USE_AURA)
  webplugin_->UpdateIMEStatus();
#endif
}

void WebPluginDelegateStub::OnImeCompositionCompleted(const string16& text) {
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

void WebPluginDelegateStub::OnImeCompositionCompleted(const string16& text) {
  if (delegate_)
    delegate_->ImeCompositionCompleted(text);
}
#endif  // OS_MACOSX

void WebPluginDelegateStub::OnDidReceiveManualResponse(
    const GURL& url,
    const PluginMsg_DidReceiveResponseParams& params) {
  delegate_->DidReceiveManualResponse(url, params.mime_type, params.headers,
                                      params.expected_length,
                                      params.last_modified);
}

void WebPluginDelegateStub::OnDidReceiveManualData(
    const std::vector<char>& buffer) {
  delegate_->DidReceiveManualData(&buffer.front(),
                                  static_cast<int>(buffer.size()));
}

void WebPluginDelegateStub::OnDidFinishManualLoading() {
  delegate_->DidFinishManualLoading();
}

void WebPluginDelegateStub::OnDidManualLoadFail() {
  delegate_->DidManualLoadFail();
}

void WebPluginDelegateStub::OnHandleURLRequestReply(
    unsigned long resource_id, const GURL& url, int notify_id) {
  WebPluginResourceClient* resource_client =
      delegate_->CreateResourceClient(resource_id, url, notify_id);
  webplugin_->OnResourceCreated(resource_id, resource_client);
}

void WebPluginDelegateStub::OnHTTPRangeRequestReply(
    unsigned long resource_id, int range_request_id) {
  WebPluginResourceClient* resource_client =
      delegate_->CreateSeekableResourceClient(resource_id, range_request_id);
  webplugin_->OnResourceCreated(resource_id, resource_client);
}

#if defined(OS_MACOSX)
void WebPluginDelegateStub::OnSetFakeAcceleratedSurfaceWindowHandle(
    gfx::PluginWindowHandle window) {
  delegate_->set_windowed_handle(window);
}
#endif
