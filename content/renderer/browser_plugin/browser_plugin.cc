// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#if defined (OS_WIN)
#include "base/sys_info.h"
#endif
#include "base/utf_string_conversions.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/view_messages.h"
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
const char kCrashEventName[] = "crash";
const char kIsTopLevel[] = "isTopLevel";
const char kLoadAbortEventName[] = "loadAbort";
const char kLoadCommitEventName[] = "loadCommit";
const char kLoadRedirectEventName[] = "loadRedirect";
const char kLoadStartEventName[] = "loadStart";
const char kLoadStopEventName[] = "loadStop";
const char kNewURL[] = "newUrl";
const char kOldURL[] = "oldUrl";
const char kPartitionAttribute[] = "partition";
const char kPersistPrefix[] = "persist:";
const char kSrcAttribute[] = "src";
const char kType[] = "type";
const char kURL[] = "url";
}

BrowserPlugin::BrowserPlugin(
    int instance_id,
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebPluginParams& params)
    : instance_id_(instance_id),
      render_view_(render_view->AsWeakPtr()),
      render_view_routing_id_(render_view->GetRoutingID()),
      container_(NULL),
      damage_buffer_(NULL),
      sad_guest_(NULL),
      guest_crashed_(false),
      resize_pending_(false),
      navigate_src_sent_(false),
      process_id_(-1),
      persist_storage_(false),
      content_window_routing_id_(MSG_ROUTING_NONE),
      visible_(true),
      current_nav_entry_index_(0),
      nav_entry_count_(0) {
  BrowserPluginManager::Get()->AddBrowserPlugin(instance_id, this);
  bindings_.reset(new BrowserPluginBindings(this));

  ParseAttributes(params);
}

BrowserPlugin::~BrowserPlugin() {
  if (damage_buffer_)
    FreeDamageBuffer();
  RemoveEventListeners();
  BrowserPluginManager::Get()->RemoveBrowserPlugin(instance_id_);
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_PluginDestroyed(
          render_view_routing_id_,
          instance_id_));
}

void BrowserPlugin::Cleanup() {
  if (damage_buffer_)
    FreeDamageBuffer();
}

std::string BrowserPlugin::GetSrcAttribute() const {
  return src_;
}

void BrowserPlugin::SetSrcAttribute(const std::string& src) {
  if (src.empty() || (src == src_ && !guest_crashed_))
    return;

  // If we haven't created the guest yet, do so now. We will navigate it right
  // after creation. If |src| is empty, we can delay the creation until we
  // acutally need it.
  if (!navigate_src_sent_) {
    BrowserPluginManager::Get()->Send(
        new BrowserPluginHostMsg_CreateGuest(
            render_view_routing_id_,
            instance_id_,
            storage_partition_id_,
            persist_storage_));
  }

  scoped_ptr<BrowserPluginHostMsg_ResizeGuest_Params> params(
      GetPendingResizeParams());
  DCHECK(!params->resize_pending);

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_NavigateGuest(
          render_view_routing_id_,
          instance_id_,
          src,
          *params));
  // Record that we sent a NavigateGuest message to embedder.
  // Once this instance has navigated, the storage partition cannot be changed,
  // so this value is used for enforcing this.
  navigate_src_sent_ = true;
  src_ = src;
}

NPObject* BrowserPlugin::GetContentWindow() const {
  if (content_window_routing_id_ == MSG_ROUTING_NONE)
    return NULL;
  RenderViewImpl* guest_render_view = static_cast<RenderViewImpl*>(
      ChildThread::current()->ResolveRoute(content_window_routing_id_));
  if (!guest_render_view)
    return NULL;
  WebKit::WebFrame* guest_frame = guest_render_view->GetWebView()->mainFrame();
  return guest_frame->windowObject();
}

std::string BrowserPlugin::GetPartitionAttribute() const {
  std::string value;
  if (persist_storage_)
    value.append(kPersistPrefix);

  value.append(storage_partition_id_);
  return value;
}

bool BrowserPlugin::CanGoBack() const {
  return nav_entry_count_ > 1 && current_nav_entry_index_ > 0;
}

bool BrowserPlugin::CanGoForward() const {
  return current_nav_entry_index_ >= 0 &&
      current_nav_entry_index_ < (nav_entry_count_ - 1);
}

