// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include "base/json/json_string_value_serializer.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/browser_plugin/browser_plugin_compositing_helper.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMCustomEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "webkit/plugins/sad_plugin.h"

#if defined (OS_WIN)
#include "base/sys_info.h"
#endif

using WebKit::WebCanvas;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebURL;
using WebKit::WebVector;

namespace content {

namespace {

const int INSTANCE_ID_NONE = 0;

// Events.
const char kEventExit[] = "exit";
const char kEventLoadAbort[] = "loadabort";
const char kEventLoadCommit[] = "loadcommit";
const char kEventLoadRedirect[] = "loadredirect";
const char kEventLoadStart[] = "loadstart";
const char kEventLoadStop[] = "loadstop";
const char kEventResponsive[] = "responsive";
const char kEventSizeChanged[] = "sizechanged";
const char kEventUnresponsive[] = "unresponsive";

// Parameters/properties on events.
const char kIsTopLevel[] = "isTopLevel";
const char kName[] = "name";
const char kNewURL[] = "newUrl";
const char kNewHeight[] = "newHeight";
const char kNewWidth[] = "newWidth";
const char kOldURL[] = "oldUrl";
const char kOldHeight[] = "oldHeight";
const char kOldWidth[] = "oldWidth";
const char kPartition[] = "partition";
const char kPersistPrefix[] = "persist:";
const char kProcessId[] = "processId";
const char kReason[] = "reason";
const char kSrc[] = "src";
const char kURL[] = "url";

// Error messages.
const char kErrorAlreadyNavigated[] =
    "The object has already navigated, so its partition cannot be changed.";
const char kErrorInvalidPartition[] =
    "Invalid partition attribute.";

static std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return "abnormal";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "crashed";
    case base::TERMINATION_STATUS_STILL_RUNNING:
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

static std::string GetInternalEventName(const char* event_name) {
  return base::StringPrintf("-internal-%s", event_name);
}
}

BrowserPlugin::BrowserPlugin(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebPluginParams& params)
    : instance_id_(INSTANCE_ID_NONE),
      render_view_(render_view->AsWeakPtr()),
      render_view_routing_id_(render_view->GetRoutingID()),
      container_(NULL),
      damage_buffer_sequence_id_(0),
      resize_ack_received_(true),
      sad_guest_(NULL),
      guest_crashed_(false),
      navigate_src_sent_(false),
      auto_size_(false),
      max_height_(0),
      max_width_(0),
      min_height_(0),
      min_width_(0),
      process_id_(-1),
      persist_storage_(false),
      valid_partition_id_(true),
      content_window_routing_id_(MSG_ROUTING_NONE),
      plugin_focused_(false),
      visible_(true),
      size_changed_in_flight_(false),
      allocate_instance_id_sent_(false),
      browser_plugin_manager_(render_view->browser_plugin_manager()),
      current_nav_entry_index_(0),
      nav_entry_count_(0),
      compositing_enabled_(false) {
  bindings_.reset(new BrowserPluginBindings(this));

  ParseAttributes(params);
}

BrowserPlugin::~BrowserPlugin() {
  // If the BrowserPlugin has never navigated then the browser process and
  // BrowserPluginManager don't know about it and so there is nothing to do
  // here.
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->RemoveBrowserPlugin(instance_id_);
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_PluginDestroyed(
          render_view_routing_id_,
          instance_id_));
}

bool BrowserPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPlugin, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_BuffersSwapped, OnBuffersSwapped)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestContentWindowReady,
                        OnGuestContentWindowReady)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestResponsive, OnGuestResponsive)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestUnresponsive, OnGuestUnresponsive)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_LoadAbort, OnLoadAbort)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_LoadCommit, OnLoadCommit)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_LoadRedirect, OnLoadRedirect)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_LoadStart, OnLoadStart)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_LoadStop, OnLoadStop)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_UpdatedName, OnUpdatedName)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPlugin::UpdateDOMAttribute(
    const std::string& attribute_name,
    const std::string& attribute_value) {
  if (!container())
    return;

  WebKit::WebElement element = container()->element();
  WebKit::WebString web_attribute_name =
      WebKit::WebString::fromUTF8(attribute_name);
  std::string current_value(element.getAttribute(web_attribute_name).utf8());
  if (current_value == attribute_value)
    return;

  if (attribute_value.empty()) {
    element.removeAttribute(web_attribute_name);
  } else {
    element.setAttribute(web_attribute_name,
        WebKit::WebString::fromUTF8(attribute_value));
  }
}

