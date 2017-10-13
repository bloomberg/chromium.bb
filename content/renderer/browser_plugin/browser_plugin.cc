// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/switches.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/sad_plugin.h"
#include "third_party/WebKit/public/platform/WebCoalescedInputEvent.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebPluginContainer;
using blink::WebPoint;
using blink::WebRect;
using blink::WebURL;
using blink::WebVector;

namespace {

using PluginContainerMap =
    std::map<blink::WebPluginContainer*, content::BrowserPlugin*>;
static base::LazyInstance<PluginContainerMap>::DestructorAtExit
    g_plugin_container_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

// static
BrowserPlugin* BrowserPlugin::GetFromNode(blink::WebNode& node) {
  blink::WebPluginContainer* container = node.PluginContainer();
  if (!container)
    return nullptr;

  PluginContainerMap* browser_plugins = g_plugin_container_map.Pointer();
  PluginContainerMap::iterator it = browser_plugins->find(container);
  return it == browser_plugins->end() ? nullptr : it->second;
}

BrowserPlugin::BrowserPlugin(
    RenderFrame* render_frame,
    const base::WeakPtr<BrowserPluginDelegate>& delegate)
    : attached_(false),
      render_frame_routing_id_(render_frame->GetRoutingID()),
      container_(nullptr),
      guest_crashed_(false),
      plugin_focused_(false),
      visible_(true),
      mouse_locked_(false),
      ready_(false),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  browser_plugin_instance_id_ =
      BrowserPluginManager::Get()->GetNextInstanceID();

  if (delegate_)
    delegate_->SetElementInstanceID(browser_plugin_instance_id_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  enable_surface_synchronization_ =
      command_line.HasSwitch(switches::kEnableSurfaceSynchronization);
}

BrowserPlugin::~BrowserPlugin() {
  Detach();

  if (delegate_) {
    delegate_->DidDestroyElement();
    delegate_.reset();
  }

  BrowserPluginManager::Get()->RemoveBrowserPlugin(browser_plugin_instance_id_);
}

bool BrowserPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPlugin, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestReady, OnGuestReady)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetMouseLock, OnSetMouseLock)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetTooltipText, OnSetTooltipText)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetChildFrameSurface,
                        OnSetChildFrameSurface)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPlugin::OnSetChildFrameSurface(
    int browser_plugin_instance_id,
    const viz::SurfaceInfo& surface_info,
    const viz::SurfaceSequence& sequence) {
  if (!attached())
    return;

  if (!enable_surface_synchronization_)
    compositing_helper_->SetPrimarySurfaceInfo(surface_info);
  compositing_helper_->SetFallbackSurfaceId(surface_info.id(), sequence);
}

void BrowserPlugin::SendSatisfySequence(const viz::SurfaceSequence& sequence) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SatisfySequence(
      render_frame_routing_id_, browser_plugin_instance_id_, sequence));
}

void BrowserPlugin::UpdateDOMAttribute(const std::string& attribute_name,
                                       const base::string16& attribute_value) {
  if (!Container())
    return;

  blink::WebElement element = Container()->GetElement();
  element.SetAttribute(blink::WebString::FromUTF8(attribute_name),
                       blink::WebString::FromUTF16(attribute_value));
}

void BrowserPlugin::Attach() {
  Detach();

  BrowserPluginHostMsg_Attach_Params attach_params;
  attach_params.focused = ShouldGuestBeFocused();
  attach_params.visible = visible_;
  attach_params.view_rect = view_rect();
  attach_params.is_full_page_plugin = false;
  if (Container()) {
    blink::WebLocalFrame* frame = Container()->GetDocument().GetFrame();
    attach_params.is_full_page_plugin =
        frame->View()->MainFrame()->IsWebLocalFrame() &&
        frame->View()
            ->MainFrame()
            ->ToWebLocalFrame()
            ->GetDocument()
            .IsPluginDocument();
  }
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_Attach(
      render_frame_routing_id_,
      browser_plugin_instance_id_,
      attach_params));

  attached_ = true;

  // Post an update event to the associated accessibility object.
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  if (render_frame && render_frame->render_accessibility() && Container()) {
    blink::WebElement element = Container()->GetElement();
    blink::WebAXObject ax_element = blink::WebAXObject::FromWebNode(element);
    if (!ax_element.IsDetached()) {
      render_frame->render_accessibility()->HandleAXEvent(
          ax_element,
          ui::AX_EVENT_CHILDREN_CHANGED);
    }
  }

  ViewRectsChanged(view_rect());
}

void BrowserPlugin::Detach() {
  if (!attached())
    return;

  attached_ = false;
  guest_crashed_ = false;
  compositing_helper_->OnContainerDestroy();

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Detach(browser_plugin_instance_id_));
}

