// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/sad_plugin.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebCanvas;
using blink::WebPluginContainer;
using blink::WebPluginParams;
using blink::WebPoint;
using blink::WebRect;
using blink::WebURL;
using blink::WebVector;

namespace content {

BrowserPlugin::BrowserPlugin(RenderViewImpl* render_view,
                             blink::WebFrame* frame,
                             bool auto_navigate)
    : attached_(false),
      attach_pending_(false),
      render_view_(render_view->AsWeakPtr()),
      render_view_routing_id_(render_view->GetRoutingID()),
      container_(NULL),
      paint_ack_received_(true),
      last_device_scale_factor_(GetDeviceScaleFactor()),
      sad_guest_(NULL),
      guest_crashed_(false),
      content_window_routing_id_(MSG_ROUTING_NONE),
      plugin_focused_(false),
      visible_(true),
      auto_navigate_(auto_navigate),
      mouse_locked_(false),
      browser_plugin_manager_(render_view->GetBrowserPluginManager()),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      weak_ptr_factory_(this) {
}

BrowserPlugin::~BrowserPlugin() {
  browser_plugin_manager()->RemoveBrowserPlugin(browser_plugin_instance_id_);

  if (!ready())
    return;

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_PluginDestroyed(render_view_routing_id_,
                                               browser_plugin_instance_id_));
}

bool BrowserPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPlugin, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_Attach_ACK, OnAttachACK)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_BuffersSwapped, OnBuffersSwapped)
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(message))
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_CopyFromCompositingSurface,
                        OnCopyFromCompositingSurface)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestContentWindowReady,
                        OnGuestContentWindowReady)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetMouseLock, OnSetMouseLock)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPlugin::UpdateDOMAttribute(const std::string& attribute_name,
                                       const std::string& attribute_value) {
  if (!container())
    return;

  blink::WebElement element = container()->element();
  blink::WebString web_attribute_name =
      blink::WebString::fromUTF8(attribute_name);
  if (!HasDOMAttribute(attribute_name) ||
      (std::string(element.getAttribute(web_attribute_name).utf8()) !=
          attribute_value)) {
    element.setAttribute(web_attribute_name,
        blink::WebString::fromUTF8(attribute_value));
  }
}

void BrowserPlugin::RemoveDOMAttribute(const std::string& attribute_name) {
  if (!container())
    return;

  container()->element().removeAttribute(
      blink::WebString::fromUTF8(attribute_name));
}

std::string BrowserPlugin::GetDOMAttributeValue(
    const std::string& attribute_name) const {
  if (!container())
    return std::string();

  return container()->element().getAttribute(
      blink::WebString::fromUTF8(attribute_name)).utf8();
}

bool BrowserPlugin::HasDOMAttribute(const std::string& attribute_name) const {
  if (!container())
    return false;

  return container()->element().hasAttribute(
      blink::WebString::fromUTF8(attribute_name));
}

bool BrowserPlugin::GetAllowTransparencyAttribute() const {
  return HasDOMAttribute(browser_plugin::kAttributeAllowTransparency);
}

void BrowserPlugin::ParseAllowTransparencyAttribute() {
  if (!ready())
    return;

  bool opaque = !GetAllowTransparencyAttribute();

  if (compositing_helper_)
    compositing_helper_->SetContentsOpaque(opaque);

  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetContentsOpaque(
        render_view_routing_id_,
        browser_plugin_instance_id_,
        opaque));
}

void BrowserPlugin::Attach() {
  if (ready()) {
    attached_ = false;
    guest_crashed_ = false;
    EnableCompositing(false);
    if (compositing_helper_) {
      compositing_helper_->OnContainerDestroy();
      compositing_helper_ = NULL;
    }
  }

  // TODO(fsamuel): Add support for reattachment.
  BrowserPluginHostMsg_Attach_Params attach_params;
  attach_params.focused = ShouldGuestBeFocused();
  attach_params.visible = visible_;
  attach_params.opaque = !GetAllowTransparencyAttribute();
  attach_params.origin = plugin_rect().origin();
  GetSizeParams(&attach_params.resize_guest_params, false);

  browser_plugin_manager()->Send(new BrowserPluginHostMsg_Attach(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      attach_params));

  attach_pending_ = true;
}

void BrowserPlugin::DidCommitCompositorFrame() {
  if (compositing_helper_.get())
    compositing_helper_->DidCommitCompositorFrame();
}