void BrowserPlugin::SetNameAttribute(const std::string& name) {
  if (name_ == name)
    return;

  name_ = name;
  if (!navigate_src_sent_)
    return;

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_SetName(
          render_view_routing_id_,
          instance_id_,
          name));
}

bool BrowserPlugin::SetSrcAttribute(const std::string& src,
                                    std::string* error_message) {
  if (!valid_partition_id_) {
    *error_message = kErrorInvalidPartition;
    return false;
  }

  if (src.empty() || (src == src_ && !guest_crashed_))
    return true;

  src_ = src;

  // If we haven't created the guest yet, do so now. We will navigate it right
  // after creation. If |src| is empty, we can delay the creation until we
  // actually need it.
  if (!navigate_src_sent_) {
    // On initial navigation, we request an instance ID from the browser
    // process. We essentially ignore all subsequent calls to SetSrcAttribute
    // until we receive an instance ID. |allocate_instance_id_sent_|
    // prevents BrowserPlugin from allocating more than one instance ID.
    // Upon receiving an instance ID from the browser process, we continue
    // the process of navigation by populating the
    // BrowserPluginHostMsg_CreateGuest_Params with the current state of
    // BrowserPlugin and sending a BrowserPluginHostMsg_CreateGuest to the
    // browser process in order to create a new guest.
    if (!allocate_instance_id_sent_) {
      browser_plugin_manager()->AllocateInstanceID(this);
      allocate_instance_id_sent_ = true;
    }
    return true;
  }

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_NavigateGuest(
          render_view_routing_id_,
          instance_id_,
          src));
  return true;
}

void BrowserPlugin::SetAutoSizeAttribute(bool auto_size) {
  if (auto_size_ == auto_size)
    return;
  auto_size_ = auto_size;
  last_view_size_ = plugin_rect_.size();
  UpdateGuestAutoSizeState();
}

void BrowserPlugin::PopulateAutoSizeParameters(
    BrowserPluginHostMsg_AutoSize_Params* params) {
  // If maxWidth or maxHeight have not been set, set them to the container size.
  max_height_ = max_height_ ? max_height_ : height();
  max_width_ = max_width_ ? max_width_ : width();
  // minWidth should not be bigger than maxWidth, and minHeight should not be
  // bigger than maxHeight.
  min_height_ = std::min(min_height_, max_height_);
  min_width_ = std::min(min_width_, max_width_);
  params->enable = auto_size_;
  params->max_size = gfx::Size(max_width_, max_height_);
  params->min_size = gfx::Size(min_width_, min_height_);
}

void BrowserPlugin::UpdateGuestAutoSizeState() {
  // If we haven't yet heard back from the guest about the last resize request,
  // then we don't issue another request until we do in
  // BrowserPlugin::UpdateRect.
  if (!navigate_src_sent_ || !resize_ack_received_)
    return;
  BrowserPluginHostMsg_AutoSize_Params auto_size_params;
  BrowserPluginHostMsg_ResizeGuest_Params resize_guest_params;
  GetDamageBufferWithSizeParams(&auto_size_params, &resize_guest_params);
  resize_ack_received_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetAutoSize(
      render_view_routing_id_,
      instance_id_,
      auto_size_params,
      resize_guest_params));
}

