// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webplugin_delegate_proxy.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

#include <algorithm>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "ipc/ipc_channel_handle.h"
#include "net/base/mime_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_WIN)
#include "printing/native_metafile_factory.h"
#include "printing/native_metafile.h"
#endif

using WebKit::WebBindings;
using WebKit::WebCursorInfo;
using WebKit::WebDragData;
using WebKit::WebInputEvent;
using WebKit::WebString;
using WebKit::WebView;

// Proxy for WebPluginResourceClient.  The object owns itself after creation,
// deleting itself after its callback has been called.
class ResourceClientProxy : public webkit::npapi::WebPluginResourceClient {
 public:
  ResourceClientProxy(PluginChannelHost* channel, int instance_id)
    : channel_(channel), instance_id_(instance_id), resource_id_(0),
      multibyte_response_expected_(false) {
  }

  ~ResourceClientProxy() {
  }

  void Initialize(unsigned long resource_id, const GURL& url, int notify_id) {
    resource_id_ = resource_id;
    channel_->Send(new PluginMsg_HandleURLRequestReply(
        instance_id_, resource_id, url, notify_id));
  }

  void InitializeForSeekableStream(unsigned long resource_id,
                                   int range_request_id) {
    resource_id_ = resource_id;
    multibyte_response_expected_ = true;
    channel_->Send(new PluginMsg_HTTPRangeRequestReply(
        instance_id_, resource_id, range_request_id));
  }

  // PluginResourceClient implementation:
  void WillSendRequest(const GURL& url, int http_status_code) {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_WillSendRequest(instance_id_, resource_id_,
                                                 url, http_status_code));
  }

  void DidReceiveResponse(const std::string& mime_type,
                          const std::string& headers,
                          uint32 expected_length,
                          uint32 last_modified,
                          bool request_is_seekable) {
    DCHECK(channel_ != NULL);
    PluginMsg_DidReceiveResponseParams params;
    params.id = resource_id_;
    params.mime_type = mime_type;
    params.headers = headers;
    params.expected_length = expected_length;
    params.last_modified = last_modified;
    params.request_is_seekable = request_is_seekable;
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveResponse(instance_id_, params));
  }

  void DidReceiveData(const char* buffer, int length, int data_offset) {
    DCHECK(channel_ != NULL);
    DCHECK_GT(length, 0);
    std::vector<char> data;
    data.resize(static_cast<size_t>(length));
    memcpy(&data.front(), buffer, length);
    // Grab a reference on the underlying channel so it does not get
    // deleted from under us.
    scoped_refptr<PluginChannelHost> channel_ref(channel_);
    channel_->Send(new PluginMsg_DidReceiveData(instance_id_, resource_id_,
                                                data, data_offset));
  }

  void DidFinishLoading() {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_DidFinishLoading(instance_id_, resource_id_));
    channel_ = NULL;
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  void DidFail() {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_DidFail(instance_id_, resource_id_));
    channel_ = NULL;
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  bool IsMultiByteResponseExpected() {
    return multibyte_response_expected_;
  }

  int ResourceId() {
    return resource_id_;
  }

 private:
  scoped_refptr<PluginChannelHost> channel_;
  int instance_id_;
  unsigned long resource_id_;
  // Set to true if the response expected is a multibyte response.
  // For e.g. response for a HTTP byte range request.
  bool multibyte_response_expected_;
};

#if defined(OS_MACOSX)
static void ReleaseTransportDIB(TransportDIB* dib) {
  if (dib) {
    IPC::Message* message = new ViewHostMsg_FreeTransportDIB(dib->id());
    RenderThread::current()->Send(message);
  }
}
#endif

WebPluginDelegateProxy::WebPluginDelegateProxy(
    const std::string& mime_type,
    const base::WeakPtr<RenderView>& render_view)
    : render_view_(render_view),
      plugin_(NULL),
      uses_shared_bitmaps_(false),
      window_(gfx::kNullPluginWindow),
      mime_type_(mime_type),
      instance_id_(MSG_ROUTING_NONE),
      npobject_(NULL),
      sad_plugin_(NULL),
      invalidate_pending_(false),
      transparent_(false),
      page_url_(render_view_->webview()->mainFrame()->url()) {
}

WebPluginDelegateProxy::~WebPluginDelegateProxy() {
#if defined(OS_MACOSX)
  // Ask the browser to release old TransportDIB objects for which no
  // PluginHostMsg_UpdateGeometry_ACK was ever received from the plugin
  // process.
  for (OldTransportDIBMap::iterator iterator = old_transport_dibs_.begin();
       iterator != old_transport_dibs_.end();
       ++iterator) {
    ReleaseTransportDIB(iterator->second.get());
  }

  // Ask the browser to release the "live" TransportDIB object.
  ReleaseTransportDIB(transport_store_.get());
  DCHECK(!background_store_.get());
#endif
}

