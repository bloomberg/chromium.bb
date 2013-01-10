// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>

#import "content/browser/web_contents/web_contents_view_mac.h"

#include <string>

#import "base/mac/scoped_sending_event.h"
#include "base/message_loop.h"
#import "base/message_pump_mac.h"
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#import "content/browser/web_contents/web_drag_source_mac.h"
#include "content/common/view_messages.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/clipboard/custom_data_helper.h"
#import "ui/base/cocoa/focus_tracker.h"
#include "ui/base/dragdrop/cocoa_dnd_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using content::PopupMenuHelper;
using content::RenderViewHostFactory;
using content::RenderWidgetHostView;
using content::RenderWidgetHostViewMac;
using content::WebContents;
using content::WebContentsImpl;
using content::WebContentsViewMac;

// Ensure that the WebKit::WebDragOperation enum values stay in sync with
// NSDragOperation constants, since the code below static_casts between 'em.
#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(int(NS##name) == int(WebKit::Web##name), enum_mismatch_##name)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

@interface WebContentsViewCocoa (Private)
- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w;
- (void)registerDragTypes;
- (void)setCurrentDragOperation:(NSDragOperation)operation;
- (WebDropData*)dropData;
- (void)startDragWithDropData:(const WebDropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset;
- (void)cancelDeferredClose;
- (void)clearWebContentsView;
- (void)closeTabAfterEvent;
- (void)viewDidBecomeFirstResponder:(NSNotification*)notification;
@end

namespace content {
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
      allow_overlapping_views_(false) {
}

WebContentsViewMac::~WebContentsViewMac() {
  // This handles the case where a renderer close call was deferred
  // while the user was operating a UI control which resulted in a
  // close.  In that case, the Cocoa view outlives the
  // WebContentsViewMac instance due to Cocoa retain count.
  [cocoa_view_ cancelDeferredClose];
  [cocoa_view_ clearWebContentsView];
}

void WebContentsViewMac::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context) {
  WebContentsViewCocoa* view =
      [[WebContentsViewCocoa alloc] initWithWebContentsViewMac:this];
  cocoa_view_.reset(view);
}

RenderWidgetHostView* WebContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  RenderWidgetHostViewMac* view = static_cast<RenderWidgetHostViewMac*>(
      RenderWidgetHostView::CreateViewForWidget(render_widget_host));
  if (delegate()) {
    NSObject<RenderWidgetHostViewMacDelegate>* rw_delegate =
        delegate()->CreateRenderWidgetHostViewDelegate(render_widget_host);
    view->SetDelegate(rw_delegate);
  }
  view->SetAllowOverlappingViews(allow_overlapping_views_);

  // Fancy layout comes later; for now just make it our size and resize it
  // with us. In case there are other siblings of the content area, we want
  // to make sure the content area is on the bottom so other things draw over
  // it.
  NSView* view_view = view->GetNativeView();
  [view_view setFrame:[cocoa_view_.get() bounds]];
  [view_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  // Add the new view below all other views; this also keeps it below any
  // overlay view installed.
  [cocoa_view_.get() addSubview:view_view
                     positioned:NSWindowBelow
                     relativeTo:nil];
  // For some reason known only to Cocoa, the autorecalculation of the key view
  // loop set on the window doesn't set the next key view when the subview is
  // added. On 10.6 things magically work fine; on 10.5 they fail
  // <http://crbug.com/61493>. Digging into Cocoa key view loop code yielded
  // madness; TODO(avi,rohit): look at this again and figure out what's really
  // going on.
  [cocoa_view_.get() setNextKeyView:view_view];
  return view;
}

RenderWidgetHostView* WebContentsViewMac::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

gfx::NativeView WebContentsViewMac::GetNativeView() const {
  return cocoa_view_.get();
}

gfx::NativeView WebContentsViewMac::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv)
    return NULL;
  return rwhv->GetNativeView();
}

gfx::NativeWindow WebContentsViewMac::GetTopLevelNativeWindow() const {
  return [cocoa_view_.get() window];
}