void BrowserPlugin::SizeChangedDueToAutoSize(const gfx::Size& old_view_size) {
  size_changed_in_flight_ = false;

  std::map<std::string, base::Value*> props;
  props[kOldHeight] = base::Value::CreateIntegerValue(old_view_size.height());
  props[kOldWidth] = base::Value::CreateIntegerValue(old_view_size.width());
  props[kNewHeight] = base::Value::CreateIntegerValue(last_view_size_.height());
  props[kNewWidth] = base::Value::CreateIntegerValue(last_view_size_.width());
  TriggerEvent(kEventSizeChanged, &props);
}

// static
bool BrowserPlugin::UsesDamageBuffer(
    const BrowserPluginMsg_UpdateRect_Params& params) {
  return params.damage_buffer_sequence_id != 0;
}

bool BrowserPlugin::UsesPendingDamageBuffer(
    const BrowserPluginMsg_UpdateRect_Params& params) {
  if (!pending_damage_buffer_.get())
    return false;
  return damage_buffer_sequence_id_ == params.damage_buffer_sequence_id;
}

void BrowserPlugin::SetInstanceID(int instance_id) {
  CHECK(instance_id != INSTANCE_ID_NONE);
  instance_id_ = instance_id;
  browser_plugin_manager()->AddBrowserPlugin(instance_id, this);

  BrowserPluginHostMsg_CreateGuest_Params create_guest_params;
  create_guest_params.storage_partition_id = storage_partition_id_;
  create_guest_params.persist_storage = persist_storage_;
  create_guest_params.focused = ShouldGuestBeFocused();
  create_guest_params.visible = visible_;
  create_guest_params.name = name_;
  create_guest_params.src = src_;
  GetDamageBufferWithSizeParams(&create_guest_params.auto_size_params,
                                &create_guest_params.resize_guest_params);
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_CreateGuest(
          render_view_routing_id_,
          instance_id_,
          create_guest_params));

  // Record that we sent a navigation request to the browser process.
  // Once this instance has navigated, the storage partition cannot be changed,
  // so this value is used for enforcing this.
  navigate_src_sent_ = true;
}