void WebPluginDelegateProxy::PluginDestroyed() {
#if defined(OS_MACOSX)
  // Ensure that the renderer doesn't think the plugin still has focus.
  if (render_view_)
    render_view_->PluginFocusChanged(false, instance_id_);
#endif

  if (window_)
    WillDestroyWindow();

  if (render_view_)
    render_view_->UnregisterPluginDelegate(this);

  if (channel_host_) {
    Send(new PluginMsg_DestroyInstance(instance_id_));

    // Must remove the route after sending the destroy message, since
    // RemoveRoute can lead to all the outstanding NPObjects being told the
    // channel went away if this was the last instance.
    channel_host_->RemoveRoute(instance_id_);

    // Release the channel host now. If we are is the last reference to the
    // channel, this avoids a race where this renderer asks a new connection to
    // the same plugin between now and the time 'this' is actually deleted.
    // Destroying the channel host is what releases the channel name -> FD
    // association on POSIX, and if we ask for a new connection before it is
    // released, the plugin will give us a new FD, and we'll assert when trying
    // to associate it with the channel name.
    channel_host_ = NULL;
  }

  if (window_script_object_) {
    // The ScriptController deallocates this object independent of its ref count
    // to avoid leaks if the plugin forgets to release it.  So mark the object
    // invalid to avoid accessing it past this point.  Note: only do this after
    // the DestroyInstance message in case the window object is scripted by the
    // plugin in NPP_Destroy.
    window_script_object_->OnPluginDestroyed();
  }

  plugin_ = NULL;

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// Returns true if the given Silverlight 'background' value corresponds to
// one that should make the plugin transparent. See:
// http://msdn.microsoft.com/en-us/library/cc838148(VS.95).aspx
// for possible values.
static bool SilverlightColorIsTransparent(const std::string& color) {
  if (StartsWithASCII(color, "#", false)) {
    // If it's #ARGB or #AARRGGBB check the alpha; if not it's an RGB form and
    // it's not transparent.
    if ((color.length() == 5 && !StartsWithASCII(color, "#F", false)) ||
        (color.length() == 9 && !StartsWithASCII(color, "#FF", false)))
      return true;
  } else if (StartsWithASCII(color, "sc#", false)) {
    // It's either sc#A,R,G,B or sc#R,G,B; if the former, check the alpha.
    if (color.length() < 4)
      return false;
    std::string value_string = color.substr(3, std::string::npos);
    std::vector<std::string> components;
    base::SplitString(value_string, ',', &components);
    if (components.size() == 4 && !StartsWithASCII(components[0], "1", false))
      return true;
  } else if (LowerCaseEqualsASCII(color, "transparent")) {
    return true;
  }
  // Anything else is a named, opaque color or an RGB form with no alpha.
  return false;
}

bool WebPluginDelegateProxy::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    webkit::npapi::WebPlugin* plugin,
    bool load_manually) {
  IPC::ChannelHandle channel_handle;
  if (!RenderThread::current()->Send(new ViewHostMsg_OpenChannelToPlugin(
          render_view_->routing_id(), url, mime_type_, &channel_handle,
          &info_))) {
    return false;
  }

  if (channel_handle.name.empty()) {
    // We got an invalid handle.  Either the plugin couldn't be found (which
    // shouldn't happen, since if we got here the plugin should exist) or the
    // plugin crashed on initialization.
    if (!info_.path.empty()) {
      render_view_->PluginCrashed(info_.path);

      // Return true so that the plugin widget is created and we can paint the
      // crashed plugin there.
      return true;
    }
    return false;
  }

  scoped_refptr<PluginChannelHost> channel_host(
      PluginChannelHost::GetPluginChannelHost(
          channel_handle, ChildProcess::current()->io_message_loop()));
  if (!channel_host.get())
    return false;

  int instance_id;
  bool result = channel_host->Send(new PluginMsg_CreateInstance(
      mime_type_, &instance_id));
  if (!result)
    return false;

  channel_host_ = channel_host;
  instance_id_ = instance_id;

  channel_host_->AddRoute(instance_id_, this, NULL);

  // Now tell the PluginInstance in the plugin process to initialize.
  PluginMsg_Init_Params params;
  params.containing_window = render_view_->host_window();
  params.url = url;
  params.page_url = page_url_;
  params.arg_names = arg_names;
  params.arg_values = arg_values;
  params.host_render_view_routing_id = render_view_->routing_id();

  bool flash =
      LowerCaseEqualsASCII(mime_type_, "application/x-shockwave-flash");
  bool silverlight =
      StartsWithASCII(mime_type_, "application/x-silverlight", false);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    if ((flash && LowerCaseEqualsASCII(arg_names[i], "wmode") &&
        LowerCaseEqualsASCII(arg_values[i], "transparent")) ||
        (silverlight && LowerCaseEqualsASCII(arg_names[i], "background") &&
         SilverlightColorIsTransparent(arg_values[i]))) {
      transparent_ = true;
    }
  }
#if defined(OS_MACOSX)
  // Unless we have a real way to support accelerated (3D) drawing on Macs
  // (which for now at least means the Core Animation drawing model), ask
  // Flash to use windowless mode so that it use CoreGraphics instead of opening
  // OpenGL contexts overlaying the browser window (which requires a very
  // expensive extra copy for us).
  if (!transparent_ && mime_type_ == "application/x-shockwave-flash") {
    bool force_opaque_mode = false;
    if (StartsWith(info_.version, ASCIIToUTF16("10.0"), false) ||
        StartsWith(info_.version, ASCIIToUTF16("9."), false)) {
      // Older versions of Flash don't support CA (and they assume QuickDraw
      // support, so we can't rely on negotiation to do the right thing).
      force_opaque_mode = true;
    } else {
      // Flash 10.1 doesn't respect QuickDraw negotiation either, so we still
      // have to force opaque mode on 10.5 (where it doesn't use CA).
      int32 major, minor, bugfix;
      base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
      if (major < 10 || (major == 10 && minor < 6))
        force_opaque_mode = true;
    }

    if (force_opaque_mode) {
      params.arg_names.push_back("wmode");
      params.arg_values.push_back("opaque");
    }
  }
#endif
  params.load_manually = load_manually;

  plugin_ = plugin;

  result = false;
  IPC::Message* msg = new PluginMsg_Init(instance_id_, params, &result);
  Send(msg);

  render_view_->RegisterPluginDelegate(this);

  return result;
}

bool WebPluginDelegateProxy::Send(IPC::Message* msg) {
  if (!channel_host_) {
    DLOG(WARNING) << "dropping message because channel host is null";
    delete msg;
    return false;
  }

  return channel_host_->Send(msg);
}

void WebPluginDelegateProxy::SendJavaScriptStream(const GURL& url,
                                                  const std::string& result,
                                                  bool success,
                                                  int notify_id) {
  Send(new PluginMsg_SendJavaScriptStream(
      instance_id_, url, result, success, notify_id));
}

void WebPluginDelegateProxy::DidReceiveManualResponse(
    const GURL& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length,
    uint32 last_modified) {
  PluginMsg_DidReceiveResponseParams params;
  params.id = 0;
  params.mime_type = mime_type;
  params.headers = headers;
  params.expected_length = expected_length;
  params.last_modified = last_modified;
  Send(new PluginMsg_DidReceiveManualResponse(instance_id_, url, params));
}

void WebPluginDelegateProxy::DidReceiveManualData(const char* buffer,
                                                  int length) {
  DCHECK_GT(length, 0);
  std::vector<char> data;
  data.resize(static_cast<size_t>(length));
  memcpy(&data.front(), buffer, length);
  Send(new PluginMsg_DidReceiveManualData(instance_id_, data));
}

void WebPluginDelegateProxy::DidFinishManualLoading() {
  Send(new PluginMsg_DidFinishManualLoading(instance_id_));
}

