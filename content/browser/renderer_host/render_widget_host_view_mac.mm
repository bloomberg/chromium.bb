// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#import <Carbon/Carbon.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/remote_cocoa/browser/ns_view_ids.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#import "content/app_shim_remote_cocoa/render_widget_host_ns_view_bridge.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#import "content/browser/renderer_host/input/synthetic_gesture_target_mac.h"
#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#import "content/browser/renderer_host/text_input_client_mac.h"
#import "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/cocoa/remote_accessibility_api.h"
#import "ui/base/cocoa/secure_password_input.h"
#include "ui/base/cocoa/text_services_context_menu.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/coordinate_conversion.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebGestureEvent;
using blink::WebTouchEvent;

namespace content {

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorMacClient, public:

SkColor RenderWidgetHostViewMac::BrowserCompositorMacGetGutterColor() const {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (host()->delegate() && host()->delegate()->IsFullscreenForCurrentTab()) {
    return SK_ColorBLACK;
  }
  return last_frame_root_background_color_;
}

void RenderWidgetHostViewMac::OnFrameTokenChanged(uint32_t frame_token) {
  OnFrameTokenChangedForView(frame_token);
}

void RenderWidgetHostViewMac::DestroyCompositorForShutdown() {
  // When RenderWidgetHostViewMac was owned by an NSView, this function was
  // necessary to ensure that the ui::Compositor did not outlive the
  // infrastructure that was needed to support it.
  // https://crbug.com/805726
  Destroy();
}

bool RenderWidgetHostViewMac::OnBrowserCompositorSurfaceIdChanged() {
  return host()->SynchronizeVisualProperties();
}

std::vector<viz::SurfaceId>
RenderWidgetHostViewMac::CollectSurfaceIdsForEviction() {
  return host()->CollectSurfaceIdsForEviction();
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMacNSView, public:

void RenderWidgetHostViewMac::AcceleratedWidgetCALayerParamsUpdated() {
  // Set the background color for the root layer from the frame that just
  // swapped. See RenderWidgetHostViewAura for more details. Note that this is
  // done only after the swap has completed, so that the background is not set
  // before the frame is up.
  SetBackgroundLayerColor(last_frame_root_background_color_);

  // Update the contents that the NSView is displaying.
  const gfx::CALayerParams* ca_layer_params =
      browser_compositor_->GetLastCALayerParams();
  if (ca_layer_params)
    ns_view_->SetCALayerParams(*ca_layer_params);

  // Take this opportunity to update the VSync parameters, if needed.
  if (display_link_) {
    base::TimeTicks timebase;
    base::TimeDelta interval;
    if (display_link_->GetVSyncParameters(&timebase, &interval))
      browser_compositor_->UpdateVSyncParameters(timebase, interval);
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::AccessibilityFocusOverrider::Client:
id RenderWidgetHostViewMac::GetAccessibilityFocusedUIElement() {
  // If content is overlayed with a focused popup from native UI code, this
  // getter must return the current menu item as the focused element, rather
  // than the focus within the content. An example of this occurs with the
  // Autofill feature, where focus is actually still in the textbox although
  // the UX acts as if focus is in the popup.
  gfx::NativeViewAccessible popup_focus_override =
      ui::AXPlatformNode::GetPopupFocusOverride();
  if (popup_focus_override)
    return popup_focus_override;

  BrowserAccessibilityManager* manager =
      host()->GetRootBrowserAccessibilityManager();
  if (manager) {
    BrowserAccessibility* focused_item = manager->GetFocus();
    DCHECK(focused_item);
    if (focused_item) {
      BrowserAccessibilityCocoa* focused_item_cocoa =
          ToBrowserAccessibilityCocoa(focused_item);
      DCHECK(focused_item_cocoa);
      if (focused_item_cocoa)
        return focused_item_cocoa;
    }
  }
  return nil;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget)
    : RenderWidgetHostViewBase(widget),
      page_at_minimum_scale_(true),
      mouse_wheel_phase_handler_(this),
      is_loading_(false),
      popup_parent_host_view_(nullptr),
      popup_child_host_view_(nullptr),
      gesture_provider_(ui::GetGestureProviderConfig(
                            ui::GestureProviderConfigType::CURRENT_PLATFORM),
                        this),
      accessibility_focus_overrider_(this),
      weak_factory_(this) {
  // The NSView is on the other side of |ns_view_|.
  in_process_ns_view_bridge_ =
      std::make_unique<remote_cocoa::RenderWidgetHostNSViewBridge>(this, this);
  ns_view_ = in_process_ns_view_bridge_.get();

  // Guess that the initial screen we will be on is the screen of the current
  // window (since that's the best guess that we have, and is usually right).
  // https://crbug.com/357443
  display_ =
      display::Screen::GetScreen()->GetDisplayNearestWindow([NSApp keyWindow]);

  viz::FrameSinkId frame_sink_id = host()->GetFrameSinkId();

  browser_compositor_.reset(new BrowserCompositorMac(
      this, this, host()->is_hidden(), display_, frame_sink_id));
  DCHECK(![GetInProcessNSView() window]);

  host()->SetView(this);

  // Let the page-level input event router know about our surface ID
  // namespace for surface-based hit testing.
  if (host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

  RenderWidgetHostOwnerDelegate* owner_delegate = host()->owner_delegate();
  if (owner_delegate) {
    // TODO(mostynb): actually use prefs.  Landing this as a separate CL
    // first to rebaseline some unreliable web tests.
    // NOTE: This will not be run for child frame widgets, which do not have
    // an owner delegate and won't get a RenderViewHost here.
    ignore_result(owner_delegate->GetWebkitPreferencesForWidget());
  }

  cursor_manager_.reset(new CursorManager(this));

  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
  if (popup_parent_host_view_) {
    DCHECK(!popup_parent_host_view_->popup_child_host_view_ ||
           popup_parent_host_view_->popup_child_host_view_ == this);
    popup_parent_host_view_->popup_child_host_view_ = nullptr;
  }
  if (popup_child_host_view_) {
    DCHECK(!popup_child_host_view_->popup_parent_host_view_ ||
           popup_child_host_view_->popup_parent_host_view_ == this);
    popup_child_host_view_->popup_parent_host_view_ = nullptr;
  }
}

void RenderWidgetHostViewMac::MigrateNSViewBridge(
    remote_cocoa::mojom::Application* remote_cocoa_application,
    uint64_t parent_ns_view_id) {
  // Destroy the previous remote accessibility element.
  remote_window_accessible_.reset();

  // Disconnect from the previous bridge (this will have the effect of
  // destroying the associated bridge), and close the receiver (to allow it
  // to be re-bound). Note that |in_process_ns_view_bridge_| remains valid.
  remote_ns_view_client_receiver_.reset();
  remote_ns_view_.reset();

  // Enable accessibility focus overriding for remote NSViews.
  accessibility_focus_overrider_.SetAppIsRemote(remote_cocoa_application !=
                                                nullptr);

  // If no host is specified, then use the locally hosted NSView.
  if (!remote_cocoa_application) {
    ns_view_ = in_process_ns_view_bridge_.get();
    return;
  }

  mojo::PendingAssociatedRemote<remote_cocoa::mojom::RenderWidgetHostNSViewHost>
      client = remote_ns_view_client_receiver_.BindNewEndpointAndPassRemote();
  mojo::PendingAssociatedReceiver<remote_cocoa::mojom::RenderWidgetHostNSView>
      view_receiver = remote_ns_view_.BindNewEndpointAndPassReceiver();

  // Cast from PendingAssociatedRemote<mojom::RenderWidgetHostNSViewHost> and
  // mojo::PendingAssociatedReceiver<mojom::RenderWidgetHostNSView> to the
  // public interfaces accepted by the application.
  // TODO(ccameron): Remove the need for this cast.
  // https://crbug.com/888290
  mojo::PendingAssociatedRemote<remote_cocoa::mojom::StubInterface> stub_client(
      client.PassHandle(), 0);
  mojo::PendingAssociatedReceiver<remote_cocoa::mojom::StubInterface>
      stub_bridge_receiver(view_receiver.PassHandle());
  remote_cocoa_application->CreateRenderWidgetHostNSView(
      std::move(stub_client), std::move(stub_bridge_receiver));

  ns_view_ = remote_ns_view_.get();

  // Popup windows will specify an invalid |parent_ns_view_id|, because popups
  // have their own NSWindows (of which they are the content NSView).
  if (parent_ns_view_id != remote_cocoa::kInvalidNSViewId)
    remote_ns_view_->SetParentWebContentsNSView(parent_ns_view_id);
}

void RenderWidgetHostViewMac::SetParentUiLayer(ui::Layer* parent_ui_layer) {
  if (parent_ui_layer && !display_only_using_parent_ui_layer_) {
    // The first time that we display using a parent ui::Layer, permanently
    // switch from drawing using Cocoa to only drawing using ui::Views. Erase
    // the existing content being drawn by Cocoa (which may have been set due
    // to races, e.g, in https://crbug.com/845807). Note that this transition
    // must be done lazily because not all code has been updated to use
    // ui::Views (e.g, content_shell).
    display_only_using_parent_ui_layer_ = true;
    ns_view_->DisableDisplay();
  }
  if (browser_compositor_)
    browser_compositor_->SetParentUiLayer(parent_ui_layer);
}

void RenderWidgetHostViewMac::SetParentAccessibilityElement(
    id parent_accessibility_element) {
  [GetInProcessNSView()
      setAccessibilityParentElement:parent_accessibility_element];
}

RenderWidgetHostViewCocoa* RenderWidgetHostViewMac::GetInProcessNSView() const {
  if (in_process_ns_view_bridge_)
    return in_process_ns_view_bridge_->GetNSView();
  return nullptr;
}

void RenderWidgetHostViewMac::SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate) {
  [GetInProcessNSView() setResponderDelegate:delegate];
}

ui::TextInputType RenderWidgetHostViewMac::GetTextInputType() {
  if (!GetActiveWidget())
    return ui::TEXT_INPUT_TYPE_NONE;
  return text_input_manager_->GetTextInputState()->type;
}

RenderWidgetHostImpl* RenderWidgetHostViewMac::GetActiveWidget() {
  return text_input_manager_ ? text_input_manager_->GetActiveWidget() : nullptr;
}

const TextInputManager::CompositionRangeInfo*
RenderWidgetHostViewMac::GetCompositionRangeInfo() {
  return text_input_manager_ ? text_input_manager_->GetCompositionRangeInfo()
                             : nullptr;
}

const TextInputManager::TextSelection*
RenderWidgetHostViewMac::GetTextSelection() {
  return text_input_manager_ ? text_input_manager_->GetTextSelection(
                                   GetFocusedViewForTextSelection())
                             : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::InitAsChild(gfx::NativeView parent_view) {
  DCHECK_EQ(widget_type_, WidgetType::kFrame);
}

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  DCHECK_EQ(widget_type_, WidgetType::kPopup);

  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewMac*>(parent_host_view);

  RenderWidgetHostViewMac* old_child =
      popup_parent_host_view_->popup_child_host_view_;
  if (old_child) {
    DCHECK(old_child->popup_parent_host_view_ == popup_parent_host_view_);
    old_child->popup_parent_host_view_ = nullptr;
  }
  popup_parent_host_view_->popup_child_host_view_ = this;

  // This path is used by the time/date picker.
  // When FormControlsRefresh is enabled the popup window should use
  // the native shadow.
  bool has_shadow = features::IsFormControlsRefreshEnabled();
  ns_view_->InitAsPopup(pos, has_shadow);
}

void RenderWidgetHostViewMac::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // This path appears never to be reached.
  NOTREACHED();
}

RenderWidgetHostViewBase*
RenderWidgetHostViewMac::GetFocusedViewForTextSelection() {
  // We obtain the TextSelection from focused RWH which is obtained from the
  // frame tree.
  return GetFocusedWidget() ? GetFocusedWidget()->GetView() : nullptr;
}

RenderWidgetHostDelegate*
RenderWidgetHostViewMac::GetFocusedRenderWidgetHostDelegate() {
  if (auto* focused_widget = GetFocusedWidget())
    return focused_widget->delegate();
  return host()->delegate();
}

RenderWidgetHostImpl* RenderWidgetHostViewMac::GetWidgetForKeyboardEvent() {
  DCHECK(in_keyboard_event_);
  return RenderWidgetHostImpl::FromID(keyboard_event_widget_process_id_,
                                      keyboard_event_widget_routing_id_);
}

RenderWidgetHostImpl* RenderWidgetHostViewMac::GetWidgetForIme() {
  if (in_keyboard_event_)
    return GetWidgetForKeyboardEvent();
  return GetActiveWidget();
}

void RenderWidgetHostViewMac::UpdateNSViewAndDisplayProperties() {
  display_link_ = ui::DisplayLinkMac::GetForDisplay(display_.id());
  if (!display_link_) {
    // Note that on some headless systems, the display link will fail to be
    // created, so this should not be a fatal error.
    LOG(ERROR) << "Failed to create display link.";
  }

  // During auto-resize it is the responsibility of the caller to ensure that
  // the NSView and RenderWidgetHostImpl are kept in sync.
  if (host()->auto_resize_enabled())
    return;

  if (host()->delegate())
    host()->delegate()->SendScreenRects();
  else
    host()->SendScreenRects();

  // RenderWidgetHostImpl will query BrowserCompositorMac for the dimensions
  // to send to the renderer, so it is required that BrowserCompositorMac be
  // updated first. Only notify RenderWidgetHostImpl of the update if any
  // properties it will query have changed.
  if (browser_compositor_->UpdateSurfaceFromNSView(
          view_bounds_in_window_dip_.size(), display_)) {
    host()->NotifyScreenInfoChanged();
  }
}

void RenderWidgetHostViewMac::GetScreenInfo(ScreenInfo* screen_info) {
  browser_compositor_->GetRendererScreenInfo(screen_info);
}

void RenderWidgetHostViewMac::Show() {
  is_visible_ = true;
  ns_view_->SetVisible(is_visible_);
  browser_compositor_->SetViewVisible(is_visible_);
  WasUnOccluded();
}

void RenderWidgetHostViewMac::Hide() {
  is_visible_ = false;
  ns_view_->SetVisible(is_visible_);
  browser_compositor_->SetViewVisible(is_visible_);
  WasOccluded();
}

void RenderWidgetHostViewMac::WasUnOccluded() {
  if (!host()->is_hidden())
    return;

  browser_compositor_->SetRenderWidgetHostIsHidden(false);

  DelegatedFrameHost* delegated_frame_host =
      browser_compositor_->GetDelegatedFrameHost();

  bool has_saved_frame =
      browser_compositor_->has_saved_frame_before_state_transition();

  auto tab_switch_start_state = TakeRecordContentToVisibleTimeRequest();

  const bool renderer_should_record_presentation_time = !has_saved_frame;
  host()->WasShown(renderer_should_record_presentation_time
                       ? tab_switch_start_state
                       : base::nullopt);

  if (delegated_frame_host) {
    // If the frame for the renderer is already available, then the
    // tab-switching time is the presentation time for the browser-compositor.
    const bool record_presentation_time = has_saved_frame;
    delegated_frame_host->WasShown(
        browser_compositor_->GetRendererLocalSurfaceIdAllocation()
            .local_surface_id(),
        browser_compositor_->GetRendererSize(),
        record_presentation_time ? tab_switch_start_state : base::nullopt);
  }
}

void RenderWidgetHostViewMac::WasOccluded() {
  if (host()->is_hidden())
    return;

  host()->WasHidden();
  browser_compositor_->SetRenderWidgetHostIsHidden(true);
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  gfx::Rect rect = GetViewBounds();
  rect.set_size(size);
  SetBounds(rect);
}

void RenderWidgetHostViewMac::SetBounds(const gfx::Rect& rect) {
  ns_view_->SetBounds(rect);
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() {
  return GetInProcessNSView();
}

gfx::NativeViewAccessible RenderWidgetHostViewMac::GetNativeViewAccessible() {
  return GetInProcessNSView();
}

void RenderWidgetHostViewMac::Focus() {
  // Ignore redundant calls, as they can cause unending loops of focus-setting.
  // https://crbug.com/998123
  if (is_first_responder_)
    return;
  ns_view_->MakeFirstResponder();
}

bool RenderWidgetHostViewMac::HasFocus() {
  return is_first_responder_;
}

bool RenderWidgetHostViewMac::IsSurfaceAvailableForCopy() {
  return browser_compositor_->GetDelegatedFrameHost()
      ->CanCopyFromCompositingSurface();
}

bool RenderWidgetHostViewMac::IsShowing() {
  return is_visible_;
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() {
  return view_bounds_in_window_dip_ +
         window_frame_in_screen_dip_.OffsetFromOrigin();
}

bool RenderWidgetHostViewMac::IsMouseLocked() {
  return mouse_locked_;
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  GetCursorManager()->UpdateCursor(this, cursor);
}

void RenderWidgetHostViewMac::DisplayCursor(const WebCursor& cursor) {
  ns_view_->DisplayCursor(cursor);
}

CursorManager* RenderWidgetHostViewMac::GetCursorManager() {
  return cursor_manager_.get();
}

void RenderWidgetHostViewMac::OnDidNavigateMainFrameToNewPage() {
  gesture_provider_.ResetDetection();
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // If we ever decide to show the waiting cursor while the page is loading
  // like Chrome does on Windows, call |UpdateCursor()| here.
}

void RenderWidgetHostViewMac::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  if (!did_update_state)
    return;

  const TextInputState* state = text_input_manager->GetTextInputState();
  if (state)
    ns_view_->SetTextInputState(state->type, state->flags);
  else
    ns_view_->SetTextInputState(ui::TEXT_INPUT_TYPE_NONE, 0);

  // |updated_view| is the last view to change its TextInputState which can be
  // used to start/stop monitoring composition info when it has a focused
  // editable text input field.
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(updated_view->GetRenderWidgetHost());

  // We might end up here when |updated_view| has had active TextInputState and
  // then got destroyed. In that case, |updated_view->GetRenderWidgetHost()|
  // returns nullptr.
  if (!widget_host)
    return;

  // Set the monitor state based on the text input focus state.
  const bool has_focus = HasFocus();
  bool need_monitor_composition =
      has_focus && state && state->type != ui::TEXT_INPUT_TYPE_NONE;

  widget_host->RequestCompositionUpdates(false /* immediate_request */,
                                         need_monitor_composition);

  if (has_focus) {
    SetTextInputActive(true);

    // Let AppKit cache the new input context to make IMEs happy.
    // See http://crbug.com/73039.
    [NSApp updateWindows];
  }
}

void RenderWidgetHostViewMac::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  ns_view_->CancelComposition();
}

void RenderWidgetHostViewMac::OnImeCompositionRangeChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  const TextInputManager::CompositionRangeInfo* info =
      GetCompositionRangeInfo();
  if (!info)
    return;
  // The RangeChanged message is only sent with valid values. The current
  // caret position (start == end) will be sent if there is no IME range.
  ns_view_->SetCompositionRangeInfo(info->range);
}

void RenderWidgetHostViewMac::OnSelectionBoundsChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(GetTextInputManager(), text_input_manager);

  // The rest of the code is to support the Mac Zoom feature tracking the
  // text caret; we can skip it if that feature is not currently enabled.
  if (!UAZoomEnabled())
    return;

  RenderWidgetHostViewBase* focused_view = GetFocusedViewForTextSelection();
  if (!focused_view)
    return;

  const TextInputManager::SelectionRegion* region =
      GetTextInputManager()->GetSelectionRegion(focused_view);
  if (!region)
    return;

  // Create a rectangle for the edge of the selection focus, which will be
  // the same as the caret position if the selection is collapsed. That's
  // what we want to try to keep centered on-screen if possible.
  gfx::Rect gfx_caret_rect(region->focus.edge_start_rounded().x(),
                           region->focus.edge_start_rounded().y(), 1,
                           region->focus.GetHeight());
  gfx_caret_rect += view_bounds_in_window_dip_.OffsetFromOrigin();
  gfx_caret_rect += window_frame_in_screen_dip_.OffsetFromOrigin();

  // Note that UAZoomChangeFocus wants unflipped screen coordinates.
  NSRect caret_rect = NSRectFromCGRect(gfx_caret_rect.ToCGRect());
  UAZoomChangeFocus(&caret_rect, &caret_rect, kUAZoomFocusTypeInsertionPoint);
}

void RenderWidgetHostViewMac::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(GetTextInputManager(), text_input_manager);