void BrowserPlugin::OnAdvanceFocus(int instance_id, bool reverse) {
  DCHECK(render_view_);
  render_view_->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::OnBuffersSwapped(int instance_id,
                                     const gfx::Size& size,
                                     std::string mailbox_name,
                                     int gpu_route_id,
                                     int gpu_host_id) {
  DCHECK(instance_id == instance_id_);
  EnableCompositing(true);

  compositing_helper_->OnBuffersSwapped(size,
                                        mailbox_name,
                                        gpu_route_id,
                                        gpu_host_id);
}

void BrowserPlugin::OnGuestContentWindowReady(int instance_id,
                                            int content_window_routing_id) {
  DCHECK(content_window_routing_id != MSG_ROUTING_NONE);
  content_window_routing_id_ = content_window_routing_id;
}

void BrowserPlugin::OnGuestGone(int instance_id, int process_id, int status) {
  // We fire the event listeners before painting the sad graphic to give the
  // developer an opportunity to display an alternative overlay image on crash.
  std::string termination_status = TerminationStatusToString(
      static_cast<base::TerminationStatus>(status));
  std::map<std::string, base::Value*> props;
  props[kProcessId] = base::Value::CreateIntegerValue(process_id);
  props[kReason] = base::Value::CreateStringValue(termination_status);

  // Event listeners may remove the BrowserPlugin from the document. If that
  // happens, the BrowserPlugin will be scheduled for later deletion (see
  // BrowserPlugin::destroy()). That will clear the container_ reference,
  // but leave other member variables valid below.
  TriggerEvent(kEventExit, &props);

  guest_crashed_ = true;
  // We won't paint the contents of the current backing store again so we might
  // as well toss it out and save memory.
  backing_store_.reset();
  // If the BrowserPlugin is scheduled to be deleted, then container_ will be
  // NULL so we shouldn't attempt to access it.
  if (container_)
    container_->invalidate();
}

void BrowserPlugin::OnGuestResponsive(int instance_id, int process_id) {
  std::map<std::string, base::Value*> props;
  props[kProcessId] = base::Value::CreateIntegerValue(process_id);
  TriggerEvent(kEventResponsive, &props);
}

void BrowserPlugin::OnGuestUnresponsive(int instance_id, int process_id) {
  std::map<std::string, base::Value*> props;
  props[kProcessId] = base::Value::CreateIntegerValue(process_id);
  TriggerEvent(kEventUnresponsive, &props);
}

void BrowserPlugin::OnLoadAbort(int instance_id,
                                const GURL& url,
                                bool is_top_level,
                                const std::string& type) {
  std::map<std::string, base::Value*> props;
  props[kURL] = base::Value::CreateStringValue(url.spec());
  props[kIsTopLevel] = base::Value::CreateBooleanValue(is_top_level);
  props[kReason] = base::Value::CreateStringValue(type);
  TriggerEvent(kEventLoadAbort, &props);
}

void BrowserPlugin::OnLoadCommit(
    int instance_id,
    const BrowserPluginMsg_LoadCommit_Params& params) {
  // If the guest has just committed a new navigation then it is no longer
  // crashed.
  guest_crashed_ = false;
  if (params.is_top_level) {
    src_ = params.url.spec();
    UpdateDOMAttribute(kSrc, src_.c_str());
  }
  process_id_ = params.process_id;
  current_nav_entry_index_ = params.current_entry_index;
  nav_entry_count_ = params.entry_count;

  std::map<std::string, base::Value*> props;
  props[kURL] = base::Value::CreateStringValue(params.url.spec());
  props[kIsTopLevel] = base::Value::CreateBooleanValue(params.is_top_level);
  TriggerEvent(kEventLoadCommit, &props);
}

void BrowserPlugin::OnLoadRedirect(int instance_id,
                                   const GURL& old_url,
                                   const GURL& new_url,
                                   bool is_top_level) {
  std::map<std::string, base::Value*> props;
  props[kOldURL] = base::Value::CreateStringValue(old_url.spec());
  props[kNewURL] = base::Value::CreateStringValue(new_url.spec());
  props[kIsTopLevel] = base::Value::CreateBooleanValue(is_top_level);
  TriggerEvent(kEventLoadRedirect, &props);
}

void BrowserPlugin::OnLoadStart(int instance_id,
                                const GURL& url,
                                bool is_top_level) {
  std::map<std::string, base::Value*> props;
  props[kURL] = base::Value::CreateStringValue(url.spec());
  props[kIsTopLevel] = base::Value::CreateBooleanValue(is_top_level);

  TriggerEvent(kEventLoadStart, &props);
}

void BrowserPlugin::OnLoadStop(int instance_id) {
  TriggerEvent(kEventLoadStop, NULL);
}

void BrowserPlugin::OnSetCursor(int instance_id, const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPlugin::OnShouldAcceptTouchEvents(int instance_id, bool accept) {
  if (container()) {
    container()->requestTouchEventType(accept ?
        WebKit::WebPluginContainer::TouchEventRequestTypeRaw :
        WebKit::WebPluginContainer::TouchEventRequestTypeNone);
  }
}

void BrowserPlugin::OnUpdatedName(int instance_id, const std::string& name) {
  name_ = name;
  UpdateDOMAttribute(kName, name);
}

void BrowserPlugin::OnUpdateRect(
    int instance_id,
    const BrowserPluginMsg_UpdateRect_Params& params) {
  bool use_new_damage_buffer = !backing_store_;
  BrowserPluginHostMsg_AutoSize_Params auto_size_params;
  BrowserPluginHostMsg_ResizeGuest_Params resize_guest_params;
  // If we have a pending damage buffer, and the guest has begun to use the
  // damage buffer then we know the guest will no longer use the current
  // damage buffer. At this point, we drop the current damage buffer, and
  // mark the pending damage buffer as the current damage buffer.
  if (UsesPendingDamageBuffer(params)) {
    SwapDamageBuffers();
    use_new_damage_buffer = true;
  }

  if (params.is_resize_ack || !UsesDamageBuffer(params))
    resize_ack_received_ = true;

  if ((!auto_size_ &&
       (width() != params.view_size.width() ||
        height() != params.view_size.height())) ||
      (auto_size_ && (!InAutoSizeBounds(params.view_size)))) {
    // We are HW accelerated, render widget does not expect an ack,
    // but we still need to update the size.
    if (!params.needs_ack) {
      UpdateGuestAutoSizeState();
      return;
    }

    if (!resize_ack_received_) {
      // The guest has not yet responded to the last resize request, and
      // so we don't want to do anything at this point other than ACK the guest.
      PopulateAutoSizeParameters(&auto_size_params);
    } else {
      // If we have no pending damage buffer, then the guest has not caught up
      // with the BrowserPlugin container. We now tell the guest about the new
      // container size.
      GetDamageBufferWithSizeParams(&auto_size_params,
                                    &resize_guest_params);
    }
    browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
        render_view_routing_id_,
        instance_id_,
        auto_size_params,
        resize_guest_params));
    return;
  }

  if (auto_size_ && (params.view_size != last_view_size_)) {
    if (backing_store_)
      backing_store_->Clear(SK_ColorWHITE);
    gfx::Size old_view_size = last_view_size_;
    last_view_size_ = params.view_size;
    // Schedule a SizeChanged instead of calling it directly to ensure that
    // the backing store has been updated before the developer attempts to
    // resize to avoid flicker. |size_changed_in_flight_| acts as a form of
    // flow control for SizeChanged events. If the guest's view size is changing
    // rapidly before a SizeChanged event fires, then we avoid scheduling
    // another SizeChanged event. SizeChanged reads the new size from
    // |last_view_size_| so we can be sure that it always fires an event
    // with the last seen view size.
    if (container_ && !size_changed_in_flight_) {
      size_changed_in_flight_ = true;
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&BrowserPlugin::SizeChangedDueToAutoSize,
                     base::Unretained(this),
                     old_view_size));
    }
  }

  // No more work to do since the guest is no longer using a damage buffer.
  if (!UsesDamageBuffer(params))
    return;

  // If we are seeing damage buffers, HW compositing should be turned off.
  EnableCompositing(false);

  // If we are now using a new damage buffer, then that means that the guest
  // has updated its size state in response to a resize request. We change
  // the backing store's size to accomodate the new damage buffer size.
  if (use_new_damage_buffer) {
    int backing_store_width = auto_size_ ? max_width_ : width();
    int backing_store_height = auto_size_ ? max_height_: height();
    backing_store_.reset(
        new BrowserPluginBackingStore(
            gfx::Size(backing_store_width, backing_store_height),
            params.scale_factor));
  }

  // Update the backing store.
  if (!params.scroll_rect.IsEmpty()) {
    backing_store_->ScrollBackingStore(params.scroll_delta,
                                       params.scroll_rect,
                                       params.view_size);
  }
  for (unsigned i = 0; i < params.copy_rects.size(); i++) {
    backing_store_->PaintToBackingStore(params.bitmap_rect,
                                        params.copy_rects,
                                        current_damage_buffer_->memory());
  }
  // Invalidate the container.
  // If the BrowserPlugin is scheduled to be deleted, then container_ will be
  // NULL so we shouldn't attempt to access it.
  if (container_)
    container_->invalidate();
  PopulateAutoSizeParameters(&auto_size_params);
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
      render_view_routing_id_,
      instance_id_,
      auto_size_params,
      resize_guest_params));
}