bool BrowserPlugin::SetPartitionAttribute(const std::string& partition_id,
                                          std::string& error_message) {
  if (navigate_src_sent_) {
    error_message =
      "The object has already navigated, so its partition cannot be changed.";
    return false;
  }

  std::string input = partition_id;

  // Since the "persist:" prefix is in ASCII, StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (StartsWithASCII(input, kPersistPrefix, true)) {
    size_t index = input.find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    input = input.substr(index + 1);
    if (input.empty()) {
      error_message = "Invalid empty partition attribute.";
      return false;
    }
    persist_storage_ = true;
  } else {
    persist_storage_ = false;
  }

  storage_partition_id_ = input;
  return true;
}

void BrowserPlugin::ParseAttributes(const WebKit::WebPluginParams& params) {
  std::string src;

  // Get the src attribute from the attributes vector
  for (unsigned i = 0; i < params.attributeNames.size(); ++i) {
    std::string attributeName = params.attributeNames[i].utf8();
    if (LowerCaseEqualsASCII(attributeName, kSrcAttribute)) {
      src = params.attributeValues[i].utf8();
    } else if (LowerCaseEqualsASCII(attributeName, kPartitionAttribute)) {
      std::string error;
      SetPartitionAttribute(params.attributeValues[i].utf8(), error);
    }
  }

  // Set the 'src' attribute last, as it will set the has_navigated_ flag to
  // true, which prevents changing the 'partition' attribute.
  SetSrcAttribute(src);
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

void BrowserPlugin::Back() {
  if (!navigate_src_sent_)
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Go(render_view_routing_id_,
                                  instance_id_, -1));
}

void BrowserPlugin::Forward() {
  if (!navigate_src_sent_)
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Go(render_view_routing_id_,
                                  instance_id_, 1));
}

void BrowserPlugin::Go(int relative_index) {
  if (!navigate_src_sent_)
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Go(render_view_routing_id_,
                                  instance_id_,
                                  relative_index));
}

void BrowserPlugin::TerminateGuest() {
  if (!navigate_src_sent_)
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_TerminateGuest(render_view_routing_id_,
                                              instance_id_));
}

void BrowserPlugin::Stop() {
  if (!navigate_src_sent_)
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Stop(render_view_routing_id_,
                                    instance_id_));
}

void BrowserPlugin::Reload() {
  if (!navigate_src_sent_)
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Reload(render_view_routing_id_,
                                      instance_id_));
}

void BrowserPlugin::UpdateRect(
    int message_id,
    const BrowserPluginMsg_UpdateRect_Params& params) {
  if (width() != params.view_size.width() ||
      height() != params.view_size.height()) {
    BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
        render_view_routing_id_,
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
      render_view_routing_id_,
      instance_id_,
      message_id,
      gfx::Size()));
}

void BrowserPlugin::GuestCrashed() {
  guest_crashed_ = true;
  container_->invalidate();
  // We won't paint the contents of the current backing store again so we might
  // as well toss it out and save memory.
  backing_store_.reset();

  if (!HasListeners(kCrashEventName))
    return;

  WebKit::WebElement plugin = container()->element();
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(
      plugin.document().frame()->mainWorldScriptContext());

  // TODO(fsamuel): Copying the event listeners is insufficent because
  // new persistent handles are not created when the copy constructor is
  // called. See http://crbug.com/155044.
  EventListeners listeners(event_listener_map_[kCrashEventName]);
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    WebKit::WebFrame* frame = plugin.document().frame();
    if (frame)
      frame->callFunctionEvenIfScriptDisabled(*it, v8::Object::New(), 0, NULL);
  }
}

void BrowserPlugin::LoadStart(const GURL& url, bool is_top_level) {
  if (!HasListeners(kLoadStartEventName))
    return;

  WebKit::WebElement plugin = container()->element();
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(
      plugin.document().frame()->mainWorldScriptContext());

  // Construct the loadStart event object.
  v8::Local<v8::Object> event = v8::Object::New();
  event->Set(v8::String::New(kURL, sizeof(kURL) - 1),
             v8::String::New(url.spec().data(), url.spec().size()));
  event->Set(v8::String::New(kIsTopLevel, sizeof(kIsTopLevel) - 1),
             v8::Boolean::New(is_top_level));
  v8::Local<v8::Value> val = event;

  // TODO(fsamuel): Copying the event listeners is insufficent because
  // new persistent handles are not created when the copy constructor is
  // called. See http://crbug.com/155044.
  EventListeners listeners(event_listener_map_[kLoadStartEventName]);
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    WebKit::WebFrame* frame = plugin.document().frame();
    if (frame)
      frame->callFunctionEvenIfScriptDisabled(*it, v8::Object::New(), 1, &val);
  }
}