void WebContentsViewMac::GetContainerBounds(gfx::Rect* out) const {
  // Convert bounds to window coordinate space.
  NSRect bounds =
      [cocoa_view_.get() convertRect:[cocoa_view_.get() bounds] toView:nil];

  // Convert bounds to screen coordinate space.
  NSWindow* window = [cocoa_view_.get() window];
  bounds.origin = [window convertBaseToScreen:bounds.origin];

  // Flip y to account for screen flip.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  bounds.origin.y = [screen frame].size.height - bounds.origin.y
      - bounds.size.height;
  *out = gfx::Rect(NSRectToCGRect(bounds));
}

void WebContentsViewMac::StartDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask allowed_operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  // By allowing nested tasks, the code below also allows Close(),
  // which would deallocate |this|.  The same problem can occur while
  // processing -sendEvent:, so Close() is deferred in that case.
  // Drags from web content do not come via -sendEvent:, this sets the
  // same flag -sendEvent: would.
  base::mac::ScopedSendingEvent sending_event_scoper;

  // The drag invokes a nested event loop, arrange to continue
  // processing events.
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
  NSDragOperation mask = static_cast<NSDragOperation>(allowed_operations);
  NSPoint offset = NSPointFromCGPoint(
      gfx::PointAtOffsetFromOrigin(image_offset).ToCGPoint());
  [cocoa_view_ startDragWithDropData:drop_data
                   dragOperationMask:mask
                               image:gfx::NSImageFromImageSkia(image)
                              offset:offset];
}

void WebContentsViewMac::RenderViewCreated(RenderViewHost* host) {
  // We want updates whenever the intrinsic width of the webpage changes.
  // Put the RenderView into that mode. The preferred width is used for example
  // when the "zoom" button in the browser window is clicked.
  host->EnablePreferredSizeMode();
}

void WebContentsViewMac::SetPageTitle(const string16& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void WebContentsViewMac::OnTabCrashed(base::TerminationStatus /* status */,
                                      int /* error_code */) {
}

void WebContentsViewMac::SizeContents(const gfx::Size& size) {
  // TODO(brettw | japhet) This is a hack and should be removed.
  // See web_contents_view.h.
  gfx::Rect rect(gfx::Point(), size);
  WebContentsViewCocoa* view = cocoa_view_.get();

  NSPoint origin = [view frame].origin;
  NSRect frame = [view flipRectToNSRect:rect];
  frame.origin = NSMakePoint(NSMinX(frame) + origin.x,
                             NSMinY(frame) + origin.y);
  [view setFrame:frame];
}

void WebContentsViewMac::Focus() {
  [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
  [[cocoa_view_.get() window] makeKeyAndOrderFront:GetContentNativeView()];
}

void WebContentsViewMac::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
}

void WebContentsViewMac::StoreFocus() {
  // We're explicitly being asked to store focus, so don't worry if there's
  // already a view saved.
  focus_tracker_.reset(
      [[FocusTracker alloc] initWithWindow:[cocoa_view_ window]]);
}

void WebContentsViewMac::RestoreFocus() {
  // TODO(avi): Could we be restoring a view that's no longer in the key view
  // chain?
  if (!(focus_tracker_.get() &&
        [focus_tracker_ restoreFocusInWindow:[cocoa_view_ window]])) {
    // Fall back to the default focus behavior if we could not restore focus.
    // TODO(shess): If location-bar gets focus by default, this will
    // select-all in the field.  If there was a specific selection in
    // the field when we navigated away from it, we should restore
    // that selection.
    SetInitialFocus();
  }

  focus_tracker_.reset(nil);
}

WebDropData* WebContentsViewMac::GetDropData() const {
  return [cocoa_view_ dropData];
}

void WebContentsViewMac::UpdateDragCursor(WebDragOperation operation) {
  [cocoa_view_ setCurrentDragOperation: operation];
}

void WebContentsViewMac::GotFocus() {
  // This is only used in the views FocusManager stuff but it bleeds through
  // all subclasses. http://crbug.com/21875
}

// This is called when the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void WebContentsViewMac::TakeFocus(bool reverse) {
  if (reverse) {
    [[cocoa_view_ window] selectPreviousKeyView:cocoa_view_.get()];
  } else {
    [[cocoa_view_ window] selectNextKeyView:cocoa_view_.get()];
  }
}