void BrowserPlugin::SetMaxHeightAttribute(int max_height) {
  if (max_height_ == max_height)
    return;
  max_height_ = max_height;
  if (!auto_size_)
    return;
  UpdateGuestAutoSizeState();
}

void BrowserPlugin::SetMaxWidthAttribute(int max_width) {
  if (max_width_ == max_width)
    return;
  max_width_ = max_width;
  if (!auto_size_)
    return;
  UpdateGuestAutoSizeState();
}

void BrowserPlugin::SetMinHeightAttribute(int min_height) {
  if (min_height_ == min_height)
    return;
  min_height_ = min_height;
  if (!auto_size_)
     return;
  UpdateGuestAutoSizeState();
}

void BrowserPlugin::SetMinWidthAttribute(int min_width) {
  if (min_width_ == min_width)
    return;
  min_width_ = min_width;
  if (!auto_size_)
     return;
  UpdateGuestAutoSizeState();
}

bool BrowserPlugin::InAutoSizeBounds(const gfx::Size& size) const {
  return size.width() <= max_width_ && size.height() <= max_height_;
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
                                          std::string* error_message) {
  if (navigate_src_sent_) {
    *error_message = kErrorAlreadyNavigated;
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
      valid_partition_id_ = false;
      *error_message = kErrorInvalidPartition;
      return false;
    }
    persist_storage_ = true;
  } else {
    persist_storage_ = false;
  }

  valid_partition_id_ = true;
  storage_partition_id_ = input;
  return true;
}