void BrowserPlugin::OnAdvanceFocus(int browser_plugin_instance_id,
                                   bool reverse) {
  DCHECK(render_view_.get());
  render_view_->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::OnAttachACK(int browser_plugin_instance_id) {
  DCHECK(!attached());
  attached_ = true;
  attach_pending_ = false;
}

void BrowserPlugin::OnBuffersSwapped(
    int instance_id,
    const FrameMsg_BuffersSwapped_Params& params) {
  EnableCompositing(true);

  compositing_helper_->OnBuffersSwapped(params.size,
                                        params.mailbox,
                                        params.gpu_route_id,
                                        params.gpu_host_id,
                                        GetDeviceScaleFactor());
}

void BrowserPlugin::OnCompositorFrameSwapped(const IPC::Message& message) {
  BrowserPluginMsg_CompositorFrameSwapped::Param param;
  if (!BrowserPluginMsg_CompositorFrameSwapped::Read(&message, &param))
    return;
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  param.b.frame.AssignTo(frame.get());

  EnableCompositing(true);
  compositing_helper_->OnCompositorFrameSwapped(frame.Pass(),
                                                param.b.producing_route_id,
                                                param.b.output_surface_id,
                                                param.b.producing_host_id,
                                                param.b.shared_memory_handle);
}

void BrowserPlugin::OnCopyFromCompositingSurface(int browser_plugin_instance_id,
                                                 int request_id,
                                                 gfx::Rect source_rect,
                                                 gfx::Size dest_size) {
  if (!compositing_helper_) {
    browser_plugin_manager()->Send(
        new BrowserPluginHostMsg_CopyFromCompositingSurfaceAck(
            render_view_routing_id_,
            browser_plugin_instance_id_,
            request_id,
            SkBitmap()));
    return;
  }
  compositing_helper_->CopyFromCompositingSurface(request_id, source_rect,
                                                  dest_size);
}

void BrowserPlugin::OnGuestContentWindowReady(int browser_plugin_instance_id,
                                              int content_window_routing_id) {
  DCHECK(content_window_routing_id != MSG_ROUTING_NONE);
  content_window_routing_id_ = content_window_routing_id;
}

void BrowserPlugin::OnGuestGone(int browser_plugin_instance_id) {
  guest_crashed_ = true;

  // Turn off compositing so we can display the sad graphic. Changes to
  // compositing state will show up at a later time after a layout and commit.
  EnableCompositing(false);

  // Queue up showing the sad graphic to give content embedders an opportunity
  // to fire their listeners and potentially overlay the webview with custom
  // behavior. If the BrowserPlugin is destroyed in the meantime, then the
  // task will not be executed.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserPlugin::ShowSadGraphic,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowserPlugin::OnSetCursor(int browser_plugin_instance_id,
                                const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPlugin::OnSetMouseLock(int browser_plugin_instance_id,
                                   bool enable) {
  if (enable) {
    if (mouse_locked_)
      return;
    render_view_->mouse_lock_dispatcher()->LockMouse(this);
  } else {
    if (!mouse_locked_) {
      OnLockMouseACK(false);
      return;
    }
    render_view_->mouse_lock_dispatcher()->UnlockMouse(this);
  }
}

void BrowserPlugin::OnShouldAcceptTouchEvents(int browser_plugin_instance_id,
                                              bool accept) {
  if (container()) {
    container()->requestTouchEventType(
        accept ? WebPluginContainer::TouchEventRequestTypeRaw
               : WebPluginContainer::TouchEventRequestTypeNone);
  }
}

void BrowserPlugin::OnUpdateRect(
    int browser_plugin_instance_id,
    const BrowserPluginMsg_UpdateRect_Params& params) {
  // Note that there is no need to send ACK for this message.
  // If the guest has updated pixels then it is no longer crashed.
  guest_crashed_ = false;

  // We receive a resize ACK in regular mode, but not in autosize.
  // In Compositing mode, we need to do it here so we can continue sending
  // resize messages when needed.
  if (params.is_resize_ack)
    paint_ack_received_ = true;

  if (params.view_size.width() == width() &&
      params.view_size.height() == height()) {
    return;
  }

  BrowserPluginHostMsg_ResizeGuest_Params resize_params;
  PopulateResizeGuestParameters(&resize_params, plugin_size(), false);
  paint_ack_received_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      resize_params));
}