void WebContentsViewMac::ShowContextMenu(const ContextMenuParams& params,
                                         ContextMenuSourceType type) {
  // Allow delegates to handle the context menu operation first.
  if (web_contents_->GetDelegate() &&
      web_contents_->GetDelegate()->HandleContextMenu(params)) {
    return;
  }

  if (delegate())
    delegate()->ShowContextMenu(params, type);
  else
    DLOG(ERROR) << "Cannot show context menus without a delegate.";
}

// Display a popup menu for WebKit using Cocoa widgets.
void WebContentsViewMac::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  PopupMenuHelper popup_menu_helper(web_contents_->GetRenderViewHost());
  popup_menu_helper.ShowPopupMenu(bounds, item_height, item_font_size,
                                  selected_item, items, right_aligned,
                                  allow_multiple_selection);
}

bool WebContentsViewMac::IsEventTracking() const {
  return base::MessagePumpMac::IsHandlingSendEvent();
}

// Arrange to call CloseTab() after we're back to the main event loop.
// The obvious way to do this would be PostNonNestableTask(), but that
// will fire when the event-tracking loop polls for events.  So we
// need to bounce the message via Cocoa, instead.
void WebContentsViewMac::CloseTabAfterEventTracking() {
  [cocoa_view_ cancelDeferredClose];
  [cocoa_view_ performSelector:@selector(closeTabAfterEvent)
                    withObject:nil
                    afterDelay:0.0];
}

gfx::Rect WebContentsViewMac::GetViewBounds() const {
  // This method is not currently used on mac.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void WebContentsViewMac::SetAllowOverlappingViews(bool overlapping) {
  if (allow_overlapping_views_ == overlapping)
    return;

  allow_overlapping_views_ = overlapping;
  RenderWidgetHostViewMac* view = static_cast<RenderWidgetHostViewMac*>(
      web_contents_->GetRenderWidgetHostView());
  if (view)
    view->SetAllowOverlappingViews(allow_overlapping_views_);
}

void WebContentsViewMac::CloseTab() {
  web_contents_->Close(web_contents_->GetRenderViewHost());
}

}  // namespace content

@implementation WebContentsViewCocoa

- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    webContentsView_ = w;
    dragDest_.reset(
        [[WebDragDest alloc] initWithWebContentsImpl:[self webContents]]);
    [self registerDragTypes];

    [[NSNotificationCenter defaultCenter]
         addObserver:self
            selector:@selector(viewDidBecomeFirstResponder:)
                name:kViewDidBecomeFirstResponder
              object:nil];

    if (webContentsView_->delegate()) {
      [dragDest_ setDragDelegate:webContentsView_->delegate()->
          GetDragDestDelegate()];
    }
  }
  return self;
}