void WebPluginDelegateProxy::DidManualLoadFail() {
  Send(new PluginMsg_DidManualLoadFail(instance_id_));
}

void WebPluginDelegateProxy::InstallMissingPlugin() {
  Send(new PluginMsg_InstallMissingPlugin(instance_id_));
}

bool WebPluginDelegateProxy::OnMessageReceived(const IPC::Message& msg) {
  child_process_logging::SetActiveURL(page_url_);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateProxy, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindow, OnSetWindow)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetWindowlessPumpEvent,
                        OnSetWindowlessPumpEvent)
#endif
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelResource, OnCancelResource)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InvalidateRect, OnInvalidateRect)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetWindowScriptNPObject,
                        OnGetWindowScriptNPObject)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetPluginElement,
                        OnGetPluginElement)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(PluginHostMsg_MissingPluginStatus,
                        OnMissingPluginStatus)
    IPC_MESSAGE_HANDLER(PluginHostMsg_URLRequest, OnHandleURLRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelDocumentLoad, OnCancelDocumentLoad)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InitiateHTTPRangeRequest,
                        OnInitiateHTTPRangeRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_DeferResourceLoading,
                        OnDeferResourceLoading)

#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginHostMsg_FocusChanged,
                        OnFocusChanged);
    IPC_MESSAGE_HANDLER(PluginHostMsg_StartIme,
                        OnStartIme);
    IPC_MESSAGE_HANDLER(PluginHostMsg_BindFakePluginWindowHandle,
                        OnBindFakePluginWindowHandle);
    IPC_MESSAGE_HANDLER(PluginHostMsg_UpdateGeometry_ACK,
                        OnUpdateGeometry_ACK)
    // Used only on 10.6 and later.
    IPC_MESSAGE_HANDLER(PluginHostMsg_AcceleratedSurfaceSetIOSurface,
                        OnAcceleratedSurfaceSetIOSurface)
    // Used on 10.5 and earlier.
    IPC_MESSAGE_HANDLER(PluginHostMsg_AcceleratedSurfaceSetTransportDIB,
                        OnAcceleratedSurfaceSetTransportDIB)
    IPC_MESSAGE_HANDLER(PluginHostMsg_AllocTransportDIB,
                        OnAcceleratedSurfaceAllocTransportDIB)
    IPC_MESSAGE_HANDLER(PluginHostMsg_FreeTransportDIB,
                        OnAcceleratedSurfaceFreeTransportDIB)
    IPC_MESSAGE_HANDLER(PluginHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#endif
    IPC_MESSAGE_HANDLER(PluginHostMsg_URLRedirectResponse,
                        OnURLRedirectResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void WebPluginDelegateProxy::OnChannelError() {
  if (plugin_) {
    if (window_) {
      // The actual WebPluginDelegate never got a chance to tell the WebPlugin
      // its window was going away. Do it on its behalf.
      WillDestroyWindow();
    }
    plugin_->Invalidate();
  }
  if (!channel_host_->expecting_shutdown())
    render_view_->PluginCrashed(info_.path);

#if defined(OS_MACOSX)
  // Ensure that the renderer doesn't think the plugin still has focus.
  if (render_view_)
    render_view_->PluginFocusChanged(false, instance_id_);
#endif
}

void WebPluginDelegateProxy::UpdateGeometry(const gfx::Rect& window_rect,
                                            const gfx::Rect& clip_rect) {
  // window_rect becomes either a window in native windowing system
  // coords, or a backing buffer.  In either case things will go bad
  // if the rectangle is very large.
  if (window_rect.width() < 0  || window_rect.width() > (1<<15) ||
      window_rect.height() < 0 || window_rect.height() > (1<<15) ||
      // Clip to 8m pixels; we know this won't overflow due to above checks.
      window_rect.width() * window_rect.height() > (8<<20)) {
    return;
  }

  plugin_rect_ = window_rect;
  clip_rect_ = clip_rect;

  bool bitmaps_changed = false;

  PluginMsg_UpdateGeometry_Param param;
#if defined(OS_MACOSX)
  param.ack_key = -1;
#endif

  if (uses_shared_bitmaps_) {
    if (!backing_store_canvas_.get() ||
        (window_rect.width() != backing_store_canvas_->getDevice()->width() ||
         window_rect.height() != backing_store_canvas_->getDevice()->height()))
    {
      bitmaps_changed = true;

      bool needs_background_store = transparent_;
#if defined(OS_MACOSX)
      // We don't support transparency under QuickDraw, and CoreGraphics
      // preserves transparency information (and does the compositing itself)
      // so plugins don't need access to the page background.
      needs_background_store = false;
      if (transport_store_.get()) {
        // ResetWindowlessBitmaps inserts the old TransportDIBs into
        // old_transport_dibs_ using the transport store's file descriptor as
        // the key.  The constraints on the keys are that -1 is reserved
        // to mean "no ACK required," and in-flight keys must be unique.
        // File descriptors will never be -1, and because they won't be closed
        // until receipt of the ACK, they're unique.
        param.ack_key = transport_store_->handle().fd;
      }
#endif

      // Create a shared memory section that the plugin paints into
      // asynchronously.
      ResetWindowlessBitmaps();
      if (!window_rect.IsEmpty()) {
        if (!CreateSharedBitmap(&transport_store_, &transport_store_canvas_) ||
#if defined(OS_WIN)
            !CreateSharedBitmap(&backing_store_, &backing_store_canvas_) ||
#else
            !CreateLocalBitmap(&backing_store_, &backing_store_canvas_) ||
#endif
            (needs_background_store &&
             !CreateSharedBitmap(&background_store_,
                                 &background_store_canvas_))) {
          DCHECK(false);
          ResetWindowlessBitmaps();
          return;
        }
      }
    }
  }

  param.window_rect = window_rect;
  param.clip_rect = clip_rect;
  param.windowless_buffer = TransportDIB::DefaultHandleValue();
  param.background_buffer = TransportDIB::DefaultHandleValue();
  param.transparent = transparent_;

#if defined(OS_POSIX)
  // If we're using POSIX mmap'd TransportDIBs, sending the handle across
  // IPC establishes a new mapping rather than just sending a window ID,
  // so only do so if we've actually recreated the shared memory bitmaps.
  if (bitmaps_changed)
#endif
  {
    if (transport_store_.get())
      param.windowless_buffer = transport_store_->handle();

    if (background_store_.get())
      param.background_buffer = background_store_->handle();
  }

  IPC::Message* msg;
#if defined (OS_WIN)
  if (UseSynchronousGeometryUpdates()) {
    msg = new PluginMsg_UpdateGeometrySync(instance_id_, param);
  } else  // NOLINT
#endif
  {
    msg = new PluginMsg_UpdateGeometry(instance_id_, param);
    msg->set_unblock(true);
  }

  Send(msg);
}

void WebPluginDelegateProxy::ResetWindowlessBitmaps() {
#if defined(OS_MACOSX)
  DCHECK(!background_store_.get());
  // The Mac TransportDIB implementation uses base::SharedMemory, which
  // cannot be disposed of if an in-flight UpdateGeometry message refers to
  // the shared memory file descriptor.  The old_transport_dibs_ map holds
  // old TransportDIBs waiting to die, keyed by the |ack_key| values used in
  // UpdateGeometry messages.  When an UpdateGeometry_ACK message arrives,
  // the associated TransportDIB can be released.
  if (transport_store_.get()) {
    int ack_key = transport_store_->handle().fd;

    DCHECK_NE(ack_key, -1);

    // DCHECK_EQ does not work with base::hash_map.
    DCHECK(old_transport_dibs_.find(ack_key) == old_transport_dibs_.end());

    // Stash the old TransportDIB in the map.  It'll be released when an
    // ACK message comes in.
    old_transport_dibs_[ack_key] =
        linked_ptr<TransportDIB>(transport_store_.release());
  }
#else
  transport_store_.reset();
  background_store_.reset();
#endif
#if defined(OS_WIN)
  backing_store_.reset();
#else
  backing_store_.resize(0);
#endif

  backing_store_canvas_.reset();
  transport_store_canvas_.reset();
  background_store_canvas_.reset();
  backing_store_painted_ = gfx::Rect();
}

static size_t BitmapSizeForPluginRect(const gfx::Rect& plugin_rect) {
  const size_t stride =
      skia::PlatformCanvas::StrideForWidth(plugin_rect.width());
  return stride * plugin_rect.height();
}

#if !defined(OS_WIN)
bool WebPluginDelegateProxy::CreateLocalBitmap(
    std::vector<uint8>* memory,
    scoped_ptr<skia::PlatformCanvas>* canvas) {
  const size_t size = BitmapSizeForPluginRect(plugin_rect_);
  memory->resize(size);
  if (memory->size() != size)
    return false;
  canvas->reset(new skia::PlatformCanvas(
      plugin_rect_.width(), plugin_rect_.height(), true, &((*memory)[0])));
  return true;
}
#endif

bool WebPluginDelegateProxy::CreateSharedBitmap(
    scoped_ptr<TransportDIB>* memory,
    scoped_ptr<skia::PlatformCanvas>* canvas) {
  const size_t size = BitmapSizeForPluginRect(plugin_rect_);
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  memory->reset(TransportDIB::Create(size, 0));
  if (!memory->get())
    return false;
#endif
#if defined(OS_MACOSX)
  TransportDIB::Handle handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(size, true, &handle);
  if (!RenderThread::current()->Send(msg))
    return false;
  if (handle.fd < 0)
    return false;
  memory->reset(TransportDIB::Map(handle));
#else
  static uint32 sequence_number = 0;
  memory->reset(TransportDIB::Create(size, sequence_number++));
#endif
  canvas->reset((*memory)->GetPlatformCanvas(plugin_rect_.width(),
                                             plugin_rect_.height()));
  return !!canvas->get();
}

#if defined(OS_MACOSX)
// Flips |rect| vertically within an enclosing rect with height |height|.
// Intended for converting rects between flipped and non-flipped contexts.
static void FlipRectVerticallyWithHeight(gfx::Rect* rect, int height) {
  rect->set_y(height - rect->y() - rect->height());
}
#endif

void WebPluginDelegateProxy::Paint(WebKit::WebCanvas* canvas,
                                   const gfx::Rect& damaged_rect) {
  // Limit the damaged rectangle to whatever is contained inside the plugin
  // rectangle, as that's the rectangle that we'll actually draw.
  gfx::Rect rect = damaged_rect.Intersect(plugin_rect_);

  // If the plugin is no longer connected (channel crashed) draw a crashed
  // plugin bitmap
  if (!channel_host_ || !channel_host_->channel_valid()) {
    PaintSadPlugin(canvas, rect);
    return;
  }

  if (!uses_shared_bitmaps_)
    return;

  // We got a paint before the plugin's coordinates, so there's no buffer to
  // copy from.
  if (!backing_store_canvas_.get())
    return;

  // We're using the native OS APIs from here on out.
#if WEBKIT_USING_SKIA
  gfx::NativeDrawingContext context = canvas->beginPlatformPaint();
#elif WEBKIT_USING_CG
  gfx::NativeDrawingContext context = canvas;
#endif

  gfx::Rect offset_rect = rect;
  offset_rect.Offset(-plugin_rect_.x(), -plugin_rect_.y());
  gfx::Rect canvas_rect = offset_rect;
#if defined(OS_MACOSX)
  // The canvases are flipped relative to the context, so flip the rect too.
  FlipRectVerticallyWithHeight(&canvas_rect, plugin_rect_.height());
#endif

  bool background_changed = false;
  if (background_store_canvas_.get() && BackgroundChanged(context, rect)) {
    background_changed = true;
    gfx::Rect flipped_offset_rect = offset_rect;
    BlitContextToCanvas(background_store_canvas_.get(), canvas_rect,
                        context, rect.origin());
  }

  if (background_changed || !backing_store_painted_.Contains(offset_rect)) {
    Send(new PluginMsg_Paint(instance_id_, offset_rect));
    CopyFromTransportToBacking(offset_rect);
  }

  BlitCanvasToContext(context, rect, backing_store_canvas_.get(),
                      canvas_rect.origin());

  if (invalidate_pending_) {
    // Only send the PaintAck message if this paint is in response to an
    // invalidate from the plugin, since this message acts as an access token
    // to ensure only one process is using the transport dib at a time.
    invalidate_pending_ = false;
    Send(new PluginMsg_DidPaint(instance_id_));
  }

#if WEBKIT_USING_SKIA
  canvas->endPlatformPaint();
#endif
}

bool WebPluginDelegateProxy::BackgroundChanged(
    gfx::NativeDrawingContext context,
    const gfx::Rect& rect) {
#if defined(OS_WIN)
  HBITMAP hbitmap = static_cast<HBITMAP>(GetCurrentObject(context, OBJ_BITMAP));
  if (hbitmap == NULL) {
    NOTREACHED();
    return true;
  }

  BITMAP bitmap = { 0 };
  int result = GetObject(hbitmap, sizeof(bitmap), &bitmap);
  if (!result) {
    NOTREACHED();
    return true;
  }

  XFORM xf;
  if (!GetWorldTransform(context, &xf)) {
    NOTREACHED();
    return true;
  }

  // The damaged rect that we're given can be larger than the bitmap, so
  // intersect their rects first.
  gfx::Rect bitmap_rect(static_cast<int>(-xf.eDx), static_cast<int>(-xf.eDy),
                        bitmap.bmWidth, bitmap.bmHeight);
  gfx::Rect check_rect = rect.Intersect(bitmap_rect);
  int row_byte_size = check_rect.width() * (bitmap.bmBitsPixel / 8);
  for (int y = check_rect.y(); y < check_rect.bottom(); y++) {
    char* hdc_row_start = static_cast<char*>(bitmap.bmBits) +
        (y + static_cast<int>(xf.eDy)) * bitmap.bmWidthBytes +
        (check_rect.x() + static_cast<int>(xf.eDx)) * (bitmap.bmBitsPixel / 8);

    // getAddr32 doesn't use the translation units, so we have to subtract
    // the plugin origin from the coordinates.
    uint32_t* canvas_row_start =
        background_store_canvas_->getDevice()->accessBitmap(true).getAddr32(
            check_rect.x() - plugin_rect_.x(), y - plugin_rect_.y());
    if (memcmp(hdc_row_start, canvas_row_start, row_byte_size) != 0)
      return true;
  }
#else
#if defined(OS_MACOSX)
  // If there is a translation on the content area context, we need to account
  // for it; the context may be a subset of the full content area with a
  // transform that makes the coordinates work out.
  CGAffineTransform transform = CGContextGetCTM(context);
  bool flipped = fabs(transform.d + 1) < 0.0001;
  CGFloat context_offset_x = -transform.tx;
  CGFloat context_offset_y = flipped ? transform.ty -
                                           CGBitmapContextGetHeight(context)
                                     : -transform.ty;
  gfx::Rect full_content_rect(context_offset_x, context_offset_y,
                              CGBitmapContextGetWidth(context),
                              CGBitmapContextGetHeight(context));
#else
  cairo_surface_t* page_surface = cairo_get_target(context);
  DCHECK_EQ(cairo_surface_get_type(page_surface), CAIRO_SURFACE_TYPE_IMAGE);
  DCHECK_EQ(cairo_image_surface_get_format(page_surface), CAIRO_FORMAT_ARGB32);

  // Transform context coordinates into surface coordinates.
  double page_x_double = rect.x();
  double page_y_double = rect.y();
  cairo_user_to_device(context, &page_x_double, &page_y_double);
  gfx::Rect full_content_rect(0, 0,
                              cairo_image_surface_get_width(page_surface),
                              cairo_image_surface_get_height(page_surface));
#endif
  // According to comments in the Windows code, the damage rect that we're given
  // may project outside the image, so intersect their rects.
  gfx::Rect content_rect = rect.Intersect(full_content_rect);

#if defined(OS_MACOSX)
  const unsigned char* page_bytes = static_cast<const unsigned char*>(
      CGBitmapContextGetData(context));
  int page_stride = CGBitmapContextGetBytesPerRow(context);
  int page_start_x = content_rect.x() - context_offset_x;
  int page_start_y = content_rect.y() - context_offset_y;

  CGContextRef bg_context =
      background_store_canvas_->getTopPlatformDevice().GetBitmapContext();
  DCHECK_EQ(CGBitmapContextGetBitsPerPixel(context),
            CGBitmapContextGetBitsPerPixel(bg_context));
  const unsigned char* bg_bytes = static_cast<const unsigned char*>(
      CGBitmapContextGetData(bg_context));
  int full_bg_width = CGBitmapContextGetWidth(bg_context);
  int full_bg_height = CGBitmapContextGetHeight(bg_context);
  int bg_stride = CGBitmapContextGetBytesPerRow(bg_context);
  int bg_last_row = CGBitmapContextGetHeight(bg_context) - 1;

  int bytes_per_pixel = CGBitmapContextGetBitsPerPixel(context) / 8;
#else
  cairo_surface_flush(page_surface);
  const unsigned char* page_bytes = cairo_image_surface_get_data(page_surface);
  int page_stride = cairo_image_surface_get_stride(page_surface);
  int page_start_x = static_cast<int>(page_x_double);
  int page_start_y = static_cast<int>(page_y_double);

  skia::PlatformDevice& device =
      background_store_canvas_->getTopPlatformDevice();
  cairo_surface_t* bg_surface = cairo_get_target(device.beginPlatformPaint());
  DCHECK_EQ(cairo_surface_get_type(bg_surface), CAIRO_SURFACE_TYPE_IMAGE);
  DCHECK_EQ(cairo_image_surface_get_format(bg_surface), CAIRO_FORMAT_ARGB32);
  cairo_surface_flush(bg_surface);
  const unsigned char* bg_bytes = cairo_image_surface_get_data(bg_surface);
  int full_bg_width = cairo_image_surface_get_width(bg_surface);
  int full_bg_height = cairo_image_surface_get_height(bg_surface);
  int bg_stride = cairo_image_surface_get_stride(bg_surface);

  int bytes_per_pixel = 4;  // ARGB32 = 4 bytes per pixel.
#endif

  int damage_width = content_rect.width();
  int damage_height = content_rect.height();

  int bg_start_x = rect.x() - plugin_rect_.x();
  int bg_start_y = rect.y() - plugin_rect_.y();
  // The damage rect is supposed to have been intersected with the plugin rect;
  // double-check, since if it hasn't we'll walk off the end of the buffer.
  DCHECK_LE(bg_start_x + damage_width, full_bg_width);
  DCHECK_LE(bg_start_y + damage_height, full_bg_height);

  int bg_x_byte_offset = bg_start_x * bytes_per_pixel;
  int page_x_byte_offset = page_start_x * bytes_per_pixel;
  for (int row = 0; row < damage_height; ++row) {
    int page_offset = page_stride * (page_start_y + row) + page_x_byte_offset;
    int bg_y = bg_start_y + row;
#if defined(OS_MACOSX)
    // The background buffer is upside down relative to the content.
    bg_y = bg_last_row - bg_y;
#endif
    int bg_offset = bg_stride * bg_y + bg_x_byte_offset;
    if (memcmp(page_bytes + page_offset,
               bg_bytes + bg_offset,
               damage_width * bytes_per_pixel) != 0)
      return true;
  }
#endif

  return false;
}

void WebPluginDelegateProxy::Print(gfx::NativeDrawingContext context) {
  base::SharedMemoryHandle shared_memory;
  uint32 size;
  if (!Send(new PluginMsg_Print(instance_id_, &shared_memory, &size)))
    return;

  base::SharedMemory memory(shared_memory, true);
  if (!memory.Map(size)) {
    NOTREACHED();
    return;
  }

#if defined(OS_WIN)
  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  if (!metafile->Init(memory.memory(), size)) {
    NOTREACHED();
    return;
  }
  // Playback the buffer.
  metafile->Playback(context, NULL);
#else
  // TODO(port): plugin printing.
  NOTIMPLEMENTED();
#endif
}

NPObject* WebPluginDelegateProxy::GetPluginScriptableObject() {
  if (npobject_)
    return WebBindings::retainObject(npobject_);

  int route_id = MSG_ROUTING_NONE;
  Send(new PluginMsg_GetPluginScriptableObject(instance_id_, &route_id));
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  npobject_ = NPObjectProxy::Create(
      channel_host_.get(), route_id, 0, page_url_);

  return WebBindings::retainObject(npobject_);
}

void WebPluginDelegateProxy::DidFinishLoadWithReason(
    const GURL& url, NPReason reason, int notify_id) {
  Send(new PluginMsg_DidFinishLoadWithReason(
      instance_id_, url, reason, notify_id));
}

void WebPluginDelegateProxy::SetFocus(bool focused) {
  Send(new PluginMsg_SetFocus(instance_id_, focused));
}

bool WebPluginDelegateProxy::HandleInputEvent(
    const WebInputEvent& event,
    WebCursorInfo* cursor_info) {
  bool handled;
  WebCursor cursor;
  // A windowless plugin can enter a modal loop in the context of a
  // NPP_HandleEvent call, in which case we need to pump messages to
  // the plugin. We pass of the corresponding event handle to the
  // plugin process, which is set if the plugin does enter a modal loop.
  IPC::SyncMessage* message = new PluginMsg_HandleInputEvent(
      instance_id_, &event, &handled, &cursor);
  message->set_pump_messages_event(modal_loop_pump_messages_event_.get());
  Send(message);
  cursor.GetCursorInfo(cursor_info);
  return handled;
}

int WebPluginDelegateProxy::GetProcessId() {
  return channel_host_->peer_pid();
}

void WebPluginDelegateProxy::SetContentAreaFocus(bool has_focus) {
  IPC::Message* msg = new PluginMsg_SetContentAreaFocus(instance_id_,
                                                        has_focus);
  // Make sure focus events are delivered in the right order relative to
  // sync messages they might interact with (Paint, HandleEvent, etc.).
  msg->set_unblock(true);
  Send(msg);
}

#if defined(OS_MACOSX)
void WebPluginDelegateProxy::SetWindowFocus(bool window_has_focus) {
  IPC::Message* msg = new PluginMsg_SetWindowFocus(instance_id_,
                                                   window_has_focus);
  // Make sure focus events are delivered in the right order relative to
  // sync messages they might interact with (Paint, HandleEvent, etc.).
  msg->set_unblock(true);
  Send(msg);
}

void WebPluginDelegateProxy::SetContainerVisibility(bool is_visible) {
  IPC::Message* msg;
  if (is_visible) {
    gfx::Rect window_frame = render_view_->rootWindowRect();
    gfx::Rect view_frame = render_view_->windowRect();
    WebKit::WebView* webview = render_view_->webview();
    msg = new PluginMsg_ContainerShown(instance_id_, window_frame, view_frame,
                                       webview && webview->isActive());
  } else {
    msg = new PluginMsg_ContainerHidden(instance_id_);
  }
  // Make sure visibility events are delivered in the right order relative to
  // sync messages they might interact with (Paint, HandleEvent, etc.).
  msg->set_unblock(true);
  Send(msg);
}

void WebPluginDelegateProxy::WindowFrameChanged(gfx::Rect window_frame,
                                                gfx::Rect view_frame) {
  IPC::Message* msg = new PluginMsg_WindowFrameChanged(instance_id_,
                                                       window_frame,
                                                       view_frame);
  // Make sure frame events are delivered in the right order relative to
  // sync messages they might interact with (e.g., HandleEvent).
  msg->set_unblock(true);
  Send(msg);
}
void WebPluginDelegateProxy::ImeCompositionCompleted(const string16& text,
                                                     int plugin_id) {
  // If the message isn't intended for this plugin, there's nothing to do.
  if (instance_id_ != plugin_id)
    return;

  IPC::Message* msg = new PluginMsg_ImeCompositionCompleted(instance_id_,
                                                            text);
  // Order relative to other key events is important.
  msg->set_unblock(true);
  Send(msg);
}
#endif  // OS_MACOSX

void WebPluginDelegateProxy::OnSetWindow(gfx::PluginWindowHandle window) {
  uses_shared_bitmaps_ = !window;
  window_ = window;
  if (plugin_)
    plugin_->SetWindow(window);
}

void WebPluginDelegateProxy::WillDestroyWindow() {
  DCHECK(window_);
  plugin_->WillDestroyWindow(window_);
#if defined(OS_MACOSX)
  if (window_) {
    // This is actually a "fake" window handle only for the GPU
    // plugin. Deallocate it on the browser side.
    if (render_view_)
      render_view_->DestroyFakePluginWindowHandle(window_);
  }
#endif
  window_ = gfx::kNullPluginWindow;
}

#if defined(OS_WIN)
void WebPluginDelegateProxy::OnSetWindowlessPumpEvent(
      HANDLE modal_loop_pump_messages_event) {
  DCHECK(modal_loop_pump_messages_event_ == NULL);

  // Bug 25583: this can be null because some "virus scanners" block the
  // DuplicateHandle call in the plugin process.
  if (!modal_loop_pump_messages_event)
    return;

  modal_loop_pump_messages_event_.reset(
      new base::WaitableEvent(modal_loop_pump_messages_event));
}
#endif

void WebPluginDelegateProxy::OnCancelResource(int id) {
  if (plugin_)
    plugin_->CancelResource(id);
}

void WebPluginDelegateProxy::OnInvalidateRect(const gfx::Rect& rect) {
  if (!plugin_)
    return;

  // Clip the invalidation rect to the plugin bounds; the plugin may have been
  // resized since the invalidate message was sent.
  const gfx::Rect clipped_rect(rect.Intersect(gfx::Rect(plugin_rect_.size())));

  invalidate_pending_ = true;
  CopyFromTransportToBacking(clipped_rect);
  plugin_->InvalidateRect(clipped_rect);
}

void WebPluginDelegateProxy::OnGetWindowScriptNPObject(
    int route_id, bool* success) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetWindowScriptNPObject();

  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  window_script_object_ = (new NPObjectStub(
      npobject, channel_host_.get(), route_id, 0, page_url_))->AsWeakPtr();
  *success = true;
}

void WebPluginDelegateProxy::OnGetPluginElement(int route_id, bool* success) {
  *success = false;
  NPObject* npobject = NULL;
  if (plugin_)
    npobject = plugin_->GetPluginElement();
  if (!npobject)
    return;

  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  new NPObjectStub(
      npobject, channel_host_.get(), route_id, 0, page_url_);
  *success = true;
}

void WebPluginDelegateProxy::OnSetCookie(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         const std::string& cookie) {
  if (plugin_)
    plugin_->SetCookie(url, first_party_for_cookies, cookie);
}

void WebPluginDelegateProxy::OnGetCookies(const GURL& url,
                                          const GURL& first_party_for_cookies,
                                          std::string* cookies) {
  DCHECK(cookies);
  if (plugin_)
    *cookies = plugin_->GetCookies(url, first_party_for_cookies);
}

void WebPluginDelegateProxy::OnMissingPluginStatus(int status) {
  if (render_view_)
    render_view_->OnMissingPluginStatus(this, status);
}

void WebPluginDelegateProxy::PaintSadPlugin(WebKit::WebCanvas* native_context,
                                            const gfx::Rect& rect) {
  // Lazily load the sad plugin image.
  if (!sad_plugin_) {
    sad_plugin_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SAD_PLUGIN);
  }
  if (!sad_plugin_)
    return;

  // Make a temporary canvas for the background image.
  const int width = plugin_rect_.width();
  const int height = plugin_rect_.height();
  gfx::CanvasSkia canvas(width, height, false);
#if defined(OS_MACOSX)
  // Flip the canvas, since the context expects flipped data.
  canvas.translate(0, height);
  canvas.scale(1, -1);
#endif
  SkPaint paint;

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);
  canvas.drawRectCoords(0, 0, SkIntToScalar(width), SkIntToScalar(height),
                        paint);
  canvas.DrawBitmapInt(*sad_plugin_,
                       std::max(0, (width - sad_plugin_->width())/2),
                       std::max(0, (height - sad_plugin_->height())/2));

  // It's slightly less code to make a big SkBitmap of the sad tab image and
  // then copy that to the screen than to use the native APIs. The small speed
  // penalty is not important when drawing crashed plugins.
