// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webplugin_delegate_proxy.h"

#include <algorithm>

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "app/gfx/blit.h"
#include "app/gfx/canvas.h"
#include "app/gfx/native_widget_types.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/gfx/size.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "net/base/mime_util.h"
#include "printing/native_metafile.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#endif

using WebKit::WebBindings;
using WebKit::WebCursorInfo;
using WebKit::WebDragData;
using WebKit::WebInputEvent;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;

// Proxy for WebPluginResourceClient.  The object owns itself after creation,
// deleting itself after its callback has been called.
class ResourceClientProxy : public webkit_glue::WebPluginResourceClient {
 public:
  ResourceClientProxy(PluginChannelHost* channel, int instance_id)
    : channel_(channel), instance_id_(instance_id), resource_id_(0),
      notify_needed_(false), notify_data_(0),
      multibyte_response_expected_(false) {
  }

  ~ResourceClientProxy() {
  }

  void Initialize(unsigned long resource_id, const GURL& url,
                  bool notify_needed, intptr_t notify_data,
                  intptr_t existing_stream) {
    resource_id_ = resource_id;
    url_ = url;
    notify_needed_ = notify_needed;
    notify_data_ = notify_data;

    PluginMsg_URLRequestReply_Params params;
    params.resource_id = resource_id;
    params.url = url_;
    params.notify_needed = notify_needed_;
    params.notify_data = notify_data_;
    params.stream = existing_stream;

    multibyte_response_expected_ = (existing_stream != 0);

    channel_->Send(new PluginMsg_HandleURLRequestReply(instance_id_, params));
  }

  // PluginResourceClient implementation:
  void WillSendRequest(const GURL& url) {
    DCHECK(channel_ != NULL);
    channel_->Send(new PluginMsg_WillSendRequest(instance_id_, resource_id_,
                                                 url));
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

 private:
  scoped_refptr<PluginChannelHost> channel_;
  int instance_id_;
  unsigned long resource_id_;
  GURL url_;
  bool notify_needed_;
  intptr_t notify_data_;
  // Set to true if the response expected is a multibyte response.
  // For e.g. response for a HTTP byte range request.
  bool multibyte_response_expected_;
};

WebPluginDelegateProxy::WebPluginDelegateProxy(
    const std::string& mime_type,
    const base::WeakPtr<RenderView>& render_view)
    : render_view_(render_view),
      plugin_(NULL),
      windowless_(false),
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
}

