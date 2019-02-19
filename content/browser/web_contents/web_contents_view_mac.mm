// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>

#import "content/browser/web_contents/web_contents_view_mac.h"

#include <string>

#import "base/mac/mac_util.h"
#import "base/mac/scoped_sending_event.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop_current.h"
#import "base/message_loop/message_pump_mac.h"
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_ns_view_bridge.h"
#import "content/browser/web_contents/web_contents_view_cocoa.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/ns_view_bridge_factory_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/web_contents_ns_view_bridge.mojom-shared.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/cocoa/ns_view_ids.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/mac/coordinate_conversion.h"

using blink::WebDragOperation;
using blink::WebDragOperationsMask;

// Ensure that the blink::WebDragOperation enum values stay in sync with
// NSDragOperation constants, since the code below static_casts between 'em.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "enum mismatch: " #a)
STATIC_ASSERT_ENUM(NSDragOperationNone, blink::kWebDragOperationNone);
STATIC_ASSERT_ENUM(NSDragOperationCopy, blink::kWebDragOperationCopy);
STATIC_ASSERT_ENUM(NSDragOperationLink, blink::kWebDragOperationLink);
STATIC_ASSERT_ENUM(NSDragOperationGeneric, blink::kWebDragOperationGeneric);
STATIC_ASSERT_ENUM(NSDragOperationPrivate, blink::kWebDragOperationPrivate);
STATIC_ASSERT_ENUM(NSDragOperationMove, blink::kWebDragOperationMove);
STATIC_ASSERT_ENUM(NSDragOperationDelete, blink::kWebDragOperationDelete);
STATIC_ASSERT_ENUM(NSDragOperationEvery, blink::kWebDragOperationEvery);

namespace content {

namespace {

WebContentsViewMac::RenderWidgetHostViewCreateFunction
    g_create_render_widget_host_view = nullptr;

}  // namespace

// static
void WebContentsViewMac::InstallCreateHookForTests(
    RenderWidgetHostViewCreateFunction create_render_widget_host_view) {
  CHECK_EQ(nullptr, g_create_render_widget_host_view);
  g_create_render_widget_host_view = create_render_widget_host_view;
}

WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewMac* rv = new WebContentsViewMac(web_contents, delegate);
  *render_view_host_delegate_view = rv;
  return rv;
}