  const TextInputManager::TextSelection* selection = GetTextSelection();
  if (!selection)
    return;

  ns_view_->SetTextSelection(selection->text(), selection->offset(),
                             selection->range());
}

void RenderWidgetHostViewMac::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);

  if (ShouldRouteEvents()) {
    blink::WebGestureEvent gesture_event(web_gesture);
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &gesture_event, latency_info);
  } else {
    host()->ForwardGestureEventWithLatencyInfo(web_gesture, latency_info);
  }
}

void RenderWidgetHostViewMac::OnRenderFrameMetadataChangedAfterActivation() {
  last_frame_root_background_color_ = host()
                                          ->render_frame_metadata_provider()
                                          ->LastRenderFrameMetadata()
                                          .root_background_color;
  RenderWidgetHostViewBase::OnRenderFrameMetadataChangedAfterActivation();
}

void RenderWidgetHostViewMac::RenderProcessGone() {
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // Unlock the mouse in the NSView's process before destroying our bridge to
  // it.
  if (mouse_locked_) {
    mouse_locked_ = false;
    ns_view_->SetCursorLocked(false);
  }

  // Destroy the local and remote briges to the NSView. Note that the NSView on
  // the other side of |ns_view_| may outlive us due to other retains.
  ns_view_ = nullptr;
  in_process_ns_view_bridge_.reset();
  remote_ns_view_client_receiver_.reset();
  remote_ns_view_.reset();

  // Delete the delegated frame state, which will reach back into
  // host().
  browser_compositor_.reset();

  // Make sure none of our observers send events for us to process after
  // we release host().
  NotifyObserversAboutShutdown();

  if (text_input_manager_)
    text_input_manager_->RemoveObserver(this);

  mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();

  // The call to the base class will set host() to nullptr.
  RenderWidgetHostViewBase::Destroy();

  delete this;
}