void BrowserPlugin::LoadCommit(
    const BrowserPluginMsg_LoadCommit_Params& params) {
  // If the guest has just committed a new navigation then it is no longer
  // crashed.
  guest_crashed_ = false;
  src_ = params.url.spec();
  process_id_ = params.process_id;
  current_nav_entry_index_ = params.current_entry_index;
  nav_entry_count_ = params.entry_count;

  if (!HasListeners(kLoadCommitEventName))
    return;

  WebKit::WebElement plugin = container()->element();
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(
      plugin.document().frame()->mainWorldScriptContext());

  // Construct the loadCommit event object.
  v8::Local<v8::Object> event = v8::Object::New();
  event->Set(v8::String::New(kURL, sizeof(kURL) - 1),
             v8::String::New(src_.data(), src_.size()));
  event->Set(v8::String::New(kIsTopLevel, sizeof(kIsTopLevel) - 1),
             v8::Boolean::New(params.is_top_level));
  v8::Local<v8::Value> val = event;

  // TODO(fsamuel): Copying the event listeners is insufficent because
  // new persistent handles are not created when the copy constructor is
  // called. See http://crbug.com/155044.
  EventListeners listeners(event_listener_map_[kLoadCommitEventName]);
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    WebKit::WebFrame* frame = plugin.document().frame();
    if (frame)
      frame->callFunctionEvenIfScriptDisabled(*it, v8::Object::New(), 1, &val);
  }
}

void BrowserPlugin::LoadStop() {
  if (!HasListeners(kLoadStopEventName))
    return;

  WebKit::WebElement plugin = container()->element();
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(
      plugin.document().frame()->mainWorldScriptContext());

  // Construct the loadStop event object.
  v8::Local<v8::Object> event = v8::Object::New();
  v8::Local<v8::Value> val = event;

  // TODO(fsamuel): Copying the event listeners is insufficent because
  // new persistent handles are not created when the copy constructor is
  // called. See http://crbug.com/155044.
  EventListeners listeners(event_listener_map_[kLoadStopEventName]);
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    WebKit::WebFrame* frame = plugin.document().frame();
    if (frame)
      frame->callFunctionEvenIfScriptDisabled(*it, v8::Object::New(), 1, &val);
  }
}

void BrowserPlugin::LoadAbort(const GURL& url,
                              bool is_top_level,
                              const std::string& type) {
  if (!HasListeners(kLoadAbortEventName))
    return;

  WebKit::WebElement plugin = container()->element();
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(
      plugin.document().frame()->mainWorldScriptContext());

  // Construct the loadAbort event object.
  v8::Local<v8::Object> event = v8::Object::New();
  event->Set(v8::String::New(kURL, sizeof(kURL) - 1),
             v8::String::New(url.spec().data(), url.spec().size()));
  event->Set(v8::String::New(kIsTopLevel, sizeof(kIsTopLevel) - 1),
             v8::Boolean::New(is_top_level));
  event->Set(v8::String::New(kType, sizeof(kType) - 1),
             v8::String::New(type.data(), type.size()));
  v8::Local<v8::Value> val = event;

  // TODO(fsamuel): Copying the event listeners is insufficent because
  // new persistent handles are not created when the copy constructor is
  // called. See http://crbug.com/155044.
  EventListeners listeners(event_listener_map_[kLoadAbortEventName]);
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    WebKit::WebFrame* frame = plugin.document().frame();
    if (frame)
      frame->callFunctionEvenIfScriptDisabled(*it, v8::Object::New(), 1, &val);
  }
}