NPObject* BrowserPlugin::GetContentWindow() const {
  if (content_window_routing_id_ == MSG_ROUTING_NONE)
    return NULL;
  RenderViewImpl* guest_render_view = RenderViewImpl::FromRoutingID(
      content_window_routing_id_);
  if (!guest_render_view)
    return NULL;
  blink::WebFrame* guest_frame = guest_render_view->GetWebView()->mainFrame();
  return guest_frame->windowObject();
}

void BrowserPlugin::ShowSadGraphic() {
  // If the BrowserPlugin is scheduled to be deleted, then container_ will be
  // NULL so we shouldn't attempt to access it.
  if (container_)
    container_->invalidate();
}

float BrowserPlugin::GetDeviceScaleFactor() const {
  if (!render_view_.get())
    return 1.0f;
  return render_view_->GetWebView()->deviceScaleFactor();
}

void BrowserPlugin::UpdateDeviceScaleFactor(float device_scale_factor) {
  if (last_device_scale_factor_ == device_scale_factor || !paint_ack_received_)
    return;

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(&params, plugin_size(), true);
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      params));
}

void BrowserPlugin::UpdateGuestFocusState() {
  if (!ready())
    return;
  bool should_be_focused = ShouldGuestBeFocused();
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetFocus(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      should_be_focused));
}

bool BrowserPlugin::ShouldGuestBeFocused() const {
  bool embedder_focused = false;
  if (render_view_.get())
    embedder_focused = render_view_->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebPluginContainer* BrowserPlugin::container() const {
  return container_;
}

bool BrowserPlugin::initialize(WebPluginContainer* container) {
  if (!container)
    return false;

  // Tell |container| to allow this plugin to use script objects.
  npp_.reset(new NPP_t);
  container->allowScriptObjects();

  bindings_.reset(new BrowserPluginBindings(this));
  container_ = container;
  container_->setWantsWheelEvents(true);

  // This is a way to notify observers of our attributes that this plugin is
  // available in render tree.
  browser_plugin_instance_id_ = browser_plugin_manager()->GetNextInstanceID();
  UpdateDOMAttribute("internalinstanceid",
                     base::IntToString(browser_plugin_instance_id_));

  browser_plugin_manager()->AddBrowserPlugin(browser_plugin_instance_id_, this);
  return true;
}

void BrowserPlugin::EnableCompositing(bool enable) {
  bool enabled = !!compositing_helper_;
  if (enabled == enable)
    return;

  if (enable) {
    DCHECK(!compositing_helper_.get());
    if (!compositing_helper_.get()) {
      compositing_helper_ = ChildFrameCompositingHelper::CreateForBrowserPlugin(
          weak_ptr_factory_.GetWeakPtr());
    }
  }
  compositing_helper_->EnableCompositing(enable);
  compositing_helper_->SetContentsOpaque(!GetAllowTransparencyAttribute());

  if (!enable) {
    DCHECK(compositing_helper_.get());
    compositing_helper_->OnContainerDestroy();
    compositing_helper_ = NULL;
  }
}

void BrowserPlugin::destroy() {
  // If the plugin was initialized then it has a valid |npp_| identifier, and
  // the |container_| must clear references to the plugin's script objects.
  DCHECK(!npp_ || container_);
  if (container_)
    container_->clearScriptObjects();

  if (compositing_helper_.get())
    compositing_helper_->OnContainerDestroy();
  container_ = NULL;
  // Will be a no-op if the mouse is not currently locked.
  if (render_view_.get())
    render_view_->mouse_lock_dispatcher()->OnLockTargetDestroyed(this);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* BrowserPlugin::scriptableObject() {
  if (!bindings_)
    return NULL;

  NPObject* browser_plugin_np_object(bindings_->np_object());
  // The object is expected to be retained before it is returned.
  blink::WebBindings::retainObject(browser_plugin_np_object);
  return browser_plugin_np_object;
}

NPP BrowserPlugin::pluginNPP() {
  return npp_.get();
}

bool BrowserPlugin::supportsKeyboardFocus() const {
  return true;
}

bool BrowserPlugin::supportsEditCommands() const {
  return true;
}

bool BrowserPlugin::supportsInputMethod() const {
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
      PaintSadPlugin(canvas, plugin_rect_, *sad_guest_);
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
}

// static
bool BrowserPlugin::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginMsg_Attach_ACK::ID:
    case BrowserPluginMsg_AdvanceFocus::ID:
    case BrowserPluginMsg_BuffersSwapped::ID:
    case BrowserPluginMsg_CompositorFrameSwapped::ID:
    case BrowserPluginMsg_CopyFromCompositingSurface::ID:
    case BrowserPluginMsg_GuestContentWindowReady::ID:
    case BrowserPluginMsg_GuestGone::ID:
    case BrowserPluginMsg_SetCursor::ID:
    case BrowserPluginMsg_SetMouseLock::ID:
    case BrowserPluginMsg_ShouldAcceptTouchEvents::ID:
    case BrowserPluginMsg_UpdateRect::ID:
      return true;
    default:
      break;
  }
  return false;
}