void RenderWidgetHostViewMac::SetTooltipText(
    const base::string16& tooltip_text) {
  GetCursorManager()->SetTooltipTextForView(this, tooltip_text);
}

void RenderWidgetHostViewMac::DisplayTooltipText(
    const base::string16& tooltip_text) {
  ns_view_->SetTooltipText(tooltip_text);
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewMac::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      base::IgnoreResult(
          &RenderWidgetHostViewMac::OnDidUpdateVisualPropertiesComplete),
      weak_factory_.GetWeakPtr(), metadata);
  return browser_compositor_->GetScopedRendererSurfaceIdAllocator(
      std::move(allocation_task));
}

void RenderWidgetHostViewMac::DidNavigate() {
  browser_compositor_->DidNavigate();
}

gfx::Size RenderWidgetHostViewMac::GetRequestedRendererSize() {
  return browser_compositor_->GetRendererSize();
}

namespace {

// A helper function for CombineTextNodesAndMakeCallback() below. It would
// ordinarily be a helper lambda in that class method, but it processes a tree
// and needs to be recursive, and that's crazy difficult to do with a lambda.
// TODO(avi): Move this to be a lambda when P0839R0 lands in C++.
void AddTextNodesToVector(const ui::AXNode* node,
                          std::vector<base::string16>* strings) {
  const ui::AXNodeData& node_data = node->data();

  if (node_data.role == ax::mojom::Role::kStaticText) {
    if (node_data.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      strings->emplace_back(
          node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
    }
    return;
  }

  for (const auto* child : node->children())
    AddTextNodesToVector(child, strings);
}

using SpeechCallback = base::OnceCallback<void(const base::string16&)>;
void CombineTextNodesAndMakeCallback(SpeechCallback callback,
                                     const ui::AXTreeUpdate& update) {
  std::vector<base::string16> text_node_contents;
  text_node_contents.reserve(update.nodes.size());

  ui::AXTree tree(update);

  AddTextNodesToVector(tree.root(), &text_node_contents);

  std::move(callback).Run(
      base::JoinString(text_node_contents, base::ASCIIToUTF16("\n")));
}

}  // namespace

void RenderWidgetHostViewMac::GetPageTextForSpeech(SpeechCallback callback) {
  // Note that the WebContents::RequestAXTreeSnapshot() call has a limit on the
  // number of nodes returned. For large pages, this call might hit that limit.
  // This is a reasonable thing. The "Start Speaking" call dates back to the
  // earliest days of the Mac, before accessibility. It was designed to show off
  // the speech capabilities of the Mac, which is fine, but is mostly
  // inapplicable nowadays. Is it useful to have the Mac read megabytes of text
  // with zero control over positioning, with no fast-forward or rewind? What
  // does it even mean to read a Web 2.0 dynamic, AJAXy page aloud from
  // beginning to end?
  //
  // If this is an issue, please file a bug explaining the situation and how the
  // limits of this feature affect you in the real world.

  GetWebContents()->RequestAXTreeSnapshot(
      base::BindOnce(CombineTextNodesAndMakeCallback, std::move(callback)),
      ui::AXMode::kWebContents);
}

void RenderWidgetHostViewMac::SpeakSelection() {
  const TextInputManager::TextSelection* selection = GetTextSelection();
  if (selection && !selection->selected_text().empty()) {
    ui::TextServicesContextMenu::SpeakText(selection->selected_text());
    return;
  }

  // With no selection, speak an approximation of the entire contents of the
  // page.
  GetPageTextForSpeech(base::BindOnce(ui::TextServicesContextMenu::SpeakText));
}

//
// RenderWidgetHostViewCocoa uses the stored selection text,
// which implements NSServicesRequests protocol.
//

void RenderWidgetHostViewMac::SetShowingContextMenu(bool showing) {
  ns_view_->SetShowingContextMenu(showing);
}

uint32_t RenderWidgetHostViewMac::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void RenderWidgetHostViewMac::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  base::WeakPtr<RenderWidgetHostImpl> popup_host;
  base::WeakPtr<DelegatedFrameHost> popup_frame_host;
  if (popup_child_host_view_) {
    popup_host = popup_child_host_view_->host()->GetWeakPtr();
    popup_frame_host = popup_child_host_view_->BrowserCompositor()
                           ->GetDelegatedFrameHost()
                           ->GetWeakPtr();
  }
  RenderWidgetHostViewBase::CopyMainAndPopupFromSurface(
      host()->GetWeakPtr(),
      browser_compositor_->GetDelegatedFrameHost()->GetWeakPtr(), popup_host,
      popup_frame_host, src_subrect, dst_size, display_.device_scale_factor(),
      std::move(callback));
}

