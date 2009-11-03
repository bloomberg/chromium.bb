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
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/pepper/pepper.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

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

uint32 WebPluginDelegatePepper::next_buffer_id = 0;

WebPluginDelegatePepper* WebPluginDelegatePepper::Create(
    const FilePath& filename,
    const std::string& mime_type,
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
  return new WebPluginDelegatePepper(containing_view, instance.get());
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
  uint32 buffer_size = window_rect.height() *
                       window_rect.width() *
                       kBytesPerPixel;
  if (buffer_size_ < buffer_size) {
    buffer_size_ = buffer_size;
    plugin_buffer_ = TransportDIB::Create(buffer_size, ++next_buffer_id);
  }

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
    int resource_id, const GURL& url, bool notify_needed,
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

bool WebPluginDelegatePepper::IsPluginDelegateWindow(
    gfx::NativeWindow window) {
  return false;
}

bool WebPluginDelegatePepper::GetPluginNameFromWindow(
    gfx::NativeWindow window, std::wstring *plugin_name) {
  return false;
}

bool WebPluginDelegatePepper::IsDummyActivationWindow(
    gfx::NativeWindow window) {
  return false;
}

WebPluginDelegatePepper::WebPluginDelegatePepper(
    gfx::PluginWindowHandle containing_view,
    NPAPI::PluginInstance *instance)
    : plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      buffer_size_(0),
      plugin_buffer_(0) {
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
  // Blit from background_context to context.
  if (!committed_bitmap_.isNull()) {
    gfx::Point origin(window_rect_.origin().x(), window_rect_.origin().y());
    canvas->drawBitmap(committed_bitmap_,
                       SkIntToScalar(window_rect_.origin().x()),
                       SkIntToScalar(window_rect_.origin().y()));
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
    case NPEventType_Char:
      BuildCharEvent(&event, &npevent);
      break;
    case NPEventType_Minimize:
    case NPEventType_Focus:
    case NPEventType_Device:
      NOTIMPLEMENTED();
      break;
  }
  return instance()->NPP_HandleEvent(&npevent) != 0;
}

NPError WebPluginDelegatePepper::InitializeRenderContext(
    NPRenderType type, NPRenderContext* context) {
  switch (type) {
    case NPRenderGraphicsRGBA: {
      int width = window_rect_.width();
      int height = window_rect_.height();

      plugin_canvas_.reset(plugin_buffer_->GetPlatformCanvas(width, height));
      if (!plugin_canvas_.get())
        return NPERR_GENERIC_ERROR;

      // Note that we need to get the address out of the bitmap rather than
      // using plugin_buffer_->memory(). The memory() is when the bitmap data
      // has had "Map" called on it. For Windows, this is separate than making a
      // bitmap using the shared section.
      const SkBitmap& plugin_bitmap =
          plugin_canvas_->getTopPlatformDevice().accessBitmap(true);
      SkAutoLockPixels locker(plugin_bitmap);

      // TODO(brettw) this theoretically shouldn't be necessary. But the
      // platform device on Windows will fill itself with green to help you
      // catch areas you didn't paint.
      plugin_bitmap.eraseARGB(0, 0, 0, 0);

      context->u.graphicsRgba.region = plugin_bitmap.getAddr32(0, 0);
      context->u.graphicsRgba.stride = width * kBytesPerPixel;
      return NPERR_NO_ERROR;
    }
    default:
      return NPERR_GENERIC_ERROR;
  }
}

NPError WebPluginDelegatePepper::FlushRenderContext(NPRenderContext* context) {
  // TODO(brettw): we should have some kind of swapping of the canvases so we
  // can double buffer while avoiding this deep copy.
  if (!plugin_canvas_->getTopPlatformDevice().accessBitmap(false).copyTo(
          &committed_bitmap_, SkBitmap::kARGB_8888_Config))
    return NPERR_OUT_OF_MEMORY_ERROR;

  committed_bitmap_.setIsOpaque(false);
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::OpenFileInSandbox(const char* file_name,
                                                   void** handle) {
  *handle = NULL;

#if defined(OS_WIN)
  FilePath file_path(UTF8ToUTF16(file_name));
#elif defined(OS_POSIX)
  FilePath file_path(file_name);
#endif

  ViewMsg_OpenFileForPluginResponse_Params result;
  RenderThread::current()->Send(new ViewMsg_OpenFileForPlugin(
      file_path, &result));

#if defined(OS_WIN)
  if (!result.file_handle)
    return NPERR_INVALID_PARAM;
  *handle = result.file_handle;
#elif defined(OS_POSIX)
  if (result.file_handle.fd == -1)
    return NPERR_INVALID_PARAM;
  *reinterpret_cast<int*>(handle) = result.file_handle.fd;
#endif

  return NPERR_NO_ERROR;
}