WebContentsViewMac::WebContentsViewMac(WebContentsImpl* web_contents,
                                       WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      ns_view_id_(ui::NSViewIds::GetNewId()),
      ns_view_client_binding_(this) {}

WebContentsViewMac::~WebContentsViewMac() {
  if (views_host_)
    views_host_->OnHostableViewDestroying();
  DCHECK(!views_host_);
  ns_view_bridge_local_.reset();
}

WebContentsViewCocoa* WebContentsViewMac::cocoa_view() const {
  return ns_view_bridge_local_ ? ns_view_bridge_local_->cocoa_view() : nil;
}

gfx::NativeView WebContentsViewMac::GetNativeView() const {
  return cocoa_view();
}

gfx::NativeView WebContentsViewMac::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewMac::GetTopLevelNativeWindow() const {
  NSWindow* window = [cocoa_view() window];
  return window ? window : delegate_->GetNativeWindow();
}

void WebContentsViewMac::GetContainerBounds(gfx::Rect* out) const {
  NSWindow* window = [cocoa_view() window];
  NSRect bounds = [cocoa_view() bounds];
  if (window)  {
    // Convert bounds to window coordinate space.
    bounds = [cocoa_view() convertRect:bounds toView:nil];

    // Convert bounds to screen coordinate space.
    bounds = [window convertRectToScreen:bounds];
  }

  *out = gfx::ScreenRectFromNSRect(bounds);
}

void WebContentsViewMac::StartDragging(
    const DropData& drop_data,
    WebDragOperationsMask allowed_operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info,
    RenderWidgetHostImpl* source_rwh) {
  // By allowing nested tasks, the code below also allows Close(),
  // which would deallocate |this|.  The same problem can occur while
  // processing -sendEvent:, so Close() is deferred in that case.
  // Drags from web content do not come via -sendEvent:, this sets the
  // same flag -sendEvent: would.
  base::mac::ScopedSendingEvent sending_event_scoper;

  // The drag invokes a nested event loop, arrange to continue
  // processing events.
  base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
  NSDragOperation mask = static_cast<NSDragOperation>(allowed_operations) &
                         ~NSDragOperationGeneric;
  NSPoint offset = NSPointFromCGPoint(
      gfx::PointAtOffsetFromOrigin(image_offset).ToCGPoint());
  [drag_dest_ setDragStartTrackersForProcess:source_rwh->GetProcess()->GetID()];
  [cocoa_view() startDragWithDropData:drop_data
                            sourceRWH:source_rwh
                    dragOperationMask:mask
                                image:gfx::NSImageFromImageSkia(image)
                               offset:offset];
}

void WebContentsViewMac::SizeContents(const gfx::Size& size) {
  // TODO(brettw | japhet) This is a hack and should be removed.
  // See web_contents_view.h.
  // Note(erikchen): This method has /never/ worked correctly. I've removed the
  // previous implementation.
}

void WebContentsViewMac::Focus() {
  if (delegate())
    delegate()->ResetStoredFocus();

  // Focus the the fullscreen view, if one exists; otherwise, focus the content
  // native view. This ensures that the view currently attached to a NSWindow is
  // being used to query or set first responder state.
  RenderWidgetHostView* rwhv =
      web_contents_->GetFullscreenRenderWidgetHostView();
  if (!rwhv)
    rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return;

  static_cast<RenderWidgetHostViewBase*>(rwhv)->Focus();
}

void WebContentsViewMac::SetInitialFocus() {
  if (delegate())
    delegate()->ResetStoredFocus();

  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar();
  else
    Focus();
}

void WebContentsViewMac::StoreFocus() {
  if (delegate())
    delegate()->StoreFocus();
}

void WebContentsViewMac::RestoreFocus() {
  if (delegate() && delegate()->RestoreFocus())
    return;

  // Fall back to the default focus behavior if we could not restore focus.
  // TODO(shess): If location-bar gets focus by default, this will
  // select-all in the field.  If there was a specific selection in
  // the field when we navigated away from it, we should restore
  // that selection.
  SetInitialFocus();
}

void WebContentsViewMac::FocusThroughTabTraversal(bool reverse) {
  if (delegate())
    delegate()->ResetStoredFocus();

  if (web_contents_->ShowingInterstitialPage()) {
    web_contents_->GetInterstitialPage()->FocusThroughTabTraversal(reverse);
    return;
  }
  content::RenderWidgetHostView* fullscreen_view =
      web_contents_->GetFullscreenRenderWidgetHostView();
  if (fullscreen_view) {
    fullscreen_view->Focus();
    return;
  }
  web_contents_->GetRenderViewHost()->SetInitialFocus(reverse);
}

DropData* WebContentsViewMac::GetDropData() const {
  return [drag_dest_ currentDropData];
}

void WebContentsViewMac::UpdateDragCursor(WebDragOperation operation) {
  [drag_dest_ setCurrentOperation:operation];
}

void WebContentsViewMac::GotFocus(RenderWidgetHostImpl* render_widget_host) {
  web_contents_->NotifyWebContentsFocused(render_widget_host);
}

// This is called when the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewMac::TakeFocus(bool reverse) {
  if (delegate())
    delegate()->ResetStoredFocus();

  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse))
    return;
  if (delegate() && delegate()->TakeFocus(reverse))
    return;
  if (reverse) {
    [[cocoa_view() window] selectPreviousKeyView:cocoa_view()];
  } else {
    [[cocoa_view() window] selectNextKeyView:cocoa_view()];
  }
  if (ns_view_bridge_remote_)
    ns_view_bridge_remote_->TakeFocus(reverse);
}

void WebContentsViewMac::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
  // Allow delegates to handle the context menu operation first.
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->HandleContextMenu(params)) {
    return;
  }

  if (delegate())
    delegate()->ShowContextMenu(render_frame_host, params);
  else
    DLOG(ERROR) << "Cannot show context menus without a delegate.";
}

void WebContentsViewMac::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<MenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  popup_menu_helper_.reset(new PopupMenuHelper(this, render_frame_host));
  popup_menu_helper_->ShowPopupMenu(bounds, item_height, item_font_size,
                                    selected_item, items, right_aligned,
                                    allow_multiple_selection);
  // Note: |this| may be deleted here.
}