void BrowserPlugin::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  int old_width = width();
  int old_height = height();
  plugin_rect_ = window_rect;
  if (!attached())
    return;

  // In AutoSize mode, guests don't care when the BrowserPlugin container is
  // resized. If |!paint_ack_received_|, then we are still waiting on a
  // previous resize to be ACK'ed and so we don't issue additional resizes
  // until the previous one is ACK'ed.
  // TODO(mthiesse): Assess the performance of calling GetAutoSizeAttribute() on
  // resize.
  if (!paint_ack_received_ ||
      (old_width == window_rect.width && old_height == window_rect.height)) {
    // Let the browser know about the updated view rect.
    browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateGeometry(
        render_view_routing_id_, browser_plugin_instance_id_, plugin_rect_));
    return;
  }

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(&params, plugin_size(), false);
  paint_ack_received_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      params));
}

void BrowserPlugin::PopulateResizeGuestParameters(
    BrowserPluginHostMsg_ResizeGuest_Params* params,
    const gfx::Size& view_size,
    bool needs_repaint) {
  params->size_changed = true;
  params->view_size = view_size;
  params->repaint = needs_repaint;
  params->scale_factor = GetDeviceScaleFactor();
  if (last_device_scale_factor_ != params->scale_factor) {
    DCHECK(params->repaint);
    last_device_scale_factor_ = params->scale_factor;
  }
}

void BrowserPlugin::GetSizeParams(
    BrowserPluginHostMsg_ResizeGuest_Params* resize_guest_params,
    bool needs_repaint) {
  gfx::Size view_size(width(), height());
  if (view_size.IsEmpty())
    return;
  paint_ack_received_ = false;
  PopulateResizeGuestParameters(resize_guest_params, view_size, needs_repaint);
}

void BrowserPlugin::updateFocus(bool focused) {
  plugin_focused_ = focused;
  UpdateGuestFocusState();
}

void BrowserPlugin::updateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!ready())
    return;

  if (compositing_helper_.get())
    compositing_helper_->UpdateVisibility(visible);

  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetVisibility(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      visible));
}

bool BrowserPlugin::acceptsInputEvents() {
  return true;
}

bool BrowserPlugin::handleInputEvent(const blink::WebInputEvent& event,
                                     blink::WebCursorInfo& cursor_info) {
  if (guest_crashed_ || !ready())
    return false;

  if (event.type == blink::WebInputEvent::ContextMenu)
    return true;

  const blink::WebInputEvent* modified_event = &event;
  scoped_ptr<blink::WebTouchEvent> touch_event;
  if (blink::WebInputEvent::isTouchEventType(event.type)) {
    const blink::WebTouchEvent* orig_touch_event =
        static_cast<const blink::WebTouchEvent*>(&event);

    touch_event.reset(new blink::WebTouchEvent());
    memcpy(touch_event.get(), orig_touch_event, sizeof(blink::WebTouchEvent));

    // TODO(bokan): Blink passes back a WebGestureEvent with a touches,
    // changedTouches, and targetTouches lists; however, it doesn't set
    // the state field on the touches which is what the RenderWidget uses
    // to create a WebCore::TouchEvent. crbug.com/358132 tracks removing
    // these multiple lists from WebTouchEvent since they lead to misuse
    // like this and are functionally unused. In the mean time we'll setup
    // the state field here manually to fix multi-touch BrowserPlugins.
    for (size_t i = 0; i < touch_event->touchesLength; ++i) {
      blink::WebTouchPoint& touch = touch_event->touches[i];
      touch.state = blink::WebTouchPoint::StateStationary;
      for (size_t j = 0; j < touch_event->changedTouchesLength; ++j) {
        blink::WebTouchPoint& changed_touch = touch_event->changedTouches[j];
        if (touch.id == changed_touch.id) {
          touch.state = changed_touch.state;
          break;
        }
      }
    }

    // For End and Cancel, Blink gives BrowserPlugin a list of touches that
    // are down, but the browser process expects a list of all touches. We
    // modify these events here to match these expectations.
    if (event.type == blink::WebInputEvent::TouchEnd ||
        event.type == blink::WebInputEvent::TouchCancel) {
      if (touch_event->changedTouchesLength > 0) {
        memcpy(&touch_event->touches[touch_event->touchesLength],
               &touch_event->changedTouches,
              touch_event->changedTouchesLength * sizeof(blink::WebTouchPoint));
        touch_event->touchesLength += touch_event->changedTouchesLength;
      }
    }
    modified_event = touch_event.get();
  }

  if (blink::WebInputEvent::isKeyboardEventType(event.type) &&
      !edit_commands_.empty()) {
    browser_plugin_manager()->Send(
        new BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent(
            render_view_routing_id_,
            browser_plugin_instance_id_,
            edit_commands_));
    edit_commands_.clear();
  }

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(render_view_routing_id_,
                                                browser_plugin_instance_id_,
                                                plugin_rect_,
                                                modified_event));
  GetWebKitCursorInfo(cursor_, &cursor_info);
  return true;
}

