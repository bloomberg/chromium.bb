// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define PEPPER_APIS_ENABLED 1

#include "chrome/renderer/webplugin_delegate_pepper.h"

#include <string>
#include <vector>

#include "app/gfx/blit.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

#if defined(ENABLE_GPU)
#include "webkit/glue/plugins/plugin_constants_win.h"
#endif

using gpu::Buffer;
using webkit_glue::WebPlugin;
using webkit_glue::WebPluginDelegate;
using webkit_glue::WebPluginResourceClient;
using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

namespace {

const uint32 kBytesPerPixel = 4;  // Only 8888 RGBA for now.

}  // namespace

// Implementation artifacts for a context
struct Device2DImpl {
  TransportDIB* dib;
};

uint32 WebPluginDelegatePepper::next_buffer_id = 0;

WebPluginDelegatePepper* WebPluginDelegatePepper::Create(
    const FilePath& filename,
    const std::string& mime_type,
    const base::WeakPtr<RenderView>& render_view,
    gfx::PluginWindowHandle containing_view) {
  scoped_refptr<NPAPI::PluginLib> plugin_lib =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin_lib.get() == NULL)
    return NULL;

  NPError err = plugin_lib->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin_lib->CreateInstance(mime_type);
  return new WebPluginDelegatePepper(render_view,
                                     containing_view,
                                     instance.get());
}

bool WebPluginDelegatePepper::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    WebPlugin* plugin,
    bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin_);
  int argc = 0;
  scoped_array<char*> argn(new char*[arg_names.size()]);
  scoped_array<char*> argv(new char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = const_cast<char*>(arg_names[i].c_str());
    argv[argc] = const_cast<char*>(arg_values[i].c_str());
    argc++;
  }

  bool start_result = instance_->Start(
      url, argn.get(), argv.get(), argc, load_manually);
  if (!start_result)
    return false;

  // For windowless plugins we should set the containing window handle
  // as the instance window handle. This is what Safari does. Not having
  // a valid window handle causes subtle bugs with plugins which retreive
  // the window handle and validate the same. The window handle can be
  // retreived via NPN_GetValue of NPNVnetscapeWindow.
  instance_->set_window_handle(parent_);

  plugin_url_ = url.spec();

  return true;
}

void WebPluginDelegatePepper::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();

    window_.window = NULL;
    instance_->NPP_SetWindow(&window_);

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    instance_ = 0;
  }

  // Destroy the nested GPU plugin only after first destroying the underlying
  // Pepper plugin. This is so the Pepper plugin does not attempt to issue
  // rendering commands after the GPU plugin has stopped processing them and
  // responding to them.
  if (nested_delegate_) {
    nested_delegate_->PluginDestroyed();
    nested_delegate_ = NULL;
  }
}

void WebPluginDelegatePepper::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ == window_rect)
    return;
  window_rect_ = window_rect;

  // TODO(brettw) figure out how to tell the plugin that the size changed and it
  // needs to repaint?
  SkBitmap new_committed;
  new_committed.setConfig(SkBitmap::kARGB_8888_Config,
                          window_rect_.width(), window_rect.height());
  new_committed.allocPixels();
  committed_bitmap_ = new_committed;

  // Forward the new geometry to the nested plugin instance.
  if (nested_delegate_)
    nested_delegate_->UpdateGeometry(window_rect, clip_rect);

  if (!instance())
    return;

  // TODO(sehr): do we need all this?
  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;
  instance()->NPP_SetWindow(&window_);
}

NPObject* WebPluginDelegatePepper::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegatePepper::DidFinishLoadWithReason(
    const GURL& url,
    NPReason reason,
    intptr_t notify_data) {
  instance()->DidFinishLoadWithReason(
      url, reason, reinterpret_cast<void*>(notify_data));
}

int WebPluginDelegatePepper::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return base::GetCurrentProcId();
}

void WebPluginDelegatePepper::SendJavaScriptStream(
    const GURL& url,
    const std::string& result,
    bool success,
    bool notify_needed,
    intptr_t notify_data) {
  instance()->SendJavaScriptStream(url, result, success, notify_needed,
                                   notify_data);
}