#if WEBKIT_USING_SKIA
  gfx::NativeDrawingContext context = native_context->beginPlatformPaint();
  BlitCanvasToContext(context, plugin_rect_, &canvas, gfx::Point(0, 0));
  native_context->endPlatformPaint();
#elif WEBKIT_USING_CG
  BlitCanvasToContext(native_context, plugin_rect_, &canvas, gfx::Point(0, 0));
#endif
}

void WebPluginDelegateProxy::CopyFromTransportToBacking(const gfx::Rect& rect) {
  if (!backing_store_canvas_.get()) {
    return;
  }

  // Copy the damaged rect from the transport bitmap to the backing store.
#if defined(OS_MACOSX)
  // Blitting the bits directly is much faster than going through CG, and since
  // since the goal is just to move the raw pixels between two bitmaps with the
  // same pixel format (no compositing, color correction, etc.), it's safe.
  const size_t stride =
      skia::PlatformCanvas::StrideForWidth(plugin_rect_.width());
  const size_t chunk_size = 4 * rect.width();
  uint8* source_data = static_cast<uint8*>(transport_store_->memory()) +
                       rect.y() * stride + 4 * rect.x();
  // The two bitmaps are flipped relative to each other.
  int dest_starting_row = plugin_rect_.height() - rect.y() - 1;
  DCHECK(!backing_store_.empty());
  uint8* target_data = &(backing_store_[0]) + dest_starting_row * stride +
                       4 * rect.x();
  for (int row = 0; row < rect.height(); ++row) {
    memcpy(target_data, source_data, chunk_size);
    source_data += stride;
    target_data -= stride;
  }
#else
  BlitCanvasToCanvas(backing_store_canvas_.get(), rect,
                     transport_store_canvas_.get(), rect.origin());
#endif
  backing_store_painted_ = backing_store_painted_.Union(rect);
}