bool BrowserPlugin::handleDragStatusUpdate(blink::WebDragStatus drag_status,
                                           const blink::WebDragData& drag_data,
                                           blink::WebDragOperationsMask mask,
                                           const blink::WebPoint& position,
                                           const blink::WebPoint& screen) {
  if (guest_crashed_ || !ready())
    return false;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_DragStatusUpdate(
        render_view_routing_id_,
        browser_plugin_instance_id_,
        drag_status,
        DropDataBuilder::Build(drag_data),
        mask,
        position));
  return true;
}

void BrowserPlugin::didReceiveResponse(
    const blink::WebURLResponse& response) {
}

void BrowserPlugin::didReceiveData(const char* data, int data_length) {
  if (auto_navigate_) {
    std::string value(data, data_length);
    html_string_ += value;
  }
}

void BrowserPlugin::didFinishLoading() {
  if (auto_navigate_) {
    // TODO(lazyboy): Make |auto_navigate_| stuff work.
    UpdateDOMAttribute(content::browser_plugin::kAttributeSrc, html_string_);
  }
}

void BrowserPlugin::didFailLoading(const blink::WebURLError& error) {
}

void BrowserPlugin::didFinishLoadingFrameRequest(const blink::WebURL& url,
                                                 void* notify_data) {
}

void BrowserPlugin::didFailLoadingFrameRequest(
    const blink::WebURL& url,
    void* notify_data,
    const blink::WebURLError& error) {
}

bool BrowserPlugin::executeEditCommand(const blink::WebString& name) {
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ExecuteEditCommand(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      name.utf8()));

  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::executeEditCommand(const blink::WebString& name,
                                       const blink::WebString& value) {
  edit_commands_.push_back(EditCommand(name.utf8(), value.utf8()));
  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::setComposition(
    const blink::WebString& text,
    const blink::WebVector<blink::WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd) {
  if (!ready())
    return false;
  std::vector<blink::WebCompositionUnderline> std_underlines;
  for (size_t i = 0; i < underlines.size(); ++i) {
    std_underlines.push_back(underlines[i]);
  }
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ImeSetComposition(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      text.utf8(),
      std_underlines,
      selectionStart,
      selectionEnd));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

bool BrowserPlugin::confirmComposition(
    const blink::WebString& text,
    blink::WebWidget::ConfirmCompositionBehavior selectionBehavior) {
  if (!ready())
    return false;
  bool keep_selection = (selectionBehavior == blink::WebWidget::KeepSelection);
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ImeConfirmComposition(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      text.utf8(),
      keep_selection));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

void BrowserPlugin::extendSelectionAndDelete(int before, int after) {
  if (!ready())
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_ExtendSelectionAndDelete(
          render_view_routing_id_,
          browser_plugin_instance_id_,
          before,
          after));
}

void BrowserPlugin::OnLockMouseACK(bool succeeded) {
  mouse_locked_ = succeeded;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_LockMouse_ACK(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      succeeded));
}

void BrowserPlugin::OnMouseLockLost() {
  mouse_locked_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_UnlockMouse_ACK(
      render_view_routing_id_,
      browser_plugin_instance_id_));
}

bool BrowserPlugin::HandleMouseLockedInputEvent(
    const blink::WebMouseEvent& event) {
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(render_view_routing_id_,
                                                browser_plugin_instance_id_,
                                                plugin_rect_,
                                                &event));
  return true;
}

}  // namespace content