void RenderWidgetHostViewMac::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  browser_compositor_->ForceNewSurfaceId();
}

void RenderWidgetHostViewMac::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  browser_compositor_->UpdateSurfaceFromChild(
      host()->auto_resize_enabled(), metadata.device_scale_factor,
      metadata.viewport_size_in_pixels,
      metadata.local_surface_id_allocation.value_or(
          viz::LocalSurfaceIdAllocation()));
}

void RenderWidgetHostViewMac::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewMac* view_mac =
      static_cast<RenderWidgetHostViewMac*>(view);
  ScopedCAActionDisabler disabler;
  base::Optional<SkColor> color = view_mac->GetBackgroundColor();
  if (color)
    SetBackgroundColor(*color);

  // Make the NSView for |this| display the same content as is being displayed
  // in the NSView for |view_mac|.
  const gfx::CALayerParams* ca_layer_params =
      view_mac->browser_compositor_->GetLastCALayerParams();
  if (ca_layer_params)
    ns_view_->SetCALayerParams(*ca_layer_params);
  browser_compositor_->TakeFallbackContentFrom(
      view_mac->browser_compositor_.get());
}

bool RenderWidgetHostViewMac::GetLineBreakIndex(
    const std::vector<gfx::Rect>& bounds,
    const gfx::Range& range,
    size_t* line_break_point) {
  DCHECK(line_break_point);
  if (range.start() >= bounds.size() || range.is_reversed() || range.is_empty())
    return false;

  // We can't check line breaking completely from only rectangle array. Thus we
  // assume the line breaking as the next character's y offset is larger than
  // a threshold. Currently the threshold is determined as minimum y offset plus
  // 75% of maximum height.
  // TODO(nona): Check the threshold is reliable or not.
  // TODO(nona): Bidi support.
  const size_t loop_end_idx =
      std::min(bounds.size(), static_cast<size_t>(range.end()));
  int max_height = 0;
  int min_y_offset = std::numeric_limits<int32_t>::max();
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    max_height = std::max(max_height, bounds[idx].height());
    min_y_offset = std::min(min_y_offset, bounds[idx].y());
  }
  int line_break_threshold = min_y_offset + (max_height * 3 / 4);
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    if (bounds[idx].y() > line_break_threshold) {
      *line_break_point = idx;
      return true;
    }
  }
  return false;
}

gfx::Rect RenderWidgetHostViewMac::GetFirstRectForCompositionRange(
    const gfx::Range& range,
    gfx::Range* actual_range) {
  const TextInputManager::CompositionRangeInfo* composition_info =
      GetCompositionRangeInfo();
  if (!composition_info)
    return gfx::Rect();

  DCHECK(actual_range);
  DCHECK(!composition_info->character_bounds.empty());
  DCHECK(range.start() <= composition_info->character_bounds.size());
  DCHECK(range.end() <= composition_info->character_bounds.size());

  if (range.is_empty()) {
    *actual_range = range;
    if (range.start() == composition_info->character_bounds.size()) {
      return gfx::Rect(
          composition_info->character_bounds[range.start() - 1].right(),
          composition_info->character_bounds[range.start() - 1].y(), 0,
          composition_info->character_bounds[range.start() - 1].height());
    } else {
      return gfx::Rect(
          composition_info->character_bounds[range.start()].x(),
          composition_info->character_bounds[range.start()].y(), 0,
          composition_info->character_bounds[range.start()].height());
    }
  }

  size_t end_idx;
  if (!GetLineBreakIndex(composition_info->character_bounds, range, &end_idx)) {
    end_idx = range.end();
  }
  *actual_range = gfx::Range(range.start(), end_idx);
  gfx::Rect rect = composition_info->character_bounds[range.start()];
  for (size_t i = range.start() + 1; i < end_idx; ++i) {
    rect.Union(composition_info->character_bounds[i]);
  }
  return rect;
}

gfx::Range RenderWidgetHostViewMac::ConvertCharacterRangeToCompositionRange(
    const gfx::Range& request_range) {
  const TextInputManager::CompositionRangeInfo* composition_info =
      GetCompositionRangeInfo();
  if (!composition_info)
    return gfx::Range::InvalidRange();

  if (composition_info->range.is_empty())
    return gfx::Range::InvalidRange();

  if (composition_info->range.is_reversed())
    return gfx::Range::InvalidRange();

  if (request_range.start() < composition_info->range.start() ||
      request_range.start() > composition_info->range.end() ||
      request_range.end() > composition_info->range.end()) {
    return gfx::Range::InvalidRange();
  }

  return gfx::Range(request_range.start() - composition_info->range.start(),
                    request_range.end() - composition_info->range.start());
}

WebContents* RenderWidgetHostViewMac::GetWebContents() {
  return WebContents::FromRenderViewHost(RenderViewHost::From(host()));
}

bool RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(
    const gfx::Range& requested_range,
    gfx::Rect* rect,
    gfx::Range* actual_range) {
  if (!GetTextInputManager())
    return false;

  DCHECK(rect);
  // This exists to make IMEs more responsive, see http://crbug.com/115920
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::GetFirstRectForCharacterRange");

  const TextInputManager::TextSelection* selection = GetTextSelection();
  if (!selection)
    return false;

  // If requested range is same as caret location, we can just return it.
  if (selection->range().is_empty() && requested_range == selection->range()) {
    DCHECK(GetFocusedWidget());
    if (actual_range)
      *actual_range = requested_range;
    *rect = GetTextInputManager()
                ->GetSelectionRegion(GetFocusedWidget()->GetView())
                ->caret_rect;
    return true;
  }

  const TextInputManager::CompositionRangeInfo* composition_info =
      GetCompositionRangeInfo();
  if (!composition_info || composition_info->range.is_empty()) {
    if (!requested_range.IsBoundedBy(selection->range()))
      return false;
    DCHECK(GetFocusedWidget());
    if (actual_range)
      *actual_range = selection->range();
    *rect = GetTextInputManager()
                ->GetSelectionRegion(GetFocusedWidget()->GetView())
                ->first_selection_rect;
    return true;
  }

  const gfx::Range request_range_in_composition =
      ConvertCharacterRangeToCompositionRange(requested_range);
  if (request_range_in_composition == gfx::Range::InvalidRange())
    return false;

  // If firstRectForCharacterRange in WebFrame is failed in renderer,
  // ImeCompositionRangeChanged will be sent with empty vector.
  if (!composition_info || composition_info->character_bounds.empty())
    return false;
  DCHECK_EQ(composition_info->character_bounds.size(),
            composition_info->range.length());

  gfx::Range ui_actual_range;
  *rect = GetFirstRectForCompositionRange(request_range_in_composition,
                                          &ui_actual_range);
  if (actual_range) {
    *actual_range =
        gfx::Range(composition_info->range.start() + ui_actual_range.start(),
                   composition_info->range.start() + ui_actual_range.end());
  }
  return true;
}