void WebPluginDelegateProxy::OnHandleURLRequest(
    const PluginHostMsg_URLRequest_Params& params) {
  const char* data = NULL;
  if (params.buffer.size())
    data = &params.buffer[0];

  const char* target = NULL;
  if (params.target.length())
    target = params.target.c_str();

  plugin_->HandleURLRequest(
      params.url.c_str(), params.method.c_str(), target, data,
      static_cast<unsigned int>(params.buffer.size()), params.notify_id,
      params.popups_allowed, params.notify_redirects);
}

webkit::npapi::WebPluginResourceClient*
WebPluginDelegateProxy::CreateResourceClient(
    unsigned long resource_id, const GURL& url, int notify_id) {
  if (!channel_host_)
    return NULL;

  ResourceClientProxy* proxy = new ResourceClientProxy(channel_host_,
                                                       instance_id_);
  proxy->Initialize(resource_id, url, notify_id);
  return proxy;
}

webkit::npapi::WebPluginResourceClient*
WebPluginDelegateProxy::CreateSeekableResourceClient(
    unsigned long resource_id, int range_request_id) {
  if (!channel_host_)
    return NULL;

  ResourceClientProxy* proxy = new ResourceClientProxy(channel_host_,
                                                       instance_id_);
  proxy->InitializeForSeekableStream(resource_id, range_request_id);
  return proxy;
}