void BrowserPlugin::ParseAttributes(const WebKit::WebPluginParams& params) {
  std::string src;

  // Get the src attribute from the attributes vector
  for (unsigned i = 0; i < params.attributeNames.size(); ++i) {
    std::string attributeName = params.attributeNames[i].utf8();
    if (LowerCaseEqualsASCII(attributeName, kSrc)) {
      src = params.attributeValues[i].utf8();
    } else if (LowerCaseEqualsASCII(attributeName, kPartition)) {
      std::string error;
      SetPartitionAttribute(params.attributeValues[i].utf8(), &error);
    } else if (LowerCaseEqualsASCII(attributeName, kName)) {
      SetNameAttribute(params.attributeValues[i].utf8());
    }
  }

  // Set the 'src' attribute last, as it will set the has_navigated_ flag to
  // true, which prevents changing the 'partition' attribute.
  std::string error;
  SetSrcAttribute(src, &error);
}

float BrowserPlugin::GetDeviceScaleFactor() const {
  if (!render_view_)
    return 1.0f;
  return render_view_->GetWebView()->deviceScaleFactor();
}

void BrowserPlugin::TriggerEvent(const std::string& event_name,
                                 std::map<std::string, base::Value*>* props) {
  if (!container() || !container()->element().document().frame())
    return;
  v8::HandleScope handle_scope;
  std::string json_string;
  if (props) {
    base::DictionaryValue dict;
    for (std::map<std::string, base::Value*>::iterator iter = props->begin(),
             end = props->end(); iter != end; ++iter) {
      dict.Set(iter->first, iter->second);
    }

    JSONStringValueSerializer serializer(&json_string);
    if (!serializer.Serialize(dict))
      return;
  }

  WebKit::WebFrame* frame = container()->element().document().frame();
  WebKit::WebDOMEvent dom_event = frame->document().createEvent("CustomEvent");
  WebKit::WebDOMCustomEvent event = dom_event.to<WebKit::WebDOMCustomEvent>();

  // The events triggered directly from the plugin <object> are internal events
  // whose implementation details can (and likely will) change over time. The
  // wrapper/shim (e.g. <webview> tag) should receive these events, and expose a
  // more appropriate (and stable) event to the consumers as part of the API.
  event.initCustomEvent(
      WebKit::WebString::fromUTF8(GetInternalEventName(event_name.c_str())),
      false, false,
      WebKit::WebSerializedScriptValue::serialize(
          v8::String::New(json_string.c_str(), json_string.size())));
  container()->element().dispatchEvent(event);
}

void BrowserPlugin::Back() {
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_Go(render_view_routing_id_,
                                  instance_id_, -1));
}

void BrowserPlugin::Forward() {
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_Go(render_view_routing_id_,
                                  instance_id_, 1));
}

void BrowserPlugin::Go(int relative_index) {
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_Go(render_view_routing_id_,
                                  instance_id_,
                                  relative_index));
}

void BrowserPlugin::TerminateGuest() {
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_TerminateGuest(render_view_routing_id_,
                                              instance_id_));
}