void RenderWidgetHostViewMac::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  ns_view_->CancelComposition();

  // If the Mac Zoom feature is enabled, update it with the bounds of the
  // current focused node so that it can ensure that it's scrolled into view.
  // Don't do anything if it's an editable node, as this will be handled by
  // OnSelectionBoundsChanged instead.
  if (UAZoomEnabled() && !is_editable_node) {
    NSRect bounds = NSRectFromCGRect(node_bounds_in_screen.ToCGRect());
    UAZoomChangeFocus(&bounds, NULL, kUAZoomFocusTypeOther);
  }
}

void RenderWidgetHostViewMac::ResetFallbackToFirstNavigationSurface() {
  browser_compositor_->GetDelegatedFrameHost()
      ->ResetFallbackToFirstNavigationSurface();
}

bool RenderWidgetHostViewMac::RequestRepaintForTesting() {
  return browser_compositor_->ForceNewSurfaceId();
}

void RenderWidgetHostViewMac::TransformPointToRootSurface(gfx::PointF* point) {
  browser_compositor_->TransformPointToRootSurface(point);
}

gfx::Rect RenderWidgetHostViewMac::GetBoundsInRootWindow() {
  return window_frame_in_screen_dip_;
}

blink::mojom::PointerLockResult RenderWidgetHostViewMac::LockMouse(
    bool request_unadjusted_movement) {
  if (mouse_locked_)
    return blink::mojom::PointerLockResult::kSuccess;

  if (request_unadjusted_movement) {
    // TODO(crbug/998688): implement pointerlock unadjusted movement on mac.
    NOTIMPLEMENTED();
    return blink::mojom::PointerLockResult::kUnsupportedOptions;
  }

  mouse_locked_ = true;

  // Lock position of mouse cursor and hide it.
  ns_view_->SetCursorLocked(true);

  // Clear the tooltip window.
  ns_view_->SetTooltipText(base::string16());

  return blink::mojom::PointerLockResult::kSuccess;
}

blink::mojom::PointerLockResult RenderWidgetHostViewMac::ChangeMouseLock(
    bool request_unadjusted_movement) {
  // Unadjusted movement is not supported on Mac. Which means that
  // |mouse_locked_unadjusted_movement_| must not be set. Therefore,
  // |request_unadjusted_movement| must be true so this request will always
  // fail with kUnsupportedOptions.
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnsupportedOptions;
}

void RenderWidgetHostViewMac::UnlockMouse() {
  if (!mouse_locked_)
    return;
  mouse_locked_ = false;
  ns_view_->SetCursorLocked(false);

  if (host())
    host()->LostMouseLock();
}

bool RenderWidgetHostViewMac::LockKeyboard(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes) {
  base::Optional<std::vector<uint32_t>> uint_dom_codes;
  if (dom_codes) {
    uint_dom_codes.emplace();
    for (const auto& dom_code : *dom_codes)
      uint_dom_codes->push_back(static_cast<uint32_t>(dom_code));
  }
  is_keyboard_locked_ = true;
  ns_view_->LockKeyboard(uint_dom_codes);
  return true;
}

void RenderWidgetHostViewMac::UnlockKeyboard() {
  if (!is_keyboard_locked_)
    return;

  is_keyboard_locked_ = false;
  ns_view_->UnlockKeyboard();
}

bool RenderWidgetHostViewMac::IsKeyboardLocked() {
  return is_keyboard_locked_;
}

base::flat_map<std::string, std::string>
RenderWidgetHostViewMac::GetKeyboardLayoutMap() {
  return ui::GenerateDomKeyboardLayoutMap();
}

void RenderWidgetHostViewMac::GestureEventAck(
    const WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  ForwardTouchpadZoomEventIfNecessary(event, ack_result);

  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  bool consumed = ack_result == blink::mojom::InputEventResultState::kConsumed;
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
    case WebInputEvent::Type::kGestureScrollUpdate:
    case WebInputEvent::Type::kGestureScrollEnd:
      [GetInProcessNSView() processedGestureScrollEvent:event
                                               consumed:consumed];
      return;
    default:
      break;
  }
  mouse_wheel_phase_handler_.GestureEventAck(event, ack_result);
}

void RenderWidgetHostViewMac::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  gesture_provider_.OnTouchEventAck(
      touch.event.unique_touch_event_id, event_consumed,
      InputEventResultStateIsSetNonBlocking(ack_result));
  if (touch.event.touch_start_or_first_touch_move && event_consumed &&
      host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()
        ->delegate()
        ->GetInputEventRouter()
        ->OnHandledTouchStartOrFirstTouchMove(
            touch.event.unique_touch_event_id);
  }
}

void RenderWidgetHostViewMac::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  [GetInProcessNSView() processedOverscroll:params];
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewMac::CreateSyntheticGestureTarget() {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetMac(host, GetInProcessNSView()));
}

const viz::LocalSurfaceIdAllocation&
RenderWidgetHostViewMac::GetLocalSurfaceIdAllocation() const {
  return browser_compositor_->GetRendererLocalSurfaceIdAllocation();
}

const viz::FrameSinkId& RenderWidgetHostViewMac::GetFrameSinkId() const {
  return browser_compositor_->GetDelegatedFrameHost()->frame_sink_id();
}

bool RenderWidgetHostViewMac::ShouldRouteEvents() const {
  // Event routing requires a valid frame sink (that is, that we be connected to
  // a ui::Compositor), which is not guaranteed to be the case.
  // https://crbug.com/844095
  if (!browser_compositor_->GetRootFrameSinkId().is_valid())
    return false;

  return host()->delegate() && host()->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewMac::SendTouchpadZoomEvent(
    const WebGestureEvent* event) {
  DCHECK(event->IsTouchpadZoomEvent());
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, event, ui::LatencyInfo(ui::SourceEventType::TOUCHPAD));
    return;
  }
  host()->ForwardGestureEvent(*event);
}

void RenderWidgetHostViewMac::InjectTouchEvent(
    const WebTouchEvent& event,
    const ui::LatencyInfo& latency_info) {
  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(MotionEventWeb(event));
  if (!result.succeeded)
    return;

  if (ShouldRouteEvents()) {
    WebTouchEvent touch_event(event);
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    host()->ForwardTouchEventWithLatencyInfo(event, latency_info);
  }
}

bool RenderWidgetHostViewMac::HasFallbackSurface() const {
  return browser_compositor_->GetDelegatedFrameHost()->HasFallbackSurface();
}

bool RenderWidgetHostViewMac::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return target_view->TransformPointToLocalCoordSpace(
      point, GetCurrentSurfaceId(), transformed_point);
}

viz::FrameSinkId RenderWidgetHostViewMac::GetRootFrameSinkId() {
  return browser_compositor_->GetRootFrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewMac::GetCurrentSurfaceId() const {
  // |browser_compositor_| could be null if this method is called during its
  // destruction.
  if (!browser_compositor_)
    return viz::SurfaceId();
  return browser_compositor_->GetDelegatedFrameHost()->GetCurrentSurfaceId();
}

bool RenderWidgetHostViewMac::Send(IPC::Message* message) {
  if (host())
    return host()->Send(message);
  delete message;
  return false;
}

void RenderWidgetHostViewMac::ShutdownHost() {
  weak_factory_.InvalidateWeakPtrs();
  host()->ShutdownAndDestroyWidget(true);
  // Do not touch any members at this point, |this| has been deleted.
}

void RenderWidgetHostViewMac::SetActive(bool active) {
  if (host()) {
    host()->SetActive(active);
    if (active) {
      if (HasFocus())
        host()->Focus();
    } else {
      host()->Blur();
    }
  }
  if (HasFocus())
    SetTextInputActive(active);
  if (!active)
    UnlockMouse();
}

void RenderWidgetHostViewMac::ShowDefinitionForSelection() {
  // This will round-trip to the NSView to determine the selection range.
  ns_view_->ShowDictionaryOverlayForSelection();
}

void RenderWidgetHostViewMac::UpdateBackgroundColor() {
  // This is called by the embedding code prior to the first frame appearing,
  // to set a reasonable color to show before the web content generates its
  // first frame. This will be overridden by the web contents.
  DCHECK(RenderWidgetHostViewBase::GetBackgroundColor());
  SkColor color = *RenderWidgetHostViewBase::GetBackgroundColor();
  SetBackgroundLayerColor(color);
  browser_compositor_->SetBackgroundColor(color);
}

base::Optional<SkColor> RenderWidgetHostViewMac::GetBackgroundColor() {
  // This is used to specify a color to temporarily show while waiting for web
  // content. This should never return transparent, since that will cause bugs
  // where views are initialized as having a transparent background
  // inappropriately.
  // https://crbug.com/735407
  base::Optional<SkColor> color =
      RenderWidgetHostViewBase::GetBackgroundColor();
  return (color && *color == SK_ColorTRANSPARENT) ? SK_ColorWHITE : color;
}

void RenderWidgetHostViewMac::SetBackgroundLayerColor(SkColor color) {
  if (color == background_layer_color_)
    return;
  background_layer_color_ = color;
  ns_view_->SetBackgroundColor(color);
}

BrowserAccessibilityManager*
RenderWidgetHostViewMac::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate,
    bool for_root_frame) {
  return new BrowserAccessibilityManagerMac(
      BrowserAccessibilityManagerMac::GetEmptyDocument(), delegate);
}