void BrowserPlugin::LoadRedirect(const GURL& old_url,
                                 const GURL& new_url,
                                 bool is_top_level) {
  if (!HasListeners(kLoadRedirectEventName))
    return;

  WebKit::WebElement plugin = container()->element();
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(
      plugin.document().frame()->mainWorldScriptContext());

  // Construct the loadRedirect event object.
  v8::Local<v8::Object> event = v8::Object::New();
  event->Set(v8::String::New(kOldURL, sizeof(kOldURL) - 1),
             v8::String::New(old_url.spec().data(), old_url.spec().size()));
  event->Set(v8::String::New(kNewURL, sizeof(kNewURL) - 1),
             v8::String::New(new_url.spec().data(), new_url.spec().size()));
  event->Set(v8::String::New(kIsTopLevel, sizeof(kIsTopLevel) - 1),
             v8::Boolean::New(is_top_level));
  v8::Local<v8::Value> val = event;

  // TODO(fsamuel): Copying the event listeners is insufficent because
  // new persistent handles are not created when the copy constructor is
  // called. See http://crbug.com/155044.
  EventListeners listeners(event_listener_map_[kLoadRedirectEventName]);
  EventListeners::iterator it = listeners.begin();
  for (; it != listeners.end(); ++it) {
    WebKit::WebFrame* frame = plugin.document().frame();
    if (frame)
      frame->callFunctionEvenIfScriptDisabled(*it, v8::Object::New(), 1, &val);
  }
}

void BrowserPlugin::AdvanceFocus(bool reverse) {
  // We do not have a RenderView when we are testing.
  if (render_view_)
    render_view_->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::GuestContentWindowReady(int content_window_routing_id) {
  DCHECK(content_window_routing_id != MSG_ROUTING_NONE);
  content_window_routing_id_ = content_window_routing_id;
}

void BrowserPlugin::SetAcceptTouchEvents(bool accept) {
  if (container())
    container()->setIsAcceptingTouchEvents(accept);
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

bool BrowserPlugin::canProcessDrag() const {
  return true;
}

void BrowserPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  if (guest_crashed_) {
    if (!sad_guest_)  // Lazily initialize bitmap.
      sad_guest_ = content::GetContentClient()->renderer()->
          GetSadPluginBitmap();
    // TODO(fsamuel): Do we want to paint something other than a sad plugin
    // on crash? See http://www.crbug.com/140266.
    // content_shell does not have the sad plugin bitmap, so we'll paint black
    // instead to make it clear that something went wrong.
    if (sad_guest_) {
      webkit::PaintSadPlugin(canvas, plugin_rect_, *sad_guest_);
      return;
    }
  }
  SkAutoCanvasRestore auto_restore(canvas, true);
  canvas->translate(plugin_rect_.x(), plugin_rect_.y());
  SkRect image_data_rect = SkRect::MakeXYWH(
      SkIntToScalar(0),
      SkIntToScalar(0),
      SkIntToScalar(plugin_rect_.width()),
      SkIntToScalar(plugin_rect_.height()));
  canvas->clipRect(image_data_rect);
  // Paint black or white in case we have nothing in our backing store or we
  // need to show a gutter.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(guest_crashed_ ? SK_ColorBLACK : SK_ColorWHITE);
  canvas->drawRect(image_data_rect, paint);
  // Stay a solid color if we have never set a non-empty src, or we don't have a
  // backing store.
  if (!backing_store_.get() || !navigate_src_sent_)
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
      old_height == window_rect.height) {
    return;
  }

  const size_t stride = skia::PlatformCanvas::StrideForWidth(window_rect.width);
  // Make sure the size of the damage buffer is at least four bytes so that we
  // can fit in a magic word to verify that the memory is shared correctly.
  size_t size =
      std::max(sizeof(unsigned int),
               static_cast<size_t>(window_rect.height *
                                   stride *
                                   GetDeviceScaleFactor() *
                                   GetDeviceScaleFactor()));

  // Don't drop the old damage buffer until after we've made sure that the
  // browser process has dropped it.
  TransportDIB* new_damage_buffer = CreateTransportDIB(size);
  pending_resize_params_.reset();

  scoped_ptr<BrowserPluginHostMsg_ResizeGuest_Params> params(
      new BrowserPluginHostMsg_ResizeGuest_Params);
  params->damage_buffer_id = new_damage_buffer->id();
#if defined(OS_MACOSX)
  // |damage_buffer_id| is not enough to retrieve the damage buffer (on browser
  // side) since we don't let the browser cache the damage buffer. We need a
  // handle to the damage buffer for this.
  params->damage_buffer_handle = new_damage_buffer->handle();
#endif
#if defined(OS_WIN)
  params->damage_buffer_size = size;
#endif
  params->width = window_rect.width;
  params->height = window_rect.height;
  params->resize_pending = resize_pending_;
  params->scale_factor = GetDeviceScaleFactor();

  if (navigate_src_sent_) {
    BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ResizeGuest(
        render_view_routing_id_,
        instance_id_,
        *params));
    resize_pending_ = true;
  } else {
    // Until an actual navigation occurs, there is no browser-side embedder
    // present to notify about geometry updates. In this case, after we've
    // updated the BrowserPlugin's state we are done and we do not send a resize
    // message to the browser.
    pending_resize_params_.reset(params.release());
  }
  if (damage_buffer_)
    FreeDamageBuffer();
  damage_buffer_ = new_damage_buffer;
}