void BrowserPlugin::Stop() {
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_Stop(render_view_routing_id_,
                                    instance_id_));
}

void BrowserPlugin::Reload() {
  if (!navigate_src_sent_)
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_Reload(render_view_routing_id_,
                                      instance_id_));
}

void BrowserPlugin::UpdateGuestFocusState() {
  if (!navigate_src_sent_)
    return;
  bool should_be_focused = ShouldGuestBeFocused();
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetFocus(
      render_view_routing_id_,
      instance_id_,
      should_be_focused));
}

bool BrowserPlugin::ShouldGuestBeFocused() const {
  bool embedder_focused = false;
  if (render_view_)
    embedder_focused = render_view_->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebKit::WebPluginContainer* BrowserPlugin::container() const {
  return container_;
}

bool BrowserPlugin::initialize(WebPluginContainer* container) {
  container_ = container;
  container_->setWantsWheelEvents(true);
  return true;
}

void BrowserPlugin::EnableCompositing(bool enable) {
  if (compositing_enabled_ == enable)
    return;

  if (enable && !compositing_helper_) {
    compositing_helper_ = new BrowserPluginCompositingHelper(
        container_,
        browser_plugin_manager(),
        render_view_routing_id_);
  }

  compositing_helper_->EnableCompositing(enable);
  compositing_enabled_ = enable;
}

void BrowserPlugin::destroy() {
  // The BrowserPlugin's WebPluginContainer is deleted immediately after this
  // call returns, so let's not keep a reference to it around.
  container_ = NULL;
  if (compositing_helper_)
    compositing_helper_->OnContainerDestroy();
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
          GetSadWebViewBitmap();
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

bool BrowserPlugin::InBounds(const gfx::Point& position) const {
  // Note that even for plugins that are rotated using rotate transformations,
  // we use the the |plugin_rect_| provided by updateGeometry, which means we
  // will be off if |position| is within the plugin rect but does not fall
  // within the actual plugin boundary. Not supporting such edge case is OK
  // since this function should not be used for making security-sensitive
  // decisions.
  // This also does not take overlapping plugins into account.
  bool result = position.x() >= plugin_rect_.x() &&
      position.x() < plugin_rect_.x() + plugin_rect_.width() &&
      position.y() >= plugin_rect_.y() &&
      position.y() < plugin_rect_.y() + plugin_rect_.height();
  return result;
}

gfx::Point BrowserPlugin::ToLocalCoordinates(const gfx::Point& point) const {
  if (container_)
    return container_->windowToLocalPoint(WebKit::WebPoint(point));
  return gfx::Point(point.x() - plugin_rect_.x(), point.y() - plugin_rect_.y());
}

void BrowserPlugin::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  int old_width = width();
  int old_height = height();
  plugin_rect_ = window_rect;
  // In AutoSize mode, guests don't care when the BrowserPlugin container is
  // resized. If |!resize_ack_received_|, then we are still waiting on a
  // previous resize to be ACK'ed and so we don't issue additional resizes
  // until the previous one is ACK'ed.
  if (!navigate_src_sent_ || auto_size_ || !resize_ack_received_ ||
      (old_width == window_rect.width && old_height == window_rect.height)) {
    return;
  }

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(&params, gfx::Size(width(), height()));
  resize_ack_received_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      instance_id_,
      params));
}

void BrowserPlugin::SwapDamageBuffers() {
  current_damage_buffer_.reset(pending_damage_buffer_.release());
  resize_ack_received_ = true;
}