gfx::NativeViewAccessible
RenderWidgetHostViewMac::AccessibilityGetNativeViewAccessible() {
  return GetInProcessNSView();
}

gfx::NativeViewAccessible
RenderWidgetHostViewMac::AccessibilityGetNativeViewAccessibleForWindow() {
  if (remote_window_accessible_)
    return remote_window_accessible_.get();
  return [GetInProcessNSView() window];
}

void RenderWidgetHostViewMac::SetTextInputActive(bool active) {
  const bool should_enable_password_input =
      active && GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD;
  if (should_enable_password_input)
    password_input_enabler_.reset(new ui::ScopedPasswordInputEnabler());
  else
    password_input_enabler_.reset();
}

MouseWheelPhaseHandler* RenderWidgetHostViewMac::GetMouseWheelPhaseHandler() {
  return &mouse_wheel_phase_handler_;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostNSViewHostHelper and mojom::RenderWidgetHostNSViewHost
// implementation:

id RenderWidgetHostViewMac::GetRootBrowserAccessibilityElement() {
  if (auto* manager = host()->GetRootBrowserAccessibilityManager())
    return ToBrowserAccessibilityCocoa(manager->GetRoot());
  return nil;
}

id RenderWidgetHostViewMac::GetFocusedBrowserAccessibilityElement() {
  return GetAccessibilityFocusedUIElement();
}

void RenderWidgetHostViewMac::SetAccessibilityWindow(NSWindow* window) {
  // When running in-process, just use the NSView's NSWindow as its own
  // accessibility element.
  remote_window_accessible_.reset();
}

bool RenderWidgetHostViewMac::SyncIsWidgetForMainFrame(
    bool* is_for_main_frame) {
  *is_for_main_frame = !!host()->owner_delegate();
  return true;
}

void RenderWidgetHostViewMac::SyncIsWidgetForMainFrame(
    SyncIsWidgetForMainFrameCallback callback) {
  bool is_for_main_frame;
  SyncIsWidgetForMainFrame(&is_for_main_frame);
  std::move(callback).Run(is_for_main_frame);
}

void RenderWidgetHostViewMac::RequestShutdown() {
  if (!weak_factory_.HasWeakPtrs()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&RenderWidgetHostViewMac::ShutdownHost,
                                  weak_factory_.GetWeakPtr()));
  }
}

void RenderWidgetHostViewMac::OnFirstResponderChanged(bool is_first_responder) {
  if (is_first_responder_ == is_first_responder)
    return;
  is_first_responder_ = is_first_responder;
  accessibility_focus_overrider_.SetViewIsFirstResponder(is_first_responder_);
  if (is_first_responder_) {
    host()->GotFocus();
    SetTextInputActive(true);
  } else {
    SetTextInputActive(false);
    host()->LostFocus();
  }
}

void RenderWidgetHostViewMac::OnWindowIsKeyChanged(bool is_key) {
  if (is_window_key_ == is_key)
    return;
  is_window_key_ = is_key;
  accessibility_focus_overrider_.SetWindowIsKey(is_window_key_);
  if (is_first_responder_)
    SetActive(is_key);
}

void RenderWidgetHostViewMac::OnBoundsInWindowChanged(
    const gfx::Rect& view_bounds_in_window_dip,
    bool attached_to_window) {
  bool view_size_changed =
      view_bounds_in_window_dip_.size() != view_bounds_in_window_dip.size();

  if (attached_to_window) {
    view_bounds_in_window_dip_ = view_bounds_in_window_dip;
  } else {
    // If not attached to a window, do not update the bounds origin (since it is
    // meaningless, and the last value is the best guess at the next meaningful
    // value).
    view_bounds_in_window_dip_.set_size(view_bounds_in_window_dip.size());
  }

  if (view_size_changed)
    UpdateNSViewAndDisplayProperties();
}

void RenderWidgetHostViewMac::OnWindowFrameInScreenChanged(
    const gfx::Rect& window_frame_in_screen_dip) {
  if (window_frame_in_screen_dip_ == window_frame_in_screen_dip)
    return;

  window_frame_in_screen_dip_ = window_frame_in_screen_dip;
  if (host()->delegate())
    host()->delegate()->SendScreenRects();
  else
    host()->SendScreenRects();
}

void RenderWidgetHostViewMac::OnDisplayChanged(
    const display::Display& display) {
  display_ = display;
  UpdateNSViewAndDisplayProperties();
}

void RenderWidgetHostViewMac::BeginKeyboardEvent() {
  DCHECK(!in_keyboard_event_);
  in_keyboard_event_ = true;
  RenderWidgetHostImpl* widget_host = host();
  if (widget_host && widget_host->delegate()) {
    widget_host =
        widget_host->delegate()->GetFocusedRenderWidgetHost(widget_host);
  }
  if (widget_host) {
    keyboard_event_widget_process_id_ = widget_host->GetProcess()->GetID();
    keyboard_event_widget_routing_id_ = widget_host->GetRoutingID();
  }
}

void RenderWidgetHostViewMac::EndKeyboardEvent() {
  in_keyboard_event_ = false;
  keyboard_event_widget_process_id_ = 0;
  keyboard_event_widget_routing_id_ = 0;
}

void RenderWidgetHostViewMac::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency_info) {
  if (auto* widget_host = GetWidgetForKeyboardEvent()) {
    widget_host->ForwardKeyboardEventWithLatencyInfo(key_event, latency_info);
  }
}

void RenderWidgetHostViewMac::ForwardKeyboardEventWithCommands(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency_info,
    std::vector<blink::mojom::EditCommandPtr> commands) {
  if (auto* widget_host = GetWidgetForKeyboardEvent()) {
    widget_host->ForwardKeyboardEventWithCommands(key_event, latency_info,
                                                  std::move(commands));
  }
}

void RenderWidgetHostViewMac::RouteOrProcessMouseEvent(
    const blink::WebMouseEvent& const_web_event) {
  blink::WebMouseEvent web_event = const_web_event;
  ui::LatencyInfo latency_info(ui::SourceEventType::OTHER);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteMouseEvent(this, &web_event,
                                                               latency_info);
  } else {
    ProcessMouseEvent(web_event, latency_info);
  }
}

void RenderWidgetHostViewMac::RouteOrProcessTouchEvent(
    const blink::WebTouchEvent& const_web_event) {
  blink::WebTouchEvent web_event = const_web_event;
  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(MotionEventWeb(web_event));
  if (!result.succeeded)
    return;

  ui::LatencyInfo latency_info(ui::SourceEventType::OTHER);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(this, &web_event,
                                                               latency_info);
  } else {
    ProcessTouchEvent(web_event, latency_info);
  }
}

void RenderWidgetHostViewMac::RouteOrProcessWheelEvent(
    const blink::WebMouseWheelEvent& const_web_event) {
  blink::WebMouseWheelEvent web_event = const_web_event;
  ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
      web_event, ShouldRouteEvents());
  if (web_event.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    // A wheel end event is scheduled and will get dispatched if momentum
    // phase doesn't start in 100ms. Don't sent the wheel end event
    // immediately.
    return;
  }
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        this, &web_event, latency_info);
  } else {
    ProcessMouseWheelEvent(web_event, latency_info);
  }
}

void RenderWidgetHostViewMac::ForwardMouseEvent(
    const blink::WebMouseEvent& web_event) {
  if (host())
    host()->ForwardMouseEvent(web_event);

  if (web_event.GetType() == WebInputEvent::Type::kMouseLeave)
    ns_view_->SetTooltipText(base::string16());
}