void BrowserPlugin::FreeDamageBuffer() {
  DCHECK(damage_buffer_);
#if defined(OS_MACOSX)
  // We don't need to (nor should we) send ViewHostMsg_FreeTransportDIB
  // message to the browser to free the damage buffer since we manage the
  // damage buffer ourselves.
  delete damage_buffer_;
#else
  RenderProcess::current()->FreeTransportDIB(damage_buffer_);
  damage_buffer_ = NULL;
#endif
}

BrowserPluginHostMsg_ResizeGuest_Params*
    BrowserPlugin::GetPendingResizeParams() {
  if (pending_resize_params_.get()) {
    resize_pending_ = true;
    return pending_resize_params_.release();
  } else {
    BrowserPluginHostMsg_ResizeGuest_Params* params =
        new BrowserPluginHostMsg_ResizeGuest_Params;

    // We don't have a pending resize to send, so we send an invalid transport
    // dib Id.
    params->damage_buffer_id = TransportDIB::Id();
    params->width = width();
    params->height = height();
    params->resize_pending = false;
    return params;
  }
}

TransportDIB* BrowserPlugin::CreateTransportDIB(const size_t size) {
#if defined(OS_MACOSX)
  TransportDIB::Handle handle;
  // On OSX we don't let the browser manage the transport dib. We manage the
  // deletion of the dib in FreeDamageBuffer().
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(
      size,
      false,  // cache in browser.
      &handle);
  TransportDIB* new_damage_buffer = NULL;
  if (BrowserPluginManager::Get()->Send(msg) && handle.fd >= 0)
    new_damage_buffer = TransportDIB::Map(handle);
#else
  TransportDIB* new_damage_buffer =
      RenderProcess::current()->CreateTransportDIB(size);
#endif
  if (!new_damage_buffer)
    NOTREACHED() << "Unable to create damage buffer";
#if defined(OS_WIN)
  // Windows does not map the buffer by default.
  CHECK(new_damage_buffer->Map());
#endif
  DCHECK(new_damage_buffer->memory());
  // Insert the magic word.
  *static_cast<unsigned int*>(new_damage_buffer->memory()) = 0xdeadbeef;
  return new_damage_buffer;
}

void BrowserPlugin::updateFocus(bool focused) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetFocus(
      render_view_routing_id_,
      instance_id_,
      focused));
}

void BrowserPlugin::updateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!navigate_src_sent_)
    return;

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetVisibility(
      render_view_routing_id_,
      instance_id_,
      visible));
}

bool BrowserPlugin::acceptsInputEvents() {
  return true;
}

bool BrowserPlugin::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  if (guest_crashed_ || !navigate_src_sent_)
    return false;
  bool handled = false;
  WebCursor cursor;
  IPC::Message* message =
      new BrowserPluginHostMsg_HandleInputEvent(
          render_view_routing_id_,
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

bool BrowserPlugin::handleDragStatusUpdate(WebKit::WebDragStatus drag_status,
                                           const WebKit::WebDragData& drag_data,
                                           WebKit::WebDragOperationsMask mask,
                                           const WebKit::WebPoint& position,
                                           const WebKit::WebPoint& screen) {
  if (guest_crashed_ || !navigate_src_sent_)
    return false;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_DragStatusUpdate(
        render_view_routing_id_,
        instance_id_,
        drag_status,
        WebDropData(drag_data),
        mask,
        position));
  return false;
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
