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
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/sad_plugin.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebCanvas;
using blink::WebPluginContainer;
using blink::WebPoint;
using blink::WebRect;
using blink::WebURL;
using blink::WebVector;

namespace {
typedef std::map<blink::WebPluginContainer*, content::BrowserPlugin*>
    PluginContainerMap;
static base::LazyInstance<PluginContainerMap> g_plugin_container_map =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace content {

// static
BrowserPlugin* BrowserPlugin::GetFromNode(blink::WebNode& node) {
  blink::WebPluginContainer* container = node.pluginContainer();
  if (!container)
    return NULL;

  PluginContainerMap* browser_plugins = g_plugin_container_map.Pointer();
  PluginContainerMap::iterator it = browser_plugins->find(container);
  return it == browser_plugins->end() ? NULL : it->second;
}

BrowserPlugin::BrowserPlugin(RenderViewImpl* render_view,
                             blink::WebFrame* frame,
                             scoped_ptr<BrowserPluginDelegate> delegate)
    : attached_(false),
      attach_pending_(false),
      render_view_(render_view->AsWeakPtr()),
      render_view_routing_id_(render_view->GetRoutingID()),
      container_(NULL),
      last_device_scale_factor_(GetDeviceScaleFactor()),
      sad_guest_(NULL),
      guest_crashed_(false),
      plugin_focused_(false),
      visible_(true),
      mouse_locked_(false),
      browser_plugin_manager_(render_view->GetBrowserPluginManager()),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      contents_opaque_(true),
      delegate_(delegate.Pass()),
      weak_ptr_factory_(this) {
  browser_plugin_instance_id_ = browser_plugin_manager()->GetNextInstanceID();

  if (delegate_)
    delegate_->SetElementInstanceID(browser_plugin_instance_id_);
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
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(message))
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_CopyFromCompositingSurface,
                        OnCopyFromCompositingSurface)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetContentsOpaque, OnSetContentsOpaque)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetMouseLock, OnSetMouseLock)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
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
  element.setAttribute(web_attribute_name,
      blink::WebString::fromUTF8(attribute_value));
}

void BrowserPlugin::Attach() {
  if (ready()) {
    attached_ = false;
    guest_crashed_ = false;
    EnableCompositing(false);
    if (compositing_helper_.get()) {
      compositing_helper_->OnContainerDestroy();
      compositing_helper_ = NULL;
    }
  }

  // TODO(fsamuel): Add support for reattachment.
  BrowserPluginHostMsg_Attach_Params attach_params;
  attach_params.focused = ShouldGuestBeFocused();
  attach_params.visible = visible_;
  attach_params.origin = plugin_rect().origin();
  gfx::Size view_size(width(), height());
  if (!view_size.IsEmpty()) {
    PopulateResizeGuestParameters(view_size,
                                  &attach_params.resize_guest_params);
  }
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
  DCHECK(render_view_);
  render_view_->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::OnAttachACK(int browser_plugin_instance_id) {
  DCHECK(!attached());
  attached_ = true;
  attach_pending_ = false;
}

void BrowserPlugin::OnCompositorFrameSwapped(const IPC::Message& message) {
  BrowserPluginMsg_CompositorFrameSwapped::Param param;
  if (!BrowserPluginMsg_CompositorFrameSwapped::Read(&message, &param))
    return;

  // Note that there is no need to send ACK for this message.
  // If the guest has updated pixels then it is no longer crashed.
  guest_crashed_ = false;

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
  if (!compositing_helper_.get()) {
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

void BrowserPlugin::OnSetContentsOpaque(int browser_plugin_instance_id,
                                        bool opaque) {
  if (contents_opaque_ == opaque)
    return;
  contents_opaque_ = opaque;
  if (compositing_helper_.get())
    compositing_helper_->SetContentsOpaque(opaque);
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

void BrowserPlugin::ShowSadGraphic() {
  // If the BrowserPlugin is scheduled to be deleted, then container_ will be
  // NULL so we shouldn't attempt to access it.
  if (container_)
    container_->invalidate();
}

float BrowserPlugin::GetDeviceScaleFactor() const {
  if (!render_view_)
    return 1.0f;
  return render_view_->GetWebView()->deviceScaleFactor();
}

void BrowserPlugin::UpdateDeviceScaleFactor() {
  if (last_device_scale_factor_ == GetDeviceScaleFactor())
    return;

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(plugin_size(), &params);
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
  if (render_view_)
    embedder_focused = render_view_->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebPluginContainer* BrowserPlugin::container() const {
  return container_;
}

bool BrowserPlugin::initialize(WebPluginContainer* container) {
  if (!container)
    return false;

  container_ = container;
  container_->setWantsWheelEvents(true);

  g_plugin_container_map.Get().insert(std::make_pair(container_, this));

  // This is a way to notify observers of our attributes that this plugin is
  // available in render tree.
  // TODO(lazyboy): This should be done through the delegate instead. Perhaps
  // by firing an event from there.
  UpdateDOMAttribute("internalinstanceid",
                     base::IntToString(browser_plugin_instance_id_));

  browser_plugin_manager()->AddBrowserPlugin(browser_plugin_instance_id_, this);
  return true;
}

void BrowserPlugin::EnableCompositing(bool enable) {
  bool enabled = !!compositing_helper_.get();
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
  compositing_helper_->SetContentsOpaque(contents_opaque_);

  if (!enable) {
    DCHECK(compositing_helper_.get());
    compositing_helper_->OnContainerDestroy();
    compositing_helper_ = NULL;
  }
}

void BrowserPlugin::destroy() {
  if (container_) {
    //container_->clearScriptObjects();

    // The BrowserPlugin's WebPluginContainer is deleted immediately after this
    // call returns, so let's not keep a reference to it around.
    g_plugin_container_map.Get().erase(container_);
  }

  if (compositing_helper_.get())
    compositing_helper_->OnContainerDestroy();
  container_ = NULL;
  // Will be a no-op if the mouse is not currently locked.
  if (render_view_)
    render_view_->mouse_lock_dispatcher()->OnLockTargetDestroyed(this);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
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
    case BrowserPluginMsg_CompositorFrameSwapped::ID:
    case BrowserPluginMsg_CopyFromCompositingSurface::ID:
    case BrowserPluginMsg_GuestGone::ID:
    case BrowserPluginMsg_SetContentsOpaque::ID:
    case BrowserPluginMsg_SetCursor::ID:
    case BrowserPluginMsg_SetMouseLock::ID:
    case BrowserPluginMsg_ShouldAcceptTouchEvents::ID:
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

  if (old_width == window_rect.width && old_height == window_rect.height) {
    // Let the browser know about the updated view rect.
    browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateGeometry(
        render_view_routing_id_, browser_plugin_instance_id_, plugin_rect_));
    return;
  }

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(plugin_size(), &params);
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      browser_plugin_instance_id_,
      params));
}

void BrowserPlugin::PopulateResizeGuestParameters(
    const gfx::Size& view_size,
    BrowserPluginHostMsg_ResizeGuest_Params* params) {
  params->view_size = view_size;
  params->scale_factor = GetDeviceScaleFactor();
  if (last_device_scale_factor_ != params->scale_factor) {
    last_device_scale_factor_ = params->scale_factor;
    params->repaint = true;
  }
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
                                                &event));
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
  if (delegate_)
    delegate_->DidReceiveData(data, data_length);
}

void BrowserPlugin::didFinishLoading() {
  if (delegate_)
    delegate_->DidFinishLoading();
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