void WebPluginDelegatePepper::DidReceiveManualResponse(
    const GURL& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegatePepper::DidReceiveManualData(const char* buffer,
                                                       int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegatePepper::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegatePepper::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

FilePath WebPluginDelegatePepper::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

WebPluginResourceClient* WebPluginDelegatePepper::CreateResourceClient(
    unsigned long resource_id, const GURL& url, bool notify_needed,
    intptr_t notify_data, intptr_t existing_stream) {
  // Stream already exists. This typically happens for range requests
  // initiated via NPN_RequestRead.
  if (existing_stream) {
    NPAPI::PluginStream* plugin_stream =
        reinterpret_cast<NPAPI::PluginStream*>(existing_stream);

    return plugin_stream->AsResourceClient();
  }

  std::string mime_type;
  NPAPI::PluginStreamUrl *stream = instance()->CreateStream(
      resource_id, url, mime_type, notify_needed,
      reinterpret_cast<void*>(notify_data));
  return stream;
}

NPError WebPluginDelegatePepper::Device2DQueryCapability(int32 capability,
                                                         int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DQueryConfig(
    const NPDeviceContext2DConfig* request,
    NPDeviceContext2DConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DInitializeContext(
    const NPDeviceContext2DConfig* config,
    NPDeviceContext2D* context) {
  // This is a windowless plugin, so set it to have a NULL handle. Defer this
  // until we know the plugin will use the 2D device. If it uses the 3D device
  // it will have a window handle.
  plugin_->SetWindow(NULL);

  int width = window_rect_.width();
  int height = window_rect_.height();
  uint32 buffer_size = width * height * kBytesPerPixel;

  // Initialize the impelementation information in case of failure.
  context->reserved = NULL;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
  scoped_ptr<OpenPaintContext> paint_context(new OpenPaintContext);
  paint_context->transport_dib.reset(
      TransportDIB::Create(buffer_size, ++next_buffer_id));
  if (!paint_context->transport_dib.get())
    return NPERR_OUT_OF_MEMORY_ERROR;
  paint_context->canvas.reset(
      paint_context->transport_dib->GetPlatformCanvas(width, height));
  if (!paint_context->canvas.get())
    return NPERR_OUT_OF_MEMORY_ERROR;

  // Note that we need to get the address out of the bitmap rather than
  // using plugin_buffer_->memory(). The memory() is when the bitmap data
  // has had "Map" called on it. For Windows, this is separate than making a
  // bitmap using the shared section.
  const SkBitmap& plugin_bitmap =
      paint_context->canvas->getTopPlatformDevice().accessBitmap(true);
  SkAutoLockPixels locker(plugin_bitmap);

  // TODO(brettw) this theoretically shouldn't be necessary. But the
  // platform device on Windows will fill itself with green to help you
  // catch areas you didn't paint.
  plugin_bitmap.eraseARGB(0, 0, 0, 0);

  // Save the implementation information (the TransportDIB).
  Device2DImpl* impl = new Device2DImpl;
  if (impl == NULL) {
    // TODO(sehr,brettw): cleanup the context if we fail.
    return NPERR_GENERIC_ERROR;
  }
  impl->dib = paint_context->transport_dib.get();
  context->reserved = reinterpret_cast<void*>(impl);

  // Save the canvas to the output context structure and save the
  // OpenPaintContext for future reference.
  context->region = plugin_bitmap.getAddr32(0, 0);
  context->stride = width * kBytesPerPixel;
  context->dirty.left = 0;
  context->dirty.top = 0;
  context->dirty.right = width;
  context->dirty.bottom = height;
  open_paint_contexts_[context->region] =
      linked_ptr<OpenPaintContext>(paint_context.release());

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device2DSetStateContext(
    NPDeviceContext2D* context,
    int32 state,
    int32 value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DGetStateContext(
    NPDeviceContext2D* context,
    int32 state,
    int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DFlushContext(
    NPP id,
    NPDeviceContext2D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  // Get the bitmap data associated with the incoming context.
  OpenPaintContextMap::iterator found = open_paint_contexts_.find(
      context->region);
  if (found == open_paint_contexts_.end())
    return NPERR_INVALID_PARAM;  // TODO(brettw) call callback.

  OpenPaintContext* paint_context = found->second.get();

  // Draw the bitmap to the backing store.
  //
  // TODO(brettw) we can optimize this in the case where the entire canvas is
  // updated by actually taking ownership of the buffer and not telling the
  // plugin we're done using it. This wat we can avoid the copy when the entire
  // canvas has been updated.
  SkIRect src_rect = { context->dirty.left,
                       context->dirty.top,
                       context->dirty.right,
                       context->dirty.bottom };
  SkRect dest_rect = { SkIntToScalar(context->dirty.left),
                       SkIntToScalar(context->dirty.top),
                       SkIntToScalar(context->dirty.right),
                       SkIntToScalar(context->dirty.bottom) };
  SkCanvas committed_canvas(committed_bitmap_);

  // We want to replace the contents of the bitmap rather than blend.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  committed_canvas.drawBitmapRect(
      paint_context->canvas->getTopPlatformDevice().accessBitmap(false),
      &src_rect, dest_rect);

  committed_bitmap_.setIsOpaque(false);

  // Invoke the callback to inform the caller the work was done.
  // TODO(brettw) this is not how we want this to work, this should
  // happen when the frame is painted so the plugin knows when it can draw
  // the next frame.
  //
  // This should also be called in the failure cases as well.
  if (callback != NULL)
    (*callback)(id, context, NPERR_NO_ERROR, user_data);

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device2DDestroyContext(
    NPDeviceContext2D* context) {
  OpenPaintContextMap::iterator found = open_paint_contexts_.find(
      context->region);
  if (found == open_paint_contexts_.end())
    return NPERR_INVALID_PARAM;

  open_paint_contexts_.erase(found);
  // Free the implementation information.
  delete reinterpret_cast<Device2DImpl*>(context->reserved);
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DQueryCapability(int32 capability,
                                                         int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DQueryConfig(
    const NPDeviceContext3DConfig* request,
    NPDeviceContext3DConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DInitializeContext(
    const NPDeviceContext3DConfig* config,
    NPDeviceContext3D* context) {
#if defined(ENABLE_GPU)
  // Check to see if the GPU plugin is already initialized and fail if so.
  if (nested_delegate_)
    return NPERR_GENERIC_ERROR;

  // Create an instance of the GPU plugin that is responsible for 3D
  // rendering.
  nested_delegate_ = new WebPluginDelegateProxy(kGPUPluginMimeType,
                                                render_view_);

  // TODO(apatrick): should the GPU plugin be attached to plugin_?
  if (nested_delegate_->Initialize(GURL(),
                                   std::vector<std::string>(),
                                   std::vector<std::string>(),
                                   plugin_,
                                   false)) {
    // Ask the GPU plugin to create a command buffer and return a proxy.
    command_buffer_.reset(nested_delegate_->CreateCommandBuffer());
    if (command_buffer_.get()) {
      // Initialize the proxy command buffer.
      if (command_buffer_->Initialize(config->commandBufferEntries)) {
        // Initialize the 3D context.
        context->reserved = NULL;
        Buffer ring_buffer = command_buffer_->GetRingBuffer();
        context->commandBuffer = ring_buffer.ptr;
        context->commandBufferEntries = command_buffer_->GetSize();
        context->getOffset = command_buffer_->GetGetOffset();
        context->putOffset = command_buffer_->GetPutOffset();

        // Ensure the service knows the window size before rendering anything.
        nested_delegate_->UpdateGeometry(window_rect_, clip_rect_);

        return NPERR_NO_ERROR;
      }
    }

    command_buffer_.reset();
  }

  nested_delegate_->PluginDestroyed();
  nested_delegate_ = NULL;
#endif  // ENABLE_GPU

  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DSetStateContext(
    NPDeviceContext3D* context,
    int32 state,
    int32 value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DGetStateContext(
    NPDeviceContext3D* context,
    int32 state,
    int32* value) {
#if defined(ENABLE_GPU)
  if (!command_buffer_.get())
    return NPERR_GENERIC_ERROR;

  switch (state) {
    case NPDeviceContext3DState_GetOffset:
      context->getOffset = *value = command_buffer_->GetGetOffset();
      break;
    case NPDeviceContext3DState_PutOffset:
      *value = command_buffer_->GetPutOffset();
      break;
    case NPDeviceContext3DState_Token:
      *value = command_buffer_->GetToken();
      break;
    case NPDeviceContext3DState_ParseError:
      *value = command_buffer_->ResetParseError();
      break;
    case NPDeviceContext3DState_ErrorStatus:
      *value = command_buffer_->GetErrorStatus() ? 1 : 0;
      break;
    default:
      return NPERR_GENERIC_ERROR;
  };
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DFlushContext(
    NPP id,
    NPDeviceContext3D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
#if defined(ENABLE_GPU)
  DCHECK(callback == NULL);
  context->getOffset = command_buffer_->SyncOffsets(context->putOffset);
#endif  // ENABLE_GPU
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DDestroyContext(
    NPDeviceContext3D* context) {
#if defined(ENABLE_GPU)
  command_buffer_.reset();

  if (nested_delegate_) {
    nested_delegate_->PluginDestroyed();
    nested_delegate_ = NULL;
  }
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DCreateBuffer(
    NPDeviceContext3D* context,
    size_t size,
    int32* id) {
#if defined(ENABLE_GPU)
  *id = command_buffer_->CreateTransferBuffer(size);
  if (*id < 0)
    return NPERR_GENERIC_ERROR;
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DDestroyBuffer(
    NPDeviceContext3D* context,
    int32 id) {
#if defined(ENABLE_GPU)
  command_buffer_->DestroyTransferBuffer(id);
#endif  // ENABLE_GPU
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DMapBuffer(
    NPDeviceContext3D* context,
    int32 id,
    NPDeviceBuffer* np_buffer) {
#if defined(ENABLE_GPU)
  Buffer gpu_buffer = command_buffer_->GetTransferBuffer(id);
  np_buffer->ptr = gpu_buffer.ptr;
  np_buffer->size = gpu_buffer.size;
  if (!np_buffer->ptr)
    return NPERR_GENERIC_ERROR;
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

WebPluginDelegatePepper::WebPluginDelegatePepper(
    const base::WeakPtr<RenderView>& render_view,
    gfx::PluginWindowHandle containing_view,
    NPAPI::PluginInstance *instance)
    : render_view_(render_view),
      plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      buffer_size_(0),
      plugin_buffer_(0),
      nested_delegate_(NULL) {
  // For now we keep a window struct, although it isn't used.
  memset(&window_, 0, sizeof(window_));
  // All Pepper plugins are windowless and transparent.
  // TODO(sehr): disable resetting these NPPVs by plugins.
  instance->set_windowless(true);
  instance->set_transparent(true);
}

WebPluginDelegatePepper::~WebPluginDelegatePepper() {
  DestroyInstance();
}

void WebPluginDelegatePepper::PluginDestroyed() {
  delete this;
}

void WebPluginDelegatePepper::Paint(WebKit::WebCanvas* canvas,
                                    const gfx::Rect& rect) {
#if defined(OS_WIN)
  if (nested_delegate_) {
    // TODO(apatrick): The GPU plugin will render to an offscreen render target.
    //    Need to copy it to the screen here.
  } else {
    // Blit from background_context to context.
    if (!committed_bitmap_.isNull()) {
      gfx::Point origin(window_rect_.origin().x(), window_rect_.origin().y());
      canvas->drawBitmap(committed_bitmap_,
                         SkIntToScalar(window_rect_.origin().x()),
                         SkIntToScalar(window_rect_.origin().y()));
    }
  }
#endif
}

void WebPluginDelegatePepper::Print(gfx::NativeDrawingContext context) {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepper::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepper::SetFocus() {
  NPPepperEvent npevent;

  npevent.type = NPEventType_Focus;
  npevent.size = sizeof(npevent);
  // TODO(sehr): what timestamp should this have?
  npevent.timeStampSeconds = 0.0;
  // Currently this API only supports gaining focus.
  npevent.u.focus.value = 1;
  instance()->NPP_HandleEvent(&npevent);
}

// Anonymous namespace for functions converting WebInputEvents to NPAPI types.
namespace {
NPEventTypes ConvertEventTypes(WebInputEvent::Type wetype) {
  switch (wetype) {
    case WebInputEvent::MouseDown:
      return NPEventType_MouseDown;
    case WebInputEvent::MouseUp:
      return NPEventType_MouseUp;
    case WebInputEvent::MouseMove:
      return NPEventType_MouseMove;
    case WebInputEvent::MouseEnter:
      return NPEventType_MouseEnter;
    case WebInputEvent::MouseLeave:
      return NPEventType_MouseLeave;
    case WebInputEvent::MouseWheel:
      return NPEventType_MouseWheel;
    case WebInputEvent::RawKeyDown:
      return NPEventType_RawKeyDown;
    case WebInputEvent::KeyDown:
      return NPEventType_KeyDown;
    case WebInputEvent::KeyUp:
      return NPEventType_KeyUp;
    case WebInputEvent::Char:
      return NPEventType_Char;
    case WebInputEvent::Undefined:
    default:
      return NPEventType_Undefined;
  }
}

void BuildKeyEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKeyboardEvent*>(event);
  npevent->u.key.modifier = key_event->modifiers;
  npevent->u.key.normalizedKeyCode = key_event->windowsKeyCode;
}

void BuildCharEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKeyboardEvent*>(event);
  npevent->u.character.modifier = key_event->modifiers;
  // For consistency, check that the sizes of the texts agree.
  DCHECK(sizeof(npevent->u.character.text) == sizeof(key_event->text));
  DCHECK(sizeof(npevent->u.character.unmodifiedText) ==
         sizeof(key_event->unmodifiedText));
  for (size_t i = 0; i < WebKeyboardEvent::textLengthCap; ++i) {
    npevent->u.character.text[i] = key_event->text[i];
    npevent->u.character.unmodifiedText[i] = key_event->unmodifiedText[i];
  }
}

void BuildMouseEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebMouseEvent* mouse_event =
      reinterpret_cast<const WebMouseEvent*>(event);
  npevent->u.mouse.modifier = mouse_event->modifiers;
  npevent->u.mouse.button = mouse_event->button;
  npevent->u.mouse.x = mouse_event->x;
  npevent->u.mouse.y = mouse_event->y;
  npevent->u.mouse.clickCount = mouse_event->clickCount;
}

void BuildMouseWheelEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebMouseWheelEvent* mouse_wheel_event =
      reinterpret_cast<const WebMouseWheelEvent*>(event);
  npevent->u.wheel.modifier = mouse_wheel_event->modifiers;
  npevent->u.wheel.deltaX = mouse_wheel_event->deltaX;
  npevent->u.wheel.deltaY = mouse_wheel_event->deltaY;
  npevent->u.wheel.wheelTicksX = mouse_wheel_event->wheelTicksX;
  npevent->u.wheel.wheelTicksY = mouse_wheel_event->wheelTicksY;
  npevent->u.wheel.scrollByPage = mouse_wheel_event->scrollByPage;
}
}  // namespace

bool WebPluginDelegatePepper::HandleInputEvent(const WebInputEvent& event,
                                               WebCursorInfo* cursor_info) {
  NPPepperEvent npevent;

  npevent.type = ConvertEventTypes(event.type);
  npevent.size = sizeof(npevent);
  npevent.timeStampSeconds = event.timeStampSeconds;
  switch (npevent.type) {
    case NPEventType_Undefined:
      return false;
    case NPEventType_MouseDown:
    case NPEventType_MouseUp:
    case NPEventType_MouseMove:
    case NPEventType_MouseEnter:
    case NPEventType_MouseLeave:
      BuildMouseEvent(&event, &npevent);
      break;
    case NPEventType_MouseWheel:
      BuildMouseWheelEvent(&event, &npevent);
      break;
    case NPEventType_RawKeyDown:
    case NPEventType_KeyDown:
    case NPEventType_KeyUp:
      BuildKeyEvent(&event, &npevent);
      break;
    case NPEventType_Char:
      BuildCharEvent(&event, &npevent);
      break;
    case NPEventType_Minimize:
    case NPEventType_Focus:
    case NPEventType_Device:
      // NOTIMPLEMENTED();
      break;
  }
  return instance()->NPP_HandleEvent(&npevent) != 0;
}