#if defined(OS_MACOSX)
void WebPluginDelegateProxy::OnFocusChanged(bool focused) {
  if (render_view_)
    render_view_->PluginFocusChanged(focused, instance_id_);
}

void WebPluginDelegateProxy::OnStartIme() {
  if (render_view_)
    render_view_->StartPluginIme();
}

void WebPluginDelegateProxy::OnBindFakePluginWindowHandle(bool opaque) {
  BindFakePluginWindowHandle(opaque);
}

// Synthesize a fake window handle for the plug-in to identify the instance
// to the browser, allowing mapping to a surface for hardware acceleration
// of plug-in content. The browser generates the handle which is then set on
// the plug-in. Returns true if it successfully sets the window handle on the
// plug-in.
bool WebPluginDelegateProxy::BindFakePluginWindowHandle(bool opaque) {
  gfx::PluginWindowHandle fake_window = NULL;
  if (render_view_)
    fake_window = render_view_->AllocateFakePluginWindowHandle(opaque, false);
  // If we aren't running on 10.6, this allocation will fail.
  if (!fake_window)
    return false;
  OnSetWindow(fake_window);
  if (!Send(new PluginMsg_SetFakeAcceleratedSurfaceWindowHandle(instance_id_,
                                                                fake_window))) {
    return false;
  }

  // Since this isn't a real window, it doesn't get initial size and location
  // information the way a real windowed plugin would, so we need to feed it its
  // starting geometry.
  webkit::npapi::WebPluginGeometry geom;
  geom.window = fake_window;
  geom.window_rect = plugin_rect_;
  geom.clip_rect = clip_rect_;
  geom.rects_valid = true;
  geom.visible = true;
  render_view_->DidMovePlugin(geom);
  // Invalidate the plugin region to ensure that the move event actually gets
  // dispatched (for a plugin on an otherwise static page).
  render_view_->didInvalidateRect(WebKit::WebRect(plugin_rect_.x(),
                                                  plugin_rect_.y(),
                                                  plugin_rect_.width(),
                                                  plugin_rect_.height()));

  return true;
}
#endif