void BrowserPlugin::DidCommitCompositorFrame() {
}

void BrowserPlugin::OnAdvanceFocus(int browser_plugin_instance_id,
                                   bool reverse) {
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto* render_view = render_frame ? render_frame->GetRenderView() : nullptr;
  if (!render_view)
    return;
  render_view->GetWebView()->AdvanceFocus(reverse);
}

void BrowserPlugin::OnGuestGone(int browser_plugin_instance_id) {
  guest_crashed_ = true;
  compositing_helper_->ChildFrameGone();
}

void BrowserPlugin::OnGuestReady(int browser_plugin_instance_id,
                                 const viz::FrameSinkId& frame_sink_id) {
  guest_crashed_ = false;
  frame_sink_id_ = frame_sink_id;
  if (!view_rect_)
    return;

  gfx::Rect view_rect = *view_rect_;
  view_rect_ = gfx::Rect();
  ViewRectsChanged(view_rect);
}

void BrowserPlugin::OnSetCursor(int browser_plugin_instance_id,
                                const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPlugin::OnSetMouseLock(int browser_plugin_instance_id,
                                   bool enable) {
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto* render_view = static_cast<RenderViewImpl*>(
      render_frame ? render_frame->GetRenderView() : nullptr);
  if (enable) {
    if (mouse_locked_ || !render_view)
      return;
    render_view->mouse_lock_dispatcher()->LockMouse(this);
  } else {
    if (!mouse_locked_) {
      OnLockMouseACK(false);
      return;
    }
    if (!render_view)
      return;
    render_view->mouse_lock_dispatcher()->UnlockMouse(this);
  }
}

void BrowserPlugin::OnSetTooltipText(int instance_id,
                                     const base::string16& tooltip_text) {
  // Show tooltip text by setting the BrowserPlugin's |title| attribute.
  UpdateDOMAttribute("title", tooltip_text);
}

void BrowserPlugin::OnShouldAcceptTouchEvents(int browser_plugin_instance_id,
                                              bool accept) {
  if (Container()) {
    Container()->RequestTouchEventType(
        accept ? WebPluginContainer::kTouchEventRequestTypeRaw
               : WebPluginContainer::kTouchEventRequestTypeNone);
  }
}

void BrowserPlugin::UpdateInternalInstanceId() {
  // This is a way to notify observers of our attributes that this plugin is
  // available in render tree.
  // TODO(lazyboy): This should be done through the delegate instead. Perhaps
  // by firing an event from there.
  UpdateDOMAttribute(
      "internalinstanceid",
      base::UTF8ToUTF16(base::IntToString(browser_plugin_instance_id_)));
}

void BrowserPlugin::ViewRectsChanged(const gfx::Rect& view_rect) {
  bool rect_size_changed =
      !view_rect_ || view_rect_->size() != view_rect.size();
  if (rect_size_changed || !local_surface_id_.is_valid()) {
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
    if (enable_surface_synchronization_ && frame_sink_id_.is_valid()) {
      RenderWidget* render_widget =
          RenderFrameImpl::FromWebFrame(Container()->GetDocument().GetFrame())
              ->GetRenderWidget();
      float device_scale_factor = render_widget->GetOriginalDeviceScaleFactor();
      viz::SurfaceInfo surface_info(
          viz::SurfaceId(frame_sink_id_, local_surface_id_),
          device_scale_factor,
          gfx::ScaleToCeiledSize(view_rect.size(), device_scale_factor));
      compositing_helper_->SetPrimarySurfaceInfo(surface_info);
    }
  }

  view_rect_ = view_rect;

  if (attached()) {
    // Let the browser know about the updated view rect.
    BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UpdateGeometry(
        browser_plugin_instance_id_, *view_rect_, local_surface_id_));
  }

  if (rect_size_changed && delegate_)
    delegate_->DidResizeElement(view_rect_->size());
}

void BrowserPlugin::UpdateGuestFocusState(blink::WebFocusType focus_type) {
  if (!attached())
    return;
  bool should_be_focused = ShouldGuestBeFocused();
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetFocus(
      browser_plugin_instance_id_,
      should_be_focused,
      focus_type));
}