void BrowserPlugin::PopulateResizeGuestParameters(
    BrowserPluginHostMsg_ResizeGuest_Params* params,
    const gfx::Size& view_size) {
  params->view_size = view_size;
  params->scale_factor = GetDeviceScaleFactor();

  // In HW compositing mode, we do not need a damage buffer.
  if (compositing_enabled_)
    return;

  const size_t stride = skia::PlatformCanvasStrideForWidth(view_size.width());
  // Make sure the size of the damage buffer is at least four bytes so that we
  // can fit in a magic word to verify that the memory is shared correctly.
  size_t size =
      std::max(sizeof(unsigned int),
               static_cast<size_t>(view_size.height() *
                                   stride *
                                   GetDeviceScaleFactor() *
                                   GetDeviceScaleFactor()));

  params->damage_buffer_size = size;
  pending_damage_buffer_.reset(
      CreateDamageBuffer(size, &params->damage_buffer_handle));
  if (!pending_damage_buffer_.get())
    NOTREACHED();
  params->damage_buffer_sequence_id = ++damage_buffer_sequence_id_;
}

void BrowserPlugin::GetDamageBufferWithSizeParams(
    BrowserPluginHostMsg_AutoSize_Params* auto_size_params,
    BrowserPluginHostMsg_ResizeGuest_Params* resize_guest_params) {
  PopulateAutoSizeParameters(auto_size_params);
  gfx::Size view_size = auto_size_params->enable ? auto_size_params->max_size :
      gfx::Size(width(), height());
  if (view_size.IsEmpty())
    return;
  resize_ack_received_ = false;
  PopulateResizeGuestParameters(resize_guest_params, view_size);
}

#if defined(OS_POSIX)
base::SharedMemory* BrowserPlugin::CreateDamageBuffer(
    const size_t size,
    base::SharedMemoryHandle* damage_buffer_handle) {
  scoped_ptr<base::SharedMemory> shared_buf(
      content::RenderThread::Get()->HostAllocateSharedMemoryBuffer(
          size).release());

  if (shared_buf.get()) {
    if (shared_buf->Map(size)) {
      // Insert the magic word.
      *static_cast<unsigned int*>(shared_buf->memory()) = 0xdeadbeef;
      shared_buf->ShareToProcess(base::GetCurrentProcessHandle(),
                                 damage_buffer_handle);
      return shared_buf.release();
    }
  }
  NOTREACHED();
  return NULL;
}
#elif defined(OS_WIN)
base::SharedMemory* BrowserPlugin::CreateDamageBuffer(
    const size_t size,
    base::SharedMemoryHandle* damage_buffer_handle) {
  scoped_ptr<base::SharedMemory> shared_buf(new base::SharedMemory());

  if (!shared_buf->CreateAndMapAnonymous(size)) {
    NOTREACHED() << "Buffer allocation failed";
    return NULL;
  }

  // Insert the magic word.
  *static_cast<unsigned int*>(shared_buf->memory()) = 0xdeadbeef;
  if (shared_buf->ShareToProcess(base::GetCurrentProcessHandle(),
                                 damage_buffer_handle))
    return shared_buf.release();
  NOTREACHED();
  return NULL;
}
#endif

void BrowserPlugin::updateFocus(bool focused) {
  if (plugin_focused_ == focused)
    return;

  bool old_guest_focus_state = ShouldGuestBeFocused();
  plugin_focused_ = focused;

  if (ShouldGuestBeFocused() != old_guest_focus_state)
    UpdateGuestFocusState();
}

void BrowserPlugin::updateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!navigate_src_sent_)
    return;

  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetVisibility(
      render_view_routing_id_,
      instance_id_,
      visible));
}

bool BrowserPlugin::acceptsInputEvents() {
  return true;
}

bool BrowserPlugin::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  if (guest_crashed_ || !navigate_src_sent_ ||
      event.type == WebKit::WebInputEvent::ContextMenu)
    return false;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(render_view_routing_id_,
                                                instance_id_,
                                                plugin_rect_,
                                                &event));
  cursor_.GetCursorInfo(&cursor_info);
  return true;
}

bool BrowserPlugin::handleDragStatusUpdate(WebKit::WebDragStatus drag_status,
                                           const WebKit::WebDragData& drag_data,
                                           WebKit::WebDragOperationsMask mask,
                                           const WebKit::WebPoint& position,
                                           const WebKit::WebPoint& screen) {
  if (guest_crashed_ || !navigate_src_sent_)
    return false;
  browser_plugin_manager()->Send(
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