gfx::PluginWindowHandle WebPluginDelegateProxy::GetPluginWindowHandle() {
  return window_;
}

void WebPluginDelegateProxy::OnCancelDocumentLoad() {
  plugin_->CancelDocumentLoad();
}

void WebPluginDelegateProxy::OnInitiateHTTPRangeRequest(
    const std::string& url,
    const std::string& range_info,
    int range_request_id) {
  plugin_->InitiateHTTPRangeRequest(
      url.c_str(), range_info.c_str(), range_request_id);
}

void WebPluginDelegateProxy::OnDeferResourceLoading(unsigned long resource_id,
                                                    bool defer) {
  plugin_->SetDeferResourceLoading(resource_id, defer);
}

#if defined(OS_MACOSX)
void WebPluginDelegateProxy::OnUpdateGeometry_ACK(int ack_key) {
  DCHECK_NE(ack_key, -1);

  OldTransportDIBMap::iterator iterator = old_transport_dibs_.find(ack_key);

  // DCHECK_NE does not work with base::hash_map.
  DCHECK(iterator != old_transport_dibs_.end());

  // Now that the ACK has been received, the TransportDIB that was used
  // prior to the UpdateGeometry message now being acknowledged is known to
  // be no longer needed.  Release it, and take the stale entry out of the map.
  ReleaseTransportDIB(iterator->second.get());

  old_transport_dibs_.erase(iterator);
}