bool BrowserPlugin::ShouldGuestBeFocused() const {
  bool embedder_focused = false;
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto* render_view = static_cast<RenderViewImpl*>(
      render_frame ? render_frame->GetRenderView() : nullptr);
  if (render_view)
    embedder_focused = render_view->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebPluginContainer* BrowserPlugin::Container() const {
  return container_;
}

bool BrowserPlugin::Initialize(WebPluginContainer* container) {
  DCHECK(container);
  DCHECK_EQ(this, container->Plugin());

  container_ = container;
  container_->SetWantsWheelEvents(true);

  g_plugin_container_map.Get().insert(std::make_pair(container_, this));

  BrowserPluginManager::Get()->AddBrowserPlugin(
      browser_plugin_instance_id_, this);

  // Defer attach call so that if there's any pending browser plugin
  // destruction, then it can progress first.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserPlugin::UpdateInternalInstanceId,
                                weak_ptr_factory_.GetWeakPtr()));

  compositing_helper_.reset(ChildFrameCompositingHelper::CreateForBrowserPlugin(
      weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void BrowserPlugin::Destroy() {
  if (container_) {
    // The BrowserPlugin's WebPluginContainer is deleted immediately after this
    // call returns, so let's not keep a reference to it around.
    g_plugin_container_map.Get().erase(container_);
  }

  container_ = nullptr;
  // Will be a no-op if the mouse is not currently locked.
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto* render_view = static_cast<RenderViewImpl*>(
      render_frame ? render_frame->GetRenderView() : nullptr);
  if (render_view)
    render_view->mouse_lock_dispatcher()->OnLockTargetDestroyed(this);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> BrowserPlugin::V8ScriptableObject(v8::Isolate* isolate) {
  if (!delegate_)
    return v8::Local<v8::Object>();

  return delegate_->V8ScriptableObject(isolate);
}

bool BrowserPlugin::SupportsKeyboardFocus() const {
  return visible_;
}

bool BrowserPlugin::SupportsEditCommands() const {
  return true;
}

bool BrowserPlugin::SupportsInputMethod() const {
  return true;
}

bool BrowserPlugin::CanProcessDrag() const {
  return true;
}

// static
bool BrowserPlugin::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  return IPC_MESSAGE_CLASS(message) == BrowserPluginMsgStart;
}

void BrowserPlugin::UpdateGeometry(const WebRect& plugin_rect_in_viewport,
                                   const WebRect& clip_rect,
                                   const WebRect& unobscured_rect,
                                   bool is_visible) {
  // Convert the plugin_rect_in_viewport to window coordinates, which is css.
  WebRect rect_in_css(plugin_rect_in_viewport);

  // We will use the local root's RenderWidget to convert coordinates to Window.
  // If this local root belongs to an OOPIF, on the browser side we will have to
  // consider the displacement of the child frame in root window.
  RenderWidget* render_widget =
      RenderFrameImpl::FromWebFrame(Container()->GetDocument().GetFrame())
          ->GetRenderWidget();
  render_widget->ConvertViewportToWindow(&rect_in_css);
  gfx::Rect view_rect = rect_in_css;

  if (!ready_) {
    if (delegate_)
      delegate_->Ready();
    ready_ = true;
  }

  ViewRectsChanged(view_rect);
}

void BrowserPlugin::UpdateFocus(bool focused, blink::WebFocusType focus_type) {
  plugin_focused_ = focused;
  UpdateGuestFocusState(focus_type);
}

void BrowserPlugin::UpdateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!attached())
    return;

  compositing_helper_->UpdateVisibility(visible);

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetVisibility(
      browser_plugin_instance_id_,
      visible));
}

blink::WebInputEventResult BrowserPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    blink::WebCursorInfo& cursor_info) {
  const blink::WebInputEvent& event = coalesced_event.Event();
  if (guest_crashed_ || !attached())
    return blink::WebInputEventResult::kNotHandled;

  DCHECK(!blink::WebInputEvent::IsTouchEventType(event.GetType()));

  // With direct event routing turned on, BrowserPlugin should almost never
  // see wheel events any more. The two exceptions are (1) scroll bubbling, and
  // (2) synthetic mouse wheels generated by touchpad GesturePinch events on
  // Mac, which always go to the mainframe and thus may hit BrowserPlugin if
  // it's in a top-level embedder. In both cases we should indicate the event
  // as not handled (for GesturePinch on Mac, indicating the event has been
  // handled leads to touchpad pinch not working).
  if (event.GetType() == blink::WebInputEvent::kMouseWheel)
    return blink::WebInputEventResult::kNotHandled;

  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    auto gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    DCHECK(blink::WebInputEvent::kGestureTapDown == event.GetType() ||
           gesture_event.resending_plugin_id == browser_plugin_instance_id_);

    // We shouldn't be forwarding GestureEvents to the Guest anymore. Indicate
    // we handled this only if it's a non-resent event.
    return gesture_event.resending_plugin_id == browser_plugin_instance_id_
               ? blink::WebInputEventResult::kNotHandled
               : blink::WebInputEventResult::kHandledApplication;
  }

  if (event.GetType() == blink::WebInputEvent::kContextMenu)
    return blink::WebInputEventResult::kHandledSuppressed;

  if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()) &&
      !edit_commands_.empty()) {
    BrowserPluginManager::Get()->Send(
        new BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent(
            browser_plugin_instance_id_,
            edit_commands_));
    edit_commands_.clear();
  }

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(browser_plugin_instance_id_,
                                                &event));
  GetWebCursorInfo(cursor_, &cursor_info);

  // Although we forward this event to the guest, we don't report it as consumed
  // since other targets of this event in Blink never get that chance either.
  if (event.GetType() == blink::WebInputEvent::kGestureFlingStart)
    return blink::WebInputEventResult::kNotHandled;

  return blink::WebInputEventResult::kHandledApplication;
}

