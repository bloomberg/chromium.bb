// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "content/common/browser_plugin_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "webkit/plugins/sad_plugin.h"

using WebKit::WebCanvas;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebRect;
using WebKit::WebURL;
using WebKit::WebVector;

namespace content {

namespace {
const char kNavigationEventName[] = "navigation";
const char* kSrcAttribute = "src";
}

BrowserPlugin::BrowserPlugin(
    int instance_id,
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebPluginParams& params)
    : instance_id_(instance_id),
      render_view_(render_view),
      container_(NULL),
      damage_buffer_(NULL),
      sad_guest_(NULL),
      guest_crashed_(false),
      resize_pending_(false),
      parent_frame_(frame->identifier()) {
  BrowserPluginManager::Get()->AddBrowserPlugin(instance_id, this);
  bindings_.reset(new BrowserPluginBindings(this));

  std::string src;
  if (ParseSrcAttribute(params, &src))
    SetSrcAttribute(src);
}

BrowserPlugin::~BrowserPlugin() {
  if (damage_buffer_) {
    RenderProcess::current()->FreeTransportDIB(damage_buffer_);
    damage_buffer_ = NULL;
  }
  RemoveEventListeners();
  BrowserPluginManager::Get()->RemoveBrowserPlugin(instance_id_);
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_PluginDestroyed(
          render_view_->GetRoutingID(),
          instance_id_));
}

void BrowserPlugin::Cleanup() {
  if (damage_buffer_) {
    RenderProcess::current()->FreeTransportDIB(damage_buffer_);
    damage_buffer_ = NULL;
  }
}

std::string BrowserPlugin::GetSrcAttribute() const {
  return src_;
}

void BrowserPlugin::SetSrcAttribute(const std::string& src) {
  if (src == src_ && !guest_crashed_)
    return;
  if (!src.empty()) {
    BrowserPluginManager::Get()->Send(
        new BrowserPluginHostMsg_NavigateOrCreateGuest(
            render_view_->GetRoutingID(),
            instance_id_,
            parent_frame_,
            src));
  }
  src_ = src;
  guest_crashed_ = false;
}

bool BrowserPlugin::ParseSrcAttribute(
    const WebKit::WebPluginParams& params,
    std::string* src) {
  // Get the src attribute from the attributes vector
  for (unsigned i = 0; i < params.attributeNames.size(); ++i) {
    std::string attributeName = params.attributeNames[i].utf8();
    if (LowerCaseEqualsASCII(attributeName, kSrcAttribute)) {
      *src = params.attributeValues[i].utf8();
      return true;
    }
  }
  return false;
}

float BrowserPlugin::GetDeviceScaleFactor() const {
  if (!render_view_)
    return 1.0f;
  return render_view_->GetWebView()->deviceScaleFactor();
}

void BrowserPlugin::RemoveEventListeners() {
  EventListenerMap::iterator event_listener_map_iter =
      event_listener_map_.begin();
  for (; event_listener_map_iter != event_listener_map_.end();
       ++event_listener_map_iter) {
    EventListeners& listeners =
        event_listener_map_[event_listener_map_iter->first];
    EventListeners::iterator it = listeners.begin();
    for (; it != listeners.end(); ++it) {
      it->Dispose();
    }
  }
  event_listener_map_.clear();
}

void BrowserPlugin::UpdateRect(
    int message_id,
    const BrowserPluginMsg_UpdateRect_Params& params) {
  if (width() != params.view_size.width() ||
      height() != params.view_size.height()) {
    BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
        render_view_->GetRoutingID(),
        instance_id_,
        message_id,
        gfx::Size(width(), height())));
    return;
  }

  float backing_store_scale_factor =
      backing_store_.get() ? backing_store_->GetScaleFactor() : 1.0f;

  if (params.is_resize_ack ||
      backing_store_scale_factor != params.scale_factor) {
    resize_pending_ = !params.is_resize_ack;
    backing_store_.reset(
        new BrowserPluginBackingStore(gfx::Size(width(), height()),
                                      params.scale_factor));
  }

  // Update the backing store.
  if (!params.scroll_rect.IsEmpty()) {
    backing_store_->ScrollBackingStore(params.dx,
                                       params.dy,
                                       params.scroll_rect,
                                       params.view_size);
  }
  for (unsigned i = 0; i < params.copy_rects.size(); i++) {
    backing_store_->PaintToBackingStore(params.bitmap_rect,
                                        params.copy_rects,
                                        damage_buffer_);
  }
  // Invalidate the container.
  container_->invalidate();
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
      render_view_->GetRoutingID(),
      instance_id_,
      message_id,
      gfx::Size()));
}

void BrowserPlugin::GuestCrashed() {
  guest_crashed_ = true;
  container_->invalidate();
}

void BrowserPlugin::DidNavigate(const GURL& url) {
  src_ = url.spec();
  if (!HasListeners(kNavigationEventName))
    return;

  EventListeners& listeners = event_listener_map_[kNavigationEventName];
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    v8::Context::Scope context_scope(v8::Context::New());
    v8::HandleScope handle_scope;
    v8::Local<v8::Value> param =
        v8::Local<v8::Value>::New(v8::String::New(src_.c_str()));
    container()->element().document().frame()->
        callFunctionEvenIfScriptDisabled(*it,
                                         v8::Object::New(),
                                         1,
                                         &param);
  }
}

void BrowserPlugin::AdvanceFocus(bool reverse) {
  // We do not have a RenderView when we are testing.
  if (render_view_)
    render_view_->GetWebView()->advanceFocus(reverse);
}

bool BrowserPlugin::HasListeners(const std::string& event_name) {
  return event_listener_map_.find(event_name) != event_listener_map_.end();
}