void WebPluginDelegateProxy::OnAcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  if (render_view_)
    render_view_->AcceleratedSurfaceSetIOSurface(window, width, height,
                                                 io_surface_identifier);
}

void WebPluginDelegateProxy::OnAcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  if (render_view_)
    render_view_->AcceleratedSurfaceSetTransportDIB(window, width, height,
                                                    transport_dib);
}

void WebPluginDelegateProxy::OnAcceleratedSurfaceAllocTransportDIB(
    size_t size,
    TransportDIB::Handle* dib_handle) {
  if (render_view_)
    *dib_handle = render_view_->AcceleratedSurfaceAllocTransportDIB(size);
  else
    *dib_handle = TransportDIB::DefaultHandleValue();
}

void WebPluginDelegateProxy::OnAcceleratedSurfaceFreeTransportDIB(
    TransportDIB::Id dib_id) {
  if (render_view_)
    render_view_->AcceleratedSurfaceFreeTransportDIB(dib_id);
}

void WebPluginDelegateProxy::OnAcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window, uint64 surface_id) {
  if (render_view_)
    render_view_->AcceleratedSurfaceBuffersSwapped(window, surface_id);
}
#endif

#if defined(OS_WIN)
bool WebPluginDelegateProxy::UseSynchronousGeometryUpdates() {
  // Need to update geometry synchronously with WMP, otherwise if a site
  // scripts the plugin to start playing while it's in the middle of handling
  // an update geometry message, videos don't play.  See urls in bug 20260.
  if (info_.name.find(ASCIIToUTF16("Windows Media Player")) != string16::npos)
    return true;

  // The move networks plugin needs to be informed of geometry updates
  // synchronously.
  std::vector<webkit::npapi::WebPluginMimeType>::iterator index;
  for (index = info_.mime_types.begin(); index != info_.mime_types.end();
       index++) {
    if (index->mime_type == "application/x-vnd.moveplayer.qm" ||
        index->mime_type == "application/x-vnd.moveplay2.qm" ||
        index->mime_type == "application/x-vnd.movenetworks.qm" ||
        index->mime_type == "application/x-vnd.mnplayer.qm") {
      return true;
    }
  }
  return false;
}
#endif

void WebPluginDelegateProxy::OnURLRedirectResponse(bool allow,
                                                   int resource_id) {
  if (!plugin_)
    return;

  plugin_->URLRedirectResponse(allow, resource_id);
}