bool BrowserPlugin::HandleDragStatusUpdate(blink::WebDragStatus drag_status,
                                           const blink::WebDragData& drag_data,
                                           blink::WebDragOperationsMask mask,
                                           const blink::WebPoint& position,
                                           const blink::WebPoint& screen) {
  if (guest_crashed_ || !attached())
    return false;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_DragStatusUpdate(
        browser_plugin_instance_id_,
        drag_status,
        DropDataBuilder::Build(drag_data),
        mask,
        position));
  return true;
}

void BrowserPlugin::DidReceiveResponse(const blink::WebURLResponse& response) {}

void BrowserPlugin::DidReceiveData(const char* data, int data_length) {
  if (delegate_)
    delegate_->PluginDidReceiveData(data, data_length);
}

void BrowserPlugin::DidFinishLoading() {
  if (delegate_)
    delegate_->PluginDidFinishLoading();
}

void BrowserPlugin::DidFailLoading(const blink::WebURLError& error) {}

bool BrowserPlugin::ExecuteEditCommand(const blink::WebString& name) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ExecuteEditCommand(
      browser_plugin_instance_id_, name.Utf8()));

  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::ExecuteEditCommand(const blink::WebString& name,
                                       const blink::WebString& value) {
  edit_commands_.push_back(EditCommand(name.Utf8(), value.Utf8()));
  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::SetComposition(
    const blink::WebString& text,
    const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
    const blink::WebRange& replacementRange,
    int selectionStart,
    int selectionEnd) {
  if (!attached())
    return false;

  BrowserPluginHostMsg_SetComposition_Params params;
  params.text = text.Utf16();
  for (size_t i = 0; i < ime_text_spans.size(); ++i) {
    params.ime_text_spans.push_back(ime_text_spans[i]);
  }

  params.replacement_range =
      replacementRange.IsNull()
          ? gfx::Range::InvalidRange()
          : gfx::Range(static_cast<uint32_t>(replacementRange.StartOffset()),
                       static_cast<uint32_t>(replacementRange.EndOffset()));
  params.selection_start = selectionStart;
  params.selection_end = selectionEnd;

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ImeSetComposition(
      browser_plugin_instance_id_, params));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

bool BrowserPlugin::CommitText(
    const blink::WebString& text,
    const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
    const blink::WebRange& replacementRange,
    int relative_cursor_pos) {
  if (!attached())
    return false;

  std::vector<blink::WebImeTextSpan> std_ime_text_spans;
  for (size_t i = 0; i < ime_text_spans.size(); ++i) {
    std_ime_text_spans.push_back(ime_text_spans[i]);
  }
  gfx::Range replacement_range =
      replacementRange.IsNull()
          ? gfx::Range::InvalidRange()
          : gfx::Range(static_cast<uint32_t>(replacementRange.StartOffset()),
                       static_cast<uint32_t>(replacementRange.EndOffset()));

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ImeCommitText(
      browser_plugin_instance_id_, text.Utf16(), std_ime_text_spans,
      replacement_range, relative_cursor_pos));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

bool BrowserPlugin::FinishComposingText(
    blink::WebInputMethodController::ConfirmCompositionBehavior
        selection_behavior) {
  if (!attached())
    return false;
  bool keep_selection =
      (selection_behavior == blink::WebInputMethodController::kKeepSelection);
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_ImeFinishComposingText(
          browser_plugin_instance_id_, keep_selection));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

void BrowserPlugin::ExtendSelectionAndDelete(int before, int after) {
  if (!attached())
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_ExtendSelectionAndDelete(
          browser_plugin_instance_id_,
          before,
          after));
}

void BrowserPlugin::OnLockMouseACK(bool succeeded) {
  mouse_locked_ = succeeded;
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_LockMouse_ACK(
      browser_plugin_instance_id_,
      succeeded));
}

void BrowserPlugin::OnMouseLockLost() {
  mouse_locked_ = false;
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UnlockMouse_ACK(
      browser_plugin_instance_id_));
}

bool BrowserPlugin::HandleMouseLockedInputEvent(
    const blink::WebMouseEvent& event) {
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(browser_plugin_instance_id_,
                                                &event));
  return true;
}

}  // namespace content