void WebPluginDelegateProxy::PluginDestroyed() {
  if (window_)
    WillDestroyWindow();

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

bool WebPluginDelegateProxy::Initialize(const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    webkit_glue::WebPlugin* plugin,
    bool load_manually) {
  IPC::ChannelHandle channel_handle;
  if (!RenderThread::current()->Send(new ViewHostMsg_OpenChannelToPlugin(
          url, mime_type_, webkit_glue::GetWebKitLocale(),
          &channel_handle, &info_))) {
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

#if defined(OS_POSIX)
  // If we received a ChannelHandle, register it now.
  if (channel_handle.socket.fd >= 0)
    IPC::AddChannelSocket(channel_handle.name, channel_handle.socket.fd);
#endif

  scoped_refptr<PluginChannelHost> channel_host =
      PluginChannelHost::GetPluginChannelHost(
          channel_handle.name, ChildProcess::current()->io_message_loop());
  if (!channel_host.get())
    return false;

  int instance_id;
  bool result = channel_host->Send(new PluginMsg_CreateInstance(
      mime_type_, &instance_id));
  if (!result)
    return false;

  channel_host_ = channel_host;
  instance_id_ = instance_id;

  channel_host_->AddRoute(instance_id_, this, false);

  // Now tell the PluginInstance in the plugin process to initialize.
  PluginMsg_Init_Params params;
  params.containing_window = render_view_->host_window();
  params.url = url;
  params.page_url = page_url_;
  params.arg_names = arg_names;
  params.arg_values = arg_values;
  for (size_t i = 0; i < arg_names.size(); ++i) {
    if (LowerCaseEqualsASCII(arg_names[i], "wmode") &&
        LowerCaseEqualsASCII(arg_values[i], "transparent")) {
      transparent_ = true;
    }
  }
#if defined(OS_MACOSX)
  // Until we have a way to support accelerated (3D) drawing on Macs, ask
  // Flash to use windowless mode so that it use CoreGraphics instead of opening
  // OpenGL contexts overlaying the browser window (which will fail or crash
  // because Mac OS X does not allow that across processes).
  if (!transparent_ && mime_type_ == "application/x-shockwave-flash") {
    params.arg_names.push_back("wmode");
    params.arg_values.push_back("opaque");
  }
#endif
  params.load_manually = load_manually;

  plugin_ = plugin;

  result = false;
  IPC::Message* msg = new PluginMsg_Init(instance_id_, params, &result);
  Send(msg);

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
                                                  bool notify_needed,
                                                  intptr_t notify_data) {
  PluginMsg_SendJavaScriptStream* msg =
      new PluginMsg_SendJavaScriptStream(instance_id_, url, result,
                                         success, notify_needed,
                                         notify_data);
  Send(msg);
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

void WebPluginDelegateProxy::OnMessageReceived(const IPC::Message& msg) {
  child_process_logging::SetActiveURL(page_url_);

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
    IPC_MESSAGE_HANDLER(PluginHostMsg_ShowModalHTMLDialog,
                        OnShowModalHTMLDialog)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetDragData, OnGetDragData);
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetDropEffect, OnSetDropEffect);
    IPC_MESSAGE_HANDLER(PluginHostMsg_MissingPluginStatus,
                        OnMissingPluginStatus)
    IPC_MESSAGE_HANDLER(PluginHostMsg_URLRequest, OnHandleURLRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_GetCPBrowsingContext,
                        OnGetCPBrowsingContext)
    IPC_MESSAGE_HANDLER(PluginHostMsg_CancelDocumentLoad, OnCancelDocumentLoad)
    IPC_MESSAGE_HANDLER(PluginHostMsg_InitiateHTTPRangeRequest,
                        OnInitiateHTTPRangeRequest)
    IPC_MESSAGE_HANDLER(PluginHostMsg_DeferResourceLoading,
                        OnDeferResourceLoading)

#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(PluginHostMsg_UpdateGeometry_ACK,
                        OnUpdateGeometry_ACK)
#endif

    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
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
  render_view_->PluginCrashed(info_.path);
}