void WebContentsViewMac::HidePopupMenu() {
  if (popup_menu_helper_)
    popup_menu_helper_->Hide();
}

void WebContentsViewMac::OnMenuClosed() {
  popup_menu_helper_.reset();
}

gfx::Rect WebContentsViewMac::GetViewBounds() const {
  NSRect window_bounds = [cocoa_view() convertRect:[cocoa_view() bounds]
                                            toView:nil];
  window_bounds.origin = ui::ConvertPointFromWindowToScreen(
      [cocoa_view() window], window_bounds.origin);
  return gfx::ScreenRectFromNSRect(window_bounds);
}

void WebContentsViewMac::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context) {
  ns_view_bridge_local_ =
      std::make_unique<WebContentsNSViewBridge>(ns_view_id_, this);
  [cocoa_view() setClient:this];

  drag_dest_.reset([[WebDragDest alloc] initWithWebContentsImpl:web_contents_]);
  if (delegate_)
    [drag_dest_ setDragDelegate:delegate_->GetDragDestDelegate()];
}

RenderWidgetHostViewBase* WebContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host, bool is_guest_view_hack) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  RenderWidgetHostViewMac* view =
      g_create_render_widget_host_view
          ? g_create_render_widget_host_view(render_widget_host,
                                             is_guest_view_hack)
          : new RenderWidgetHostViewMac(render_widget_host, is_guest_view_hack);
  if (delegate()) {
    base::scoped_nsobject<NSObject<RenderWidgetHostViewMacDelegate>>
        rw_delegate(delegate()->CreateRenderWidgetHostViewDelegate(
            render_widget_host, false));

    view->SetDelegate(rw_delegate.get());
  }

  // Add the RenderWidgetHostView to the ui::Layer heirarchy.
  child_views_.push_back(view->GetWeakPtr());
  if (views_host_) {
    NSViewBridgeFactoryHost* factory_host =
        NSViewBridgeFactoryHost::GetFromHostId(
            views_host_->GetViewsFactoryHostId());

    view->MigrateNSViewBridge(factory_host, ns_view_id_);
    view->SetParentUiLayer(views_host_->GetUiLayer());
  }

  // Fancy layout comes later; for now just make it our size and resize it
  // with us. In case there are other siblings of the content area, we want
  // to make sure the content area is on the bottom so other things draw over
  // it.
  NSView* view_view = view->GetNativeView().GetNativeNSView();
  [view_view setFrame:[cocoa_view() bounds]];
  [view_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  // Add the new view below all other views; this also keeps it below any
  // overlay view installed.
  [cocoa_view() addSubview:view_view positioned:NSWindowBelow relativeTo:nil];
  // For some reason known only to Cocoa, the autorecalculation of the key view
  // loop set on the window doesn't set the next key view when the subview is
  // added. On 10.6 things magically work fine; on 10.5 they fail
  // <http://crbug.com/61493>. Digging into Cocoa key view loop code yielded
  // madness; TODO(avi,rohit): look at this again and figure out what's really
  // going on.
  [cocoa_view() setNextKeyView:view_view];
  return view;
}

RenderWidgetHostViewBase* WebContentsViewMac::CreateViewForChildWidget(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewMac* view =
      new RenderWidgetHostViewMac(render_widget_host, false);
  if (delegate()) {
    base::scoped_nsobject<NSObject<RenderWidgetHostViewMacDelegate>>
        rw_delegate(delegate()->CreateRenderWidgetHostViewDelegate(
            render_widget_host, true));
    view->SetDelegate(rw_delegate.get());
  }
  return view;
}

void WebContentsViewMac::SetPageTitle(const base::string16& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}


void WebContentsViewMac::RenderViewCreated(RenderViewHost* host) {
  // We want updates whenever the intrinsic width of the webpage changes.
  // Put the RenderView into that mode. The preferred width is used for example
  // when the "zoom" button in the browser window is clicked.
  host->EnablePreferredSizeMode();
}

void WebContentsViewMac::RenderViewReady() {}

void WebContentsViewMac::RenderViewHostChanged(RenderViewHost* old_host,
                                               RenderViewHost* new_host) {}