- (void)dealloc {
  // Cancel any deferred tab closes, just in case.
  [self cancelDeferredClose];

  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

// Registers for the view for the appropriate drag types.
- (void)registerDragTypes {
  NSArray* types = [NSArray arrayWithObjects:
      ui::kChromeDragDummyPboardType,
      kWebURLsWithTitlesPboardType,
      NSURLPboardType,
      NSStringPboardType,
      NSHTMLPboardType,
      NSRTFPboardType,
      NSFilenamesPboardType,
      ui::kWebCustomDataPboardType,
      nil];
  [self registerForDraggedTypes:types];
}

- (void)setCurrentDragOperation:(NSDragOperation)operation {
  [dragDest_ setCurrentOperation:operation];
}

- (WebDropData*)dropData {
  return [dragDest_ currentDropData];
}

- (WebContentsImpl*)webContents {
  if (webContentsView_ == NULL)
    return NULL;
  return webContentsView_->web_contents();
}

- (void)mouseEvent:(NSEvent*)theEvent {
  WebContentsImpl* webContents = [self webContents];
  if (webContents && webContents->GetDelegate()) {
    NSPoint location = [NSEvent mouseLocation];
    if ([theEvent type] == NSMouseMoved)
      webContents->GetDelegate()->ContentsMouseEvent(
          webContents, gfx::Point(location.x, location.y), true);
    if ([theEvent type] == NSMouseExited)
      webContents->GetDelegate()->ContentsMouseEvent(
          webContents, gfx::Point(location.x, location.y), false);
  }
}

- (void)setMouseDownCanMoveWindow:(BOOL)canMove {
  mouseDownCanMoveWindow_ = canMove;
}

- (BOOL)mouseDownCanMoveWindow {
  // This is needed to prevent mouseDowns from moving the window
  // around.  The default implementation returns YES only for opaque
  // views.  WebContentsViewCocoa does not draw itself in any way, but
  // its subviews do paint their entire frames.  Returning NO here
  // saves us the effort of overriding this method in every possible
  // subview.
  return mouseDownCanMoveWindow_;
}

- (void)pasteboard:(NSPasteboard*)sender provideDataForType:(NSString*)type {
  [dragSource_ lazyWriteToPasteboard:sender
                             forType:type];
}

- (void)startDragWithDropData:(const WebDropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset {
  dragSource_.reset([[WebDragSource alloc]
      initWithContents:[self webContents]
                  view:self
              dropData:&dropData
                 image:image
                offset:offset
            pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
     dragOperationMask:operationMask]);
  [dragSource_ startDrag];
}

// NSDraggingSource methods

// Returns what kind of drag operations are available. This is a required
// method for NSDraggingSource.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  if (dragSource_.get())
    return [dragSource_ draggingSourceOperationMaskForLocal:isLocal];
  // No web drag source - this is the case for dragging a file from the
  // downloads manager. Default to copy operation. Note: It is desirable to
  // allow the user to either move or copy, but this requires additional
  // plumbing to update the download item's path once its moved.
  return NSDragOperationCopy;
}

// Called when a drag initiated in our view ends.
- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)screenPoint
           operation:(NSDragOperation)operation {
  [dragSource_ endDragAt:screenPoint operation:operation];

  // Might as well throw out this object now.
  dragSource_.reset();
}

// Called when a drag initiated in our view moves.
- (void)draggedImage:(NSImage*)draggedImage movedTo:(NSPoint)screenPoint {
  [dragSource_ moveDragTo:screenPoint];
}

// Called when we're informed where a file should be dropped.
- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDest {
  if (![dropDest isFileURL])
    return nil;

  NSString* file_name = [dragSource_ dragPromisedFileTo:[dropDest path]];
  if (!file_name)
    return nil;

  return [NSArray arrayWithObject:file_name];
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [dragDest_ draggingEntered:sender view:self];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  [dragDest_ draggingExited:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return [dragDest_ draggingUpdated:sender view:self];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [dragDest_ performDragOperation:sender view:self];
}

- (void)cancelDeferredClose {
  SEL aSel = @selector(closeTabAfterEvent);
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:aSel
                                             object:nil];
}

- (void)clearWebContentsView {
  webContentsView_ = NULL;
  [dragSource_ clearWebContentsView];
}

- (void)closeTabAfterEvent {
  webContentsView_->CloseTab();
}

- (void)viewDidBecomeFirstResponder:(NSNotification*)notification {
  NSView* view = [notification object];
  if (![[self subviews] containsObject:view])
    return;

  NSSelectionDirection direction =
      [[[notification userInfo] objectForKey:kSelectionDirection]
        unsignedIntegerValue];
  if (direction == NSDirectSelection)
    return;

  [self webContents]->
      FocusThroughTabTraversal(direction == NSSelectingPrevious);
}

@end