bool BrowserPlugin::AddEventListener(const std::string& event_name,
                                     v8::Local<v8::Function> function) {
  EventListeners& listeners = event_listener_map_[event_name];
  for (unsigned int i = 0; i < listeners.size(); ++i) {
    if (listeners[i] == function)
      return false;
  }
  v8::Persistent<v8::Function> persistent_function =
      v8::Persistent<v8::Function>::New(function);
  listeners.push_back(persistent_function);
  return true;
}

bool BrowserPlugin::RemoveEventListener(const std::string& event_name,
                                        v8::Local<v8::Function> function) {
  if (event_listener_map_.find(event_name) == event_listener_map_.end())
    return false;

  EventListeners& listeners = event_listener_map_[event_name];
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    if (*it == function) {
      it->Dispose();
      listeners.erase(it);
      return true;
    }
  }
  return false;
}

WebKit::WebPluginContainer* BrowserPlugin::container() const {
  return container_;
}

bool BrowserPlugin::initialize(WebPluginContainer* container) {
  container_ = container;
  return true;
}

void BrowserPlugin::destroy() {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* BrowserPlugin::scriptableObject() {
  NPObject* browser_plugin_np_object(bindings_->np_object());
  // The object is expected to be retained before it is returned.
  WebKit::WebBindings::retainObject(browser_plugin_np_object);
  return browser_plugin_np_object;
}

bool BrowserPlugin::supportsKeyboardFocus() const {
  return true;
}

void BrowserPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  if (guest_crashed_) {
    if (!sad_guest_)  // Lazily initialize bitmap.
      sad_guest_ = content::GetContentClient()->renderer()->
          GetSadPluginBitmap();
    // TODO(fsamuel): Do we want to paint something other than a sad plugin
    // on crash? See http://www.crbug.com/140266.
    webkit::PaintSadPlugin(canvas, plugin_rect_, *sad_guest_);
    return;
  }
  SkAutoCanvasRestore auto_restore(canvas, true);
  canvas->translate(plugin_rect_.x(), plugin_rect_.y());
  SkRect image_data_rect = SkRect::MakeXYWH(
      SkIntToScalar(0),
      SkIntToScalar(0),
      SkIntToScalar(plugin_rect_.width()),
      SkIntToScalar(plugin_rect_.height()));
  canvas->clipRect(image_data_rect);
  // Paint white in case we have nothing in our backing store or we need to
  // show a gutter.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorWHITE);
  canvas->drawRect(image_data_rect, paint);
  // Stay at white if we have no src set, or we don't yet have a backing store.
  if (!backing_store_.get() || src_.empty())
    return;
  float inverse_scale_factor =  1.0f / backing_store_->GetScaleFactor();
  canvas->scale(inverse_scale_factor, inverse_scale_factor);
  canvas->drawBitmap(backing_store_->GetBitmap(), 0, 0);
}

void BrowserPlugin::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  int old_width = width();
  int old_height = height();
  plugin_rect_ = window_rect;
  if (old_width == window_rect.width &&
      old_height == window_rect.height)
    return;

  const size_t stride = skia::PlatformCanvas::StrideForWidth(window_rect.width);
  const size_t size = window_rect.height *
                      stride *
                      GetDeviceScaleFactor() *
                      GetDeviceScaleFactor();

  // Don't drop the old damage buffer until after we've made sure that the
  // browser process has dropped it.
  TransportDIB* new_damage_buffer =
      RenderProcess::current()->CreateTransportDIB(size);
  DCHECK(new_damage_buffer);

  BrowserPluginHostMsg_ResizeGuest_Params params;
  params.damage_buffer_id = new_damage_buffer->id();
  params.width = window_rect.width;
  params.height = window_rect.height;
  params.resize_pending = resize_pending_;
  params.scale_factor = GetDeviceScaleFactor();
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_->GetRoutingID(),
      instance_id_,
      params));
  resize_pending_ = true;

  if (damage_buffer_) {
    RenderProcess::current()->FreeTransportDIB(damage_buffer_);
    damage_buffer_ = NULL;
  }
  damage_buffer_ = new_damage_buffer;
}

void BrowserPlugin::updateFocus(bool focused) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetFocus(
      render_view_->GetRoutingID(),
      instance_id_,
      focused));
}

void BrowserPlugin::updateVisibility(bool visible) {
}

bool BrowserPlugin::acceptsInputEvents() {
  return true;
}

bool BrowserPlugin::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  if (guest_crashed_ || src_.empty())
    return false;
  bool handled = false;
  WebCursor cursor;
  IPC::Message* message =
      new BrowserPluginHostMsg_HandleInputEvent(
          render_view_->GetRoutingID(),
          &handled,
          &cursor);
  message->WriteInt(instance_id_);
  message->WriteData(reinterpret_cast<const char*>(&plugin_rect_),
                     sizeof(gfx::Rect));
  message->WriteData(reinterpret_cast<const char*>(&event), event.size);
  BrowserPluginManager::Get()->Send(message);
  cursor.GetCursorInfo(&cursor_info);
  return handled;
}

void BrowserPlugin::didReceiveResponse(
    const WebKit::WebURLResponse& response) {
}

void BrowserPlugin::didReceiveData(const char* data, int data_length) {
}

void BrowserPlugin::didFinishLoading() {
}

void BrowserPlugin::didFailLoading(const WebKit::WebURLError& error) {
}

void BrowserPlugin::didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                                 void* notify_data) {
}

void BrowserPlugin::didFailLoadingFrameRequest(
    const WebKit::WebURL& url,
    void* notify_data,
    const WebKit::WebURLError& error) {
}

}  // namespace content