void WebContentsViewMac::SetOverscrollControllerEnabled(bool enabled) {
}

bool WebContentsViewMac::IsEventTracking() const {
  return base::MessagePumpMac::IsHandlingSendEvent();
}

// Arrange to call CloseTab() after we're back to the main event loop.
// The obvious way to do this would be PostNonNestableTask(), but that
// will fire when the event-tracking loop polls for events.  So we
// need to bounce the message via Cocoa, instead.
void WebContentsViewMac::CloseTabAfterEventTracking() {
  [cocoa_view() cancelDeferredClose];
  [cocoa_view() performSelector:@selector(closeTabAfterEvent)
                     withObject:nil
                     afterDelay:0.0];
}

void WebContentsViewMac::CloseTab() {
  web_contents_->Close(web_contents_->GetRenderViewHost());
}

std::list<RenderWidgetHostViewMac*> WebContentsViewMac::GetChildViews() {
  // Remove any child NSViews that have been destroyed.
  std::list<RenderWidgetHostViewMac*> result;
  for (auto iter = child_views_.begin(); iter != child_views_.end();) {
    if (*iter) {
      result.push_back(reinterpret_cast<RenderWidgetHostViewMac*>(iter->get()));
      iter++;
    } else {
      iter = child_views_.erase(iter);
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewMac, mojom::WebContentsNSViewClient:

void WebContentsViewMac::OnMouseEvent(bool motion, bool exited) {
  if (!web_contents_ || !web_contents_->GetDelegate())
    return;

  web_contents_->GetDelegate()->ContentsMouseEvent(web_contents_, motion,
                                                   exited);
}

void WebContentsViewMac::OnBecameFirstResponder(
    mojom::SelectionDirection direction) {
  if (!web_contents_)
    return;
  if (direction == mojom::SelectionDirection::kDirect)
    return;

  web_contents_->FocusThroughTabTraversal(direction ==
                                          mojom::SelectionDirection::kReverse);
}

void WebContentsViewMac::OnWindowVisibilityChanged(
    mojom::Visibility mojo_visibility) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed())
    return;

  // TODO: make content use the mojo type for visibility.
  Visibility visibility = Visibility::VISIBLE;
  switch (mojo_visibility) {
    case mojom::Visibility::kVisible:
      visibility = Visibility::VISIBLE;
      break;
    case mojom::Visibility::kOccluded:
      visibility = Visibility::OCCLUDED;
      break;
    case mojom::Visibility::kHidden:
      visibility = Visibility::HIDDEN;
      break;
  }

  web_contents_->UpdateWebContentsVisibility(visibility);
}

void WebContentsViewMac::SetDropData(const DropData& drop_data) {
  [drag_dest_ setDropData:drop_data];
}

bool WebContentsViewMac::DraggingEntered(mojom::DraggingInfoPtr dragging_info,
                                         uint32_t* out_result) {
  *out_result = [drag_dest_ draggingEntered:dragging_info.get()];
  return true;
}

void WebContentsViewMac::DraggingExited() {
  [drag_dest_ draggingExited];
}

bool WebContentsViewMac::DraggingUpdated(mojom::DraggingInfoPtr dragging_info,
                                         uint32_t* out_result) {
  *out_result = [drag_dest_ draggingUpdated:dragging_info.get()];
  return true;
}

bool WebContentsViewMac::PerformDragOperation(
    mojom::DraggingInfoPtr dragging_info,
    bool* out_result) {
  *out_result = [drag_dest_ performDragOperation:dragging_info.get()];
  return true;
}

void WebContentsViewMac::DraggingEntered(mojom::DraggingInfoPtr dragging_info,
                                         DraggingEnteredCallback callback) {
  uint32_t result = 0;
  DraggingEntered(std::move(dragging_info), &result);
  std::move(callback).Run(result);
}

void WebContentsViewMac::DraggingUpdated(mojom::DraggingInfoPtr dragging_info,
                                         DraggingUpdatedCallback callback) {
  uint32_t result = false;
  DraggingUpdated(std::move(dragging_info), &result);
  std::move(callback).Run(result);
}

void WebContentsViewMac::PerformDragOperation(
    mojom::DraggingInfoPtr dragging_info,
    PerformDragOperationCallback callback) {
  bool result = false;
  PerformDragOperation(std::move(dragging_info), &result);
  std::move(callback).Run(result);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewMac, ViewsHostableView:

void WebContentsViewMac::ViewsHostableAttach(ViewsHostableView::Host* host) {
  views_host_ = host;
  // TODO(https://crbug.com/924955): Using the remote accessibility to set
  // the parent accessibility element here causes crashes, so just set it
  // directly on the in-process WebContentsViewCocoa only.
  std::vector<uint8_t> token;
  [cocoa_view()
      setAccessibilityParentElement:views_host_->GetAccessibilityElement()];

  // Create an NSView in the target process, if one exists.
  uint64_t factory_host_id = views_host_->GetViewsFactoryHostId();
  NSViewBridgeFactoryHost* factory_host =
      NSViewBridgeFactoryHost::GetFromHostId(factory_host_id);
  if (factory_host) {
    mojom::WebContentsNSViewClientAssociatedPtr client;
    ns_view_client_binding_.Bind(mojo::MakeRequest(&client));
    mojom::WebContentsNSViewBridgeAssociatedRequest bridge_request =
        mojo::MakeRequest(&ns_view_bridge_remote_);

    factory_host->GetFactory()->CreateWebContentsNSViewBridge(
        ns_view_id_, client.PassInterface(), std::move(bridge_request));

    ns_view_bridge_remote_->SetParentNSView(views_host_->GetNSViewId(), token);

    // Because this view is being displayed from a remote process, reset the
    // in-process NSView's client pointer, so that the in-process NSView will
    // not call back into |this|.
    [cocoa_view() setClient:nullptr];
  } else if (factory_host_id != NSViewBridgeFactoryHost::kLocalDirectHostId) {
    LOG(ERROR) << "Failed to look up NSViewBridgeFactoryHost!";
  }
  ns_view_bridge_local_->SetParentNSView(views_host_->GetNSViewId(), token);

  for (auto* rwhv_mac : GetChildViews()) {
    rwhv_mac->MigrateNSViewBridge(factory_host, ns_view_id_);
    rwhv_mac->SetParentUiLayer(views_host_->GetUiLayer());
  }
}

void WebContentsViewMac::ViewsHostableDetach() {
  DCHECK(views_host_);
  // Disconnect from the remote bridge, if it exists. This will have the effect
  // of destroying the associated bridge instance with its NSView.
  if (ns_view_bridge_remote_) {
    ns_view_bridge_remote_->SetVisible(false);
    ns_view_bridge_remote_->ResetParentNSView();
    ns_view_client_binding_.Close();
    ns_view_bridge_remote_.reset();
    // Permit the in-process NSView to call back into |this| again.
    [cocoa_view() setClient:this];
  }
  [cocoa_view() setAccessibilityParentElement:nil];
  ns_view_bridge_local_->SetVisible(false);
  ns_view_bridge_local_->ResetParentNSView();
  views_host_ = nullptr;

  for (auto* rwhv_mac : GetChildViews()) {
    rwhv_mac->MigrateNSViewBridge(nullptr, 0);
    rwhv_mac->SetParentUiLayer(nullptr);
  }
}

void WebContentsViewMac::ViewsHostableSetBounds(
    const gfx::Rect& bounds_in_window) {
  // Update both the in-process and out-of-process NSViews' bounds.
  ns_view_bridge_local_->SetBounds(bounds_in_window);
  if (ns_view_bridge_remote_)
    ns_view_bridge_remote_->SetBounds(bounds_in_window);
}

void WebContentsViewMac::ViewsHostableSetVisible(bool visible) {
  // Update both the in-process and out-of-process NSViews' visibility.
  ns_view_bridge_local_->SetVisible(visible);
  if (ns_view_bridge_remote_)
    ns_view_bridge_remote_->SetVisible(visible);
}

void WebContentsViewMac::ViewsHostableMakeFirstResponder() {
  // Only make the true NSView become the first responder.
  if (ns_view_bridge_remote_)
    ns_view_bridge_remote_->MakeFirstResponder();
  else
    ns_view_bridge_local_->MakeFirstResponder();
}

}  // namespace content