void RenderWidgetHostViewMac::ForwardWheelEvent(
    const blink::WebMouseWheelEvent& const_web_event) {
  blink::WebMouseWheelEvent web_event = const_web_event;
  mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(web_event,
                                                                 false);
}

void RenderWidgetHostViewMac::GestureBegin(blink::WebGestureEvent begin_event,
                                           bool is_synthetically_injected) {
  gesture_begin_event_ = std::make_unique<WebGestureEvent>(begin_event);

  // If the page is at the minimum zoom level, require a threshold be reached
  // before the pinch has an effect. Synthetic pinches are not subject to this
  // threshold.
  // TODO(crbug.com/1038683): |page_at_minimum_scale_| is always true, should it
  // be removed or correctly set based on RenderFrameMetadata?
  if (page_at_minimum_scale_) {
    pinch_has_reached_zoom_threshold_ = is_synthetically_injected;
    pinch_unused_amount_ = 1;
  }
}

void RenderWidgetHostViewMac::GestureUpdate(
    blink::WebGestureEvent update_event) {
  // If, due to nesting of multiple gestures (e.g, from multiple touch
  // devices), the beginning of the gesture has been lost, skip the remainder
  // of the gesture.
  if (!gesture_begin_event_)
    return;

  if (!pinch_has_reached_zoom_threshold_) {
    pinch_unused_amount_ *= update_event.data.pinch_update.scale;
    if (pinch_unused_amount_ < 0.667 || pinch_unused_amount_ > 1.5)
      pinch_has_reached_zoom_threshold_ = true;
  }

  // Send a GesturePinchBegin event if none has been sent yet.
  if (!gesture_begin_pinch_sent_) {
    // Before starting a pinch sequence, send the pending wheel end event to
    // finish scrolling.
    mouse_wheel_phase_handler_.DispatchPendingWheelEndEvent();
    WebGestureEvent begin_event(*gesture_begin_event_);
    begin_event.SetType(WebInputEvent::Type::kGesturePinchBegin);
    begin_event.SetSourceDevice(blink::WebGestureDevice::kTouchpad);
    begin_event.SetNeedsWheelEvent(true);
    SendTouchpadZoomEvent(&begin_event);
    gesture_begin_pinch_sent_ = YES;
  }

  // Send a GesturePinchUpdate event.
  update_event.data.pinch_update.zoom_disabled =
      !pinch_has_reached_zoom_threshold_;
  SendTouchpadZoomEvent(&update_event);
}

void RenderWidgetHostViewMac::GestureEnd(blink::WebGestureEvent end_event) {
  gesture_begin_event_.reset();
  if (gesture_begin_pinch_sent_) {
    SendTouchpadZoomEvent(&end_event);
    gesture_begin_pinch_sent_ = false;
  }
}

void RenderWidgetHostViewMac::SmartMagnify(
    const blink::WebGestureEvent& smart_magnify_event) {
  SendTouchpadZoomEvent(&smart_magnify_event);
}

void RenderWidgetHostViewMac::ImeSetComposition(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  if (auto* widget_host = GetWidgetForIme()) {
    widget_host->ImeSetComposition(text, ime_text_spans, replacement_range,
                                   selection_start, selection_end);
  }
}

void RenderWidgetHostViewMac::ImeCommitText(
    const base::string16& text,
    const gfx::Range& replacement_range) {
  if (auto* widget_host = GetWidgetForIme()) {
    widget_host->ImeCommitText(text, std::vector<ui::ImeTextSpan>(),
                               replacement_range, 0);
  }
}

void RenderWidgetHostViewMac::ImeFinishComposingText() {
  if (auto* widget_host = GetWidgetForIme()) {
    widget_host->ImeFinishComposingText(false);
  }
}

void RenderWidgetHostViewMac::ImeCancelCompositionFromCocoa() {
  if (auto* widget_host = GetWidgetForIme()) {
    widget_host->ImeCancelComposition();
  }
}

void RenderWidgetHostViewMac::LookUpDictionaryOverlayFromRange(
    const gfx::Range& range) {
  content::RenderWidgetHostViewBase* focused_view =
      GetFocusedViewForTextSelection();
  if (!focused_view)
    return;

  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(focused_view->GetRenderWidgetHost());
  if (!widget_host)
    return;

  int32_t target_widget_process_id = widget_host->GetProcess()->GetID();
  int32_t target_widget_routing_id = widget_host->GetRoutingID();
  TextInputClientMac::GetInstance()->GetStringFromRange(
      widget_host, range,
      base::BindOnce(&RenderWidgetHostViewMac::OnGotStringForDictionaryOverlay,
                     weak_factory_.GetWeakPtr(), target_widget_process_id,
                     target_widget_routing_id));
}

void RenderWidgetHostViewMac::LookUpDictionaryOverlayAtPoint(
    const gfx::PointF& root_point) {
  if (!host() || !host()->delegate() ||
      !host()->delegate()->GetInputEventRouter())
    return;

  gfx::PointF transformed_point;
  RenderWidgetHostImpl* widget_host =
      host()->delegate()->GetInputEventRouter()->GetRenderWidgetHostAtPoint(
          this, root_point, &transformed_point);
  if (!widget_host)
    return;

  // For popups, do not support QuickLook.
  if (popup_parent_host_view_)
    return;

  int32_t target_widget_process_id = widget_host->GetProcess()->GetID();
  int32_t target_widget_routing_id = widget_host->GetRoutingID();
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      widget_host, gfx::ToFlooredPoint(transformed_point),
      base::BindOnce(&RenderWidgetHostViewMac::OnGotStringForDictionaryOverlay,
                     weak_factory_.GetWeakPtr(), target_widget_process_id,
                     target_widget_routing_id));
}

bool RenderWidgetHostViewMac::SyncGetCharacterIndexAtPoint(
    const gfx::PointF& root_point,
    uint32_t* index) {
  *index = UINT32_MAX;

  if (!host() || !host()->delegate() ||
      !host()->delegate()->GetInputEventRouter())
    return true;

  gfx::PointF transformed_point;
  RenderWidgetHostImpl* widget_host =
      host()->delegate()->GetInputEventRouter()->GetRenderWidgetHostAtPoint(
          this, root_point, &transformed_point);
  if (!widget_host)
    return true;

  *index = TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
      widget_host, gfx::ToFlooredPoint(transformed_point));
  return true;
}

void RenderWidgetHostViewMac::SyncGetCharacterIndexAtPoint(
    const gfx::PointF& root_point,
    SyncGetCharacterIndexAtPointCallback callback) {
  uint32_t index;
  SyncGetCharacterIndexAtPoint(root_point, &index);
  std::move(callback).Run(index);
}

bool RenderWidgetHostViewMac::SyncGetFirstRectForRange(
    const gfx::Range& requested_range,
    const gfx::Rect& in_rect,
    const gfx::Range& in_actual_range,
    gfx::Rect* rect,
    gfx::Range* actual_range,
    bool* success) {
  *rect = in_rect;
  *actual_range = in_actual_range;
  if (!GetFocusedWidget()) {
    *success = false;
    return true;
  }
  *success = true;
  if (!GetCachedFirstRectForCharacterRange(requested_range, rect,
                                           actual_range)) {
    // https://crbug.com/121917
    base::ScopedAllowBlocking allow_wait;
    *rect = TextInputClientMac::GetInstance()->GetFirstRectForRange(
        GetFocusedWidget(), requested_range);
    // TODO(thakis): Pipe |actualRange| through TextInputClientMac machinery.
    *actual_range = requested_range;
  }
  return true;
}

void RenderWidgetHostViewMac::SyncGetFirstRectForRange(
    const gfx::Range& requested_range,
    const gfx::Rect& rect,
    const gfx::Range& actual_range,
    SyncGetFirstRectForRangeCallback callback) {
  gfx::Rect out_rect;
  gfx::Range out_actual_range;
  bool success;
  SyncGetFirstRectForRange(requested_range, rect, actual_range, &out_rect,
                           &out_actual_range, &success);
  std::move(callback).Run(out_rect, out_actual_range, success);
}

void RenderWidgetHostViewMac::ExecuteEditCommand(const std::string& command) {
  if (host()->delegate()) {
    host()->delegate()->ExecuteEditCommand(command, base::nullopt);
  }
}

void RenderWidgetHostViewMac::Undo() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->Undo();
  }
}

void RenderWidgetHostViewMac::Redo() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->Redo();
  }
}

void RenderWidgetHostViewMac::Cut() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->Cut();
  }
}

void RenderWidgetHostViewMac::Copy() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->Copy();
  }
}