void WebPluginDelegateProxy::UpdateGeometry(const gfx::Rect& window_rect,
                                            const gfx::Rect& clip_rect) {
  plugin_rect_ = window_rect;

  bool bitmaps_changed = false;

  PluginMsg_UpdateGeometry_Param param;
#if defined(OS_MACOSX)
  param.ack_key = -1;
#endif

  if (windowless_) {
    if (!backing_store_canvas_.get() ||
        (window_rect.width() != backing_store_canvas_->getDevice()->width() ||
         window_rect.height() != backing_store_canvas_->getDevice()->height()))
        {
      bitmaps_changed = true;

#if defined(OS_MACOSX)
      if (backing_store_.get()) {
        // ResetWindowlessBitmaps inserts the old TransportDIBs into
        // old_transport_dibs_ using the backing store's file descriptor as
        // the key.  The constraints on the keys are that -1 is reserved
        // to mean "no ACK required," and in-flight keys must be unique.
        // File descriptors will never be -1, and because they won't be closed
        // until receipt of the ACK, they're unique.
        param.ack_key = backing_store_->handle().fd;
      }
#endif

      // Create a shared memory section that the plugin paints into
      // asynchronously.
      ResetWindowlessBitmaps();
      if (!window_rect.IsEmpty()) {
        if (!CreateBitmap(&backing_store_, &backing_store_canvas_) ||
            !CreateBitmap(&transport_store_, &transport_store_canvas_) ||
            (transparent_ &&
             !CreateBitmap(&background_store_, &background_store_canvas_))) {
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
  if (info_.name.find(L"Windows Media Player") != std::wstring::npos) {
    // Need to update geometry synchronously with WMP, otherwise if a site
    // scripts the plugin to start playing while it's in the middle of handling
    // an update geometry message, videos don't play.  See urls in bug 20260.
    msg = new PluginMsg_UpdateGeometrySync(instance_id_, param);
  }
  else
#endif
  {
    msg = new PluginMsg_UpdateGeometry(instance_id_, param);
    msg->set_unblock(true);
  }

  Send(msg);
}

#if defined(OS_MACOSX)
static void ReleaseTransportDIB(TransportDIB *dib) {
  if (dib) {
    IPC::Message* msg = new ViewHostMsg_FreeTransportDIB(dib->id());
    RenderThread::current()->Send(msg);
  }
}
#endif

void WebPluginDelegateProxy::ResetWindowlessBitmaps() {
#if defined(OS_MACOSX)
  if (backing_store_.get()) {
    int ack_key = backing_store_->handle().fd;

    DCHECK_NE(ack_key, -1);

    // DCHECK_EQ does not work with base::hash_map.
    DCHECK(old_transport_dibs_.find(ack_key) == old_transport_dibs_.end());

    // Stash the old TransportDIBs in the map.  They'll be released when an
    // ACK message comes in.
    RelatedTransportDIBs old_dibs;
    old_dibs.backing_store =
        linked_ptr<TransportDIB>(backing_store_.release());
    old_dibs.transport_store =
        linked_ptr<TransportDIB>(transport_store_.release());
    old_dibs.background_store =
        linked_ptr<TransportDIB>(background_store_.release());

    old_transport_dibs_[ack_key] = old_dibs;
  } else {
    DCHECK(!transport_store_.get());
    DCHECK(!background_store_.get());
  }
#else
  backing_store_.reset();
  transport_store_.reset();
  background_store_.reset();
#endif

  backing_store_canvas_.reset();
  transport_store_canvas_.reset();
  background_store_canvas_.release();
  backing_store_painted_ = gfx::Rect();
}

bool WebPluginDelegateProxy::CreateBitmap(
    scoped_ptr<TransportDIB>* memory,
    scoped_ptr<skia::PlatformCanvas>* canvas) {
  int width = plugin_rect_.width();
  int height = plugin_rect_.height();
  const size_t stride = skia::PlatformCanvas::StrideForWidth(width);
  const size_t size = stride * height;
#if defined(OS_LINUX)
  memory->reset(TransportDIB::Create(size, 0));
  if (!memory->get())
    return false;
#endif
#if defined(OS_MACOSX)
  TransportDIB::Handle handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(size, &handle);
  if (!RenderThread::current()->Send(msg))
    return false;
  if (handle.fd < 0)
    return false;
  memory->reset(TransportDIB::Map(handle));
#else
  static uint32 sequence_number = 0;
  memory->reset(TransportDIB::Create(size, sequence_number++));
#endif
  canvas->reset((*memory)->GetPlatformCanvas(width, height));
  return true;
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

  // No paint events for windowed plugins.
  if (!windowless_)
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
  const unsigned char* page_bytes = cairo_image_surface_get_data(page_surface);
  int page_stride = cairo_image_surface_get_stride(page_surface);
  int page_start_x = static_cast<int>(page_x_double);
  int page_start_y = static_cast<int>(page_y_double);

  skia::PlatformDevice& device =
      background_store_canvas_->getTopPlatformDevice();
  cairo_surface_t* bg_surface = cairo_get_target(device.beginPlatformPaint());
  DCHECK_EQ(cairo_surface_get_type(bg_surface), CAIRO_SURFACE_TYPE_IMAGE);
  DCHECK_EQ(cairo_image_surface_get_format(bg_surface), CAIRO_FORMAT_ARGB32);
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
  size_t size;
  if (!Send(new PluginMsg_Print(instance_id_, &shared_memory, &size)))
    return;

  base::SharedMemory memory(shared_memory, true);
  if (!memory.Map(size)) {
    NOTREACHED();
    return;
  }

#if defined(OS_WIN)
  printing::NativeMetafile metafile;
  if (!metafile.CreateFromData(memory.memory(), size)) {
    NOTREACHED();
    return;
  }
  // Playback the buffer.
  metafile.Playback(context, NULL);
#else
  // TODO(port): plugin printing.
  NOTIMPLEMENTED();
#endif
}

NPObject* WebPluginDelegateProxy::GetPluginScriptableObject() {
  if (npobject_)
    return WebBindings::retainObject(npobject_);

  int route_id = MSG_ROUTING_NONE;
  intptr_t npobject_ptr;
  Send(new PluginMsg_GetPluginScriptableObject(
      instance_id_, &route_id, &npobject_ptr));
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  npobject_ = NPObjectProxy::Create(
      channel_host_.get(), route_id, npobject_ptr, 0, page_url_);

  return WebBindings::retainObject(npobject_);
}

void WebPluginDelegateProxy::DidFinishLoadWithReason(
    const GURL& url, NPReason reason, intptr_t notify_data) {
  Send(new PluginMsg_DidFinishLoadWithReason(
      instance_id_, url, reason, notify_data));
}

void WebPluginDelegateProxy::SetFocus() {
  Send(new PluginMsg_SetFocus(instance_id_));
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

void WebPluginDelegateProxy::OnSetWindow(gfx::PluginWindowHandle window) {
  windowless_ = !window;
  window_ = window;
  if (plugin_)
    plugin_->SetWindow(window);
}

void WebPluginDelegateProxy::WillDestroyWindow() {
  DCHECK(window_);
  plugin_->WillDestroyWindow(window_);
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

  invalidate_pending_ = true;
  CopyFromTransportToBacking(rect);
  plugin_->InvalidateRect(rect);
}

void WebPluginDelegateProxy::OnGetWindowScriptNPObject(
    int route_id, bool* success, intptr_t* npobject_ptr) {
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
  *npobject_ptr = reinterpret_cast<intptr_t>(npobject);
}

void WebPluginDelegateProxy::OnGetPluginElement(
    int route_id, bool* success, intptr_t* npobject_ptr) {
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
  *npobject_ptr = reinterpret_cast<intptr_t>(npobject);
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

void WebPluginDelegateProxy::OnShowModalHTMLDialog(
    const GURL& url, int width, int height, const std::string& json_arguments,
    std::string* json_retval) {
  DCHECK(json_retval);
  if (render_view_) {
    render_view_->ShowModalHTMLDialogForPlugin(
        url, gfx::Size(width, height), json_arguments, json_retval);
  }
}

static void EncodeDragData(const WebDragData& data, bool add_data,
                           NPVariant* drag_type, NPVariant* drag_data) {
  const NPString* np_drag_type;
  if (data.hasFileNames()) {
    static const NPString kFiles = { "Files", 5 };
    np_drag_type = &kFiles;
  } else {
    static const NPString kEmpty = { "" , 0 };
    np_drag_type = &kEmpty;
    add_data = false;
  }

  STRINGN_TO_NPVARIANT(np_drag_type->UTF8Characters,
                       np_drag_type->UTF8Length,
                       *drag_type);
  if (!add_data) {
    VOID_TO_NPVARIANT(*drag_data);
    return;
  }

  WebVector<WebString> files;
  data.fileNames(files);

  static std::string utf8;
  utf8.clear();
  for (size_t i = 0; i < files.size(); ++i) {
    static const char kBackspaceDelimiter('\b');
    if (i != 0)
      utf8.append(1, kBackspaceDelimiter);
    utf8.append(files[i].utf8());
  }

  STRINGN_TO_NPVARIANT(utf8.data(), utf8.length(), *drag_data);
}

void WebPluginDelegateProxy::OnGetDragData(const NPVariant_Param& object,
                                           bool add_data,
                                           std::vector<NPVariant_Param>* values,
                                           bool* success) {
  DCHECK(values && success);
  *success = false;

  WebView* webview = NULL;
  if (render_view_)
    webview = render_view_->webview();
  if (!webview)
    return;

  int event_id;
  WebDragData data;
  NPObject* event = reinterpret_cast<NPObject*>(object.npobject_pointer);
  const int32 drag_id = webview->dragIdentity();
  if (!drag_id || !WebBindings::getDragData(event, &event_id, &data))
    return;

  NPVariant results[4];
  INT32_TO_NPVARIANT(drag_id, results[0]);
  INT32_TO_NPVARIANT(event_id, results[1]);
  EncodeDragData(data, add_data, &results[2], &results[3]);

  for (size_t i = 0; i < arraysize(results); ++i) {
    values->push_back(NPVariant_Param());
    CreateNPVariantParam(
        results[i], NULL, &values->back(), false, 0, page_url_);
  }

  *success = true;
}

void WebPluginDelegateProxy::OnSetDropEffect(const NPVariant_Param& object,
                                             int effect,
                                             bool* success) {
  DCHECK(success);
  *success = false;

  WebView* webview = NULL;
  if (render_view_)
    webview = render_view_->webview();
  if (!webview)
    return;

  NPObject* event = reinterpret_cast<NPObject*>(object.npobject_pointer);
  const int32 drag_id = webview->dragIdentity();
  if (!drag_id || !WebBindings::isDragEvent(event))
    return;

  *success = webview->setDropEffect(effect != 0);
}

void WebPluginDelegateProxy::OnMissingPluginStatus(int status) {
  if (render_view_)
    render_view_->OnMissingPluginStatus(this, status);
}

void WebPluginDelegateProxy::OnGetCPBrowsingContext(uint32* context) {
  *context = render_view_ ? render_view_->GetCPBrowsingContext() : 0;
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
  gfx::Canvas canvas(width, height, false);
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
  gfx::Rect dest_rect = rect;
#if defined(OS_MACOSX)
  FlipRectVerticallyWithHeight(&dest_rect, plugin_rect_.height());
#endif
  BlitCanvasToCanvas(backing_store_canvas_.get(), dest_rect,
                     transport_store_canvas_.get(), rect.origin());
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

  plugin_->HandleURLRequest(params.method.c_str(),
                            params.is_javascript_url, target,
                            static_cast<unsigned int>(params.buffer.size()),
                            data, params.is_file_data, params.notify,
                            params.url.c_str(), params.notify_data,
                            params.popups_allowed);
}

webkit_glue::WebPluginResourceClient*
WebPluginDelegateProxy::CreateResourceClient(
    unsigned long resource_id, const GURL& url, bool notify_needed,
    intptr_t notify_data, intptr_t npstream) {
  if (!channel_host_)
    return NULL;

  ResourceClientProxy* proxy = new ResourceClientProxy(channel_host_,
                                                       instance_id_);
  proxy->Initialize(resource_id, url, notify_needed, notify_data, npstream);
  return proxy;
}

CommandBufferProxy* WebPluginDelegateProxy::CreateCommandBuffer() {
#if defined(ENABLE_GPU)
  int command_buffer_id;
  if (!Send(new PluginMsg_CreateCommandBuffer(instance_id_,
                                              &command_buffer_id))) {
    return NULL;
  }

  return new CommandBufferProxy(channel_host_, command_buffer_id);
#else
  return NULL;
#endif
}

void WebPluginDelegateProxy::OnCancelDocumentLoad() {
  plugin_->CancelDocumentLoad();
}

void WebPluginDelegateProxy::OnInitiateHTTPRangeRequest(
    const std::string& url, const std::string& range_info,
    intptr_t existing_stream, bool notify_needed, intptr_t notify_data) {
  plugin_->InitiateHTTPRangeRequest(url.c_str(), range_info.c_str(),
                                    existing_stream, notify_needed,
                                    notify_data);
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

  // Now that the ACK has been received, the TransportDIBs that were used
  // prior to the UpdateGeometry message now being acknowledged are known to
  // be no longer needed.  Release them, and take the stale entry out of the
  // map.
  ReleaseTransportDIB(iterator->second.backing_store.get());
  ReleaseTransportDIB(iterator->second.transport_store.get());
  ReleaseTransportDIB(iterator->second.background_store.get());

  old_transport_dibs_.erase(iterator);
}
#endif
