// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/webplugin_delegate_stub.h"

#include "build/build_config.h"

#include "base/command_line.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/webplugin_proxy.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/glue/webcursor.h"

#if defined(OS_WIN)
#include "base/scoped_ptr.h"
#include "printing/native_metafile_factory.h"
#include "printing/native_metafile.h"
#endif  // defined(OS_WIN)

#if defined(ENABLE_GPU)
#include "app/gfx/gl/gl_context.h"
#endif

using WebKit::WebBindings;
using WebKit::WebCursorInfo;
using webkit::npapi::WebPlugin;
using webkit::npapi::WebPluginResourceClient;

class FinishDestructionTask : public Task {
 public:
  FinishDestructionTask(webkit::npapi::WebPluginDelegateImpl* delegate,
                        WebPlugin* webplugin)
      : delegate_(delegate), webplugin_(webplugin) {
  }

  void Run() {
    // WebPlugin must outlive WebPluginDelegate.
    if (delegate_)
      delegate_->PluginDestroyed();

    delete webplugin_;
  }

 private:
  webkit::npapi::WebPluginDelegateImpl* delegate_;
  webkit::npapi::WebPlugin* webplugin_;
};

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
  child_process_logging::SetActiveURL(page_url_);

#if defined(ENABLE_GPU)
  // Make sure there is no command buffer before destroying the window handle.
  // The GPU service code might otherwise asynchronously perform an operation
  // using the window handle.
  command_buffer_stub_.reset();
#endif

  if (channel_->in_send()) {
    // The delegate or an npobject is in the callstack, so don't delete it
    // right away.
    MessageLoop::current()->PostNonNestableTask(FROM_HERE,
        new FinishDestructionTask(delegate_, webplugin_));
  } else {
    // Safe to delete right away.
    if (delegate_)
      delegate_->PluginDestroyed();

    delete webplugin_;
  }
}

bool WebPluginDelegateStub::OnMessageReceived(const IPC::Message& msg) {
  child_process_logging::SetActiveURL(page_url_);

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
    IPC_MESSAGE_HANDLER(PluginMsg_Print, OnPrint)
    IPC_MESSAGE_HANDLER(PluginMsg_GetPluginScriptableObject,
                        OnGetPluginScriptableObject)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometry, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometrySync, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_SendJavaScriptStream,
                        OnSendJavaScriptStream)
    IPC_MESSAGE_HANDLER(PluginMsg_SetContentAreaFocus, OnSetContentAreaFocus)
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
    IPC_MESSAGE_HANDLER(PluginMsg_InstallMissingPlugin, OnInstallMissingPlugin)
    IPC_MESSAGE_HANDLER(PluginMsg_HandleURLRequestReply,
                        OnHandleURLRequestReply)
    IPC_MESSAGE_HANDLER(PluginMsg_HTTPRangeRequestReply,
                        OnHTTPRangeRequestReply)
    IPC_MESSAGE_HANDLER(PluginMsg_CreateCommandBuffer,
                        OnCreateCommandBuffer)
    IPC_MESSAGE_HANDLER(PluginMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
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
  child_process_logging::SetActiveURL(page_url_);

  *result = false;
  if (params.arg_names.size() != params.arg_values.size()) {
    NOTREACHED();
    return;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  FilePath path =
      command_line.GetSwitchValuePath(switches::kPluginPath);

  gfx::PluginWindowHandle parent = gfx::kNullPluginWindow;
#if defined(OS_WIN)
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
    *result = delegate_->Initialize(params.url,
                                    params.arg_names,
                                    params.arg_values,
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

void WebPluginDelegateStub::OnPrint(base::SharedMemoryHandle* shared_memory,
                                    uint32* size) {
#if defined(OS_WIN)
  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  if (!metafile->CreateDc(NULL, NULL)) {
    NOTREACHED();
    return;
  }
  HDC hdc = metafile->hdc();
  skia::PlatformDevice::InitializeDC(hdc);
  delegate_->Print(hdc);
  if (!metafile->CloseDc()) {
    NOTREACHED();
    return;
  }

  *size = metafile->GetDataSize();
  DCHECK(*size);
  base::SharedMemory shared_buf;
  CreateSharedBuffer(*size, &shared_buf, shared_memory);

  // Retrieve a copy of the data.
  bool success = metafile->GetData(shared_buf.memory(), *size);
  DCHECK(success);
#else
  // TODO(port): plugin printing.
  NOTIMPLEMENTED();
#endif
}

void WebPluginDelegateStub::OnUpdateGeometry(
    const PluginMsg_UpdateGeometry_Param& param) {
  webplugin_->UpdateGeometry(
      param.window_rect, param.clip_rect,
      param.windowless_buffer, param.background_buffer,
      param.transparent
#if defined(OS_MACOSX)
      ,
      param.ack_key
#endif
      );
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

void WebPluginDelegateStub::OnInstallMissingPlugin() {
  delegate_->InstallMissingPlugin();
}

void WebPluginDelegateStub::OnCreateCommandBuffer(int* route_id) {
  *route_id = 0;
#if defined(ENABLE_GPU)
  // Fail to create the command buffer if some GL implementation cannot be
  // initialized.
  if (!gfx::GLContext::InitializeOneOff())
    return;

  command_buffer_stub_.reset(new CommandBufferStub(
      channel_,
      instance_id_,
      delegate_->windowed_handle()));

  *route_id = command_buffer_stub_->route_id();
#endif  // ENABLE_GPU
}

void WebPluginDelegateStub::OnDestroyCommandBuffer() {
#if defined(ENABLE_GPU)
  command_buffer_stub_.reset();
#endif
}

void WebPluginDelegateStub::CreateSharedBuffer(
    uint32 size,
    base::SharedMemory* shared_buf,
    base::SharedMemoryHandle* remote_handle) {
  if (!shared_buf->CreateAndMapAnonymous(size)) {
    NOTREACHED();
    shared_buf->Close();
    return;
  }

#if defined(OS_WIN)
  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                shared_buf->handle(),
                                channel_->renderer_handle(),
                                remote_handle, 0, FALSE,
                                DUPLICATE_SAME_ACCESS);
  DCHECK_NE(result, 0);

  // If the calling function's shared_buf is on the stack, its destructor will
  // close the shared memory buffer handle. This is fine since we already
  // duplicated the handle to the renderer process so it will stay "alive".
#else
  // TODO(port): this should use TransportDIB.
  NOTIMPLEMENTED();
#endif
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