void RenderWidgetHostViewMac::CopyToFindPboard() {
  WebContents* web_contents = GetWebContents();
  if (web_contents)
    web_contents->CopyToFindPboard();
}

void RenderWidgetHostViewMac::Paste() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->Paste();
  }
}

void RenderWidgetHostViewMac::PasteAndMatchStyle() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->PasteAndMatchStyle();
  }
}

void RenderWidgetHostViewMac::SelectAll() {
  if (auto* delegate = GetFocusedRenderWidgetHostDelegate()) {
    delegate->SelectAll();
  }
}

bool RenderWidgetHostViewMac::SyncIsSpeaking(bool* is_speaking) {
  *is_speaking = ui::TextServicesContextMenu::IsSpeaking();
  return true;
}

void RenderWidgetHostViewMac::SyncIsSpeaking(SyncIsSpeakingCallback callback) {
  bool is_speaking;
  SyncIsSpeaking(&is_speaking);
  std::move(callback).Run(is_speaking);
}

void RenderWidgetHostViewMac::StartSpeaking() {
  RenderWidgetHostView* target = this;
  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    content::BrowserPluginGuestManager* guest_manager =
        web_contents->GetBrowserContext()->GetGuestManager();
    if (guest_manager) {
      content::WebContents* guest =
          guest_manager->GetFullPageGuest(web_contents);
      if (guest) {
        target = guest->GetRenderWidgetHostView();
      }
    }
  }
  target->SpeakSelection();
}

void RenderWidgetHostViewMac::StopSpeaking() {
  ui::TextServicesContextMenu::StopSpeaking();
}

void RenderWidgetHostViewMac::SetRemoteAccessibilityWindowToken(
    const std::vector<uint8_t>& window_token) {
  if (window_token.empty()) {
    remote_window_accessible_.reset();
  } else {
    remote_window_accessible_ =
        ui::RemoteAccessibility::GetRemoteElementFromToken(window_token);
  }
}

///////////////////////////////////////////////////////////////////////////////
// mojom::RenderWidgetHostNSViewHost functions that translate events and
// forward them to the RenderWidgetHostNSViewHostHelper implementation:

void RenderWidgetHostViewMac::ForwardKeyboardEventWithCommands(
    std::unique_ptr<InputEvent> input_event,
    const std::vector<uint8_t>& native_event_data,
    bool skip_in_browser,
    std::vector<blink::mojom::EditCommandPtr> edit_commands) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsKeyboardEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-KeyboardEventType event.";
    return;
  }
  const blink::WebKeyboardEvent& keyboard_event =
      static_cast<const blink::WebKeyboardEvent&>(*input_event->web_event);
  NativeWebKeyboardEvent native_event(keyboard_event, nil);
  native_event.skip_in_browser = skip_in_browser;
  // The NSEvent constructed from the InputEvent sent over mojo is not even
  // close to the original NSEvent, resulting in all sorts of bugs. Use the
  // native event serialization to reconstruct the NSEvent.
  // https://crbug.com/919167,943197,964052
  [native_event.os_event release];
  native_event.os_event = [ui::EventFromData(native_event_data) retain];
  ForwardKeyboardEventWithCommands(native_event, input_event->latency_info,
                                   std::move(edit_commands));
}

void RenderWidgetHostViewMac::RouteOrProcessMouseEvent(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsMouseEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-MouseEventType event.";
    return;
  }
  const blink::WebMouseEvent& mouse_event =
      static_cast<const blink::WebMouseEvent&>(*input_event->web_event);
  RouteOrProcessMouseEvent(mouse_event);
}

void RenderWidgetHostViewMac::RouteOrProcessTouchEvent(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsTouchEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-TouchEventType event.";
    return;
  }
  const blink::WebTouchEvent& touch_event =
      static_cast<const blink::WebTouchEvent&>(*input_event->web_event);
  RouteOrProcessTouchEvent(touch_event);
}

void RenderWidgetHostViewMac::RouteOrProcessWheelEvent(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      input_event->web_event->GetType() !=
          blink::WebInputEvent::Type::kMouseWheel) {
    DLOG(ERROR) << "Absent or non-MouseWheel event.";
    return;
  }
  const blink::WebMouseWheelEvent& wheel_event =
      static_cast<const blink::WebMouseWheelEvent&>(*input_event->web_event);
  RouteOrProcessWheelEvent(wheel_event);
}

void RenderWidgetHostViewMac::ForwardMouseEvent(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsMouseEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-MouseEventType event.";
    return;
  }
  const blink::WebMouseEvent& mouse_event =
      static_cast<const blink::WebMouseEvent&>(*input_event->web_event);
  ForwardMouseEvent(mouse_event);
}

void RenderWidgetHostViewMac::ForwardWheelEvent(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      input_event->web_event->GetType() !=
          blink::WebInputEvent::Type::kMouseWheel) {
    DLOG(ERROR) << "Absent or non-MouseWheel event.";
    return;
  }
  const blink::WebMouseWheelEvent& wheel_event =
      static_cast<const blink::WebMouseWheelEvent&>(*input_event->web_event);
  ForwardWheelEvent(wheel_event);
}

void RenderWidgetHostViewMac::GestureBegin(
    std::unique_ptr<InputEvent> input_event,
    bool is_synthetically_injected) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsGestureEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-GestureEventType event.";
    return;
  }
  blink::WebGestureEvent gesture_event =
      *static_cast<const blink::WebGestureEvent*>(input_event->web_event.get());
  // Strip the gesture type, because it is not known.
  gesture_event.SetType(blink::WebInputEvent::Type::kUndefined);
  GestureBegin(gesture_event, is_synthetically_injected);
}

void RenderWidgetHostViewMac::GestureUpdate(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsGestureEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-GestureEventType event.";
    return;
  }
  const blink::WebGestureEvent& gesture_event =
      static_cast<const blink::WebGestureEvent&>(*input_event->web_event);
  GestureUpdate(gesture_event);
}

void RenderWidgetHostViewMac::GestureEnd(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsGestureEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-GestureEventType event.";
    return;
  }
  blink::WebGestureEvent gesture_event =
      *static_cast<const blink::WebGestureEvent*>(input_event->web_event.get());
  GestureEnd(gesture_event);
}

void RenderWidgetHostViewMac::SmartMagnify(
    std::unique_ptr<InputEvent> input_event) {
  if (!input_event || !input_event->web_event ||
      !blink::WebInputEvent::IsGestureEventType(
          input_event->web_event->GetType())) {
    DLOG(ERROR) << "Absent or non-GestureEventType event.";
    return;
  }
  const blink::WebGestureEvent& gesture_event =
      static_cast<const blink::WebGestureEvent&>(*input_event->web_event);
  SmartMagnify(gesture_event);
}

void RenderWidgetHostViewMac::OnGotStringForDictionaryOverlay(
    int32_t target_widget_process_id,
    int32_t target_widget_routing_id,
    const mac::AttributedStringCoder::EncodedString& encoded_string,
    gfx::Point baseline_point) {
  if (encoded_string.string().empty()) {
    // The PDF plugin does not support getting the attributed string at point.
    // Until it does, use NSPerformService(), which opens Dictionary.app.
    // TODO(shuchen): Support GetStringAtPoint() & GetStringFromRange() for PDF.
    // https://crbug.com/152438
    // This often just opens a blank dictionary, not the definition of |string|.
    // https://crbug.com/830047
    // This path will be taken, inappropriately, when a lookup gesture was
    // performed at a location that doesn't have text, but some text is
    // selected.
    // https://crbug.com/830906
    if (auto* selection = GetTextSelection()) {
      const base::string16& selected_text = selection->selected_text();
      NSString* ns_selected_text = base::SysUTF16ToNSString(selected_text);
      if ([ns_selected_text length] == 0)
        return;
      scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;
      if ([pasteboard->get() writeObjects:@[ ns_selected_text ]]) {
        NSPerformService(@"Look Up in Dictionary", pasteboard->get());
      }
    }
  } else {
    // By the time we get here |widget_host| might have been destroyed.
    // https://crbug.com/737032
    auto* widget_host = content::RenderWidgetHost::FromID(
        target_widget_process_id, target_widget_routing_id);
    if (widget_host) {
      if (auto* rwhv = widget_host->GetView())
        baseline_point = rwhv->TransformPointToRootCoordSpace(baseline_point);
    }
    ns_view_->ShowDictionaryOverlay(encoded_string, baseline_point);
  }
}

Class GetRenderWidgetHostViewCocoaClassForTesting() {
  return [RenderWidgetHostViewCocoa class];
}

}  // namespace content
