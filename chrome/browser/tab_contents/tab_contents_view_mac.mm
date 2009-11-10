// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>

#include "chrome/browser/tab_contents/tab_contents_view_mac.h"

#include <string>

#import "base/chrome_application_mac.h"
#include "chrome/browser/browser.h" // TODO(beng): this dependency is awful.
#import "chrome/browser/cocoa/focus_tracker.h"
#import "chrome/browser/cocoa/chrome_browser_window.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/cocoa/sad_tab_view.h"
#import "chrome/browser/cocoa/web_drag_source.h"
#import "chrome/browser/cocoa/web_drop_target.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

#include "chrome/common/temp_scaffolding_stubs.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

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

@interface TabContentsViewCocoa (Private)
- (id)initWithTabContentsViewMac:(TabContentsViewMac*)w;
- (BOOL)processKeyboardEvent:(NativeWebKeyboardEvent*)event;
- (void)registerDragTypes;
- (void)setCurrentDragOperation:(NSDragOperation)operation;
- (void)startDragWithDropData:(const WebDropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask;
- (void)cancelDeferredClose;
- (void)closeTabAfterEvent;
@end

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewMac(tab_contents);
}

TabContentsViewMac::TabContentsViewMac(TabContents* tab_contents)
    : TabContentsView(tab_contents) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_CONNECTED,
                 Source<TabContents>(tab_contents));
}

TabContentsViewMac::~TabContentsViewMac() {
  // This handles the case where a renderer close call was deferred
  // while the user was operating a UI control which resulted in a
  // close.  In that case, the Cocoa view outlives the
  // TabContentsViewMac instance due to Cocoa retain count.
  [cocoa_view_ cancelDeferredClose];
}

void TabContentsViewMac::CreateView(const gfx::Size& initial_size) {
  TabContentsViewCocoa* view =
      [[TabContentsViewCocoa alloc] initWithTabContentsViewMac:this];
  cocoa_view_.reset(view);
}

RenderWidgetHostView* TabContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  RenderWidgetHostViewMac* view =
      new RenderWidgetHostViewMac(render_widget_host);

  // Fancy layout comes later; for now just make it our size and resize it
  // with us. In case there are other siblings of the content area, we want
  // to make sure the content area is on the bottom so other things draw over
  // it.
  NSView* view_view = view->native_view();
  [view_view setFrame:[cocoa_view_.get() bounds]];
  [view_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [cocoa_view_.get() addSubview:view_view
                     positioned:NSWindowBelow
                     relativeTo:nil];
  return view;
}

gfx::NativeView TabContentsViewMac::GetNativeView() const {
  return cocoa_view_.get();
}

gfx::NativeView TabContentsViewMac::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
}

gfx::NativeWindow TabContentsViewMac::GetTopLevelNativeWindow() const {
  return [cocoa_view_.get() window];
}

void TabContentsViewMac::GetContainerBounds(gfx::Rect* out) const {
  *out = [cocoa_view_.get() NSRectToRect:[cocoa_view_.get() bounds]];
}

void TabContentsViewMac::StartDragging(const WebDropData& drop_data,
    WebDragOperationsMask allowed_operations) {
  // By allowing nested tasks, the code below also allows Close(),
  // which would deallocate |this|.  The same problem can occur while
  // processing -sendEvent:, so Close() is deferred in that case.
  // Drags from web content do not come via -sendEvent:, this sets the
  // same flag -sendEvent: would.
  chrome_application_mac::ScopedSendingEvent sendingEventScoper;

  // The drag invokes a nested event loop, arrange to continue
  // processing events.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  NSDragOperation mask = static_cast<NSDragOperation>(allowed_operations);
  [cocoa_view_ startDragWithDropData:drop_data
                   dragOperationMask:mask];
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void TabContentsViewMac::RenderViewCreated(RenderViewHost* host) {
  // We want updates whenever the intrinsic width of the webpage
  // changes. Put the RenderView into that mode.
  int routing_id = host->routing_id();
  host->Send(new ViewMsg_EnablePreferredSizeChangedMode(routing_id));
}

void TabContentsViewMac::SetPageTitle(const std::wstring& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void TabContentsViewMac::OnTabCrashed() {
  if (!sad_tab_.get()) {
    SadTabView* view = [[SadTabView alloc] initWithFrame:NSZeroRect];
    sad_tab_.reset(view);

    // Set as the dominant child.
    [cocoa_view_.get() addSubview:view];
    [view setFrame:[cocoa_view_.get() bounds]];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  }
}

void TabContentsViewMac::SizeContents(const gfx::Size& size) {
  // TODO(brettw | japhet) This is a hack and should be removed.
  // See tab_contents_view.h.
  gfx::Rect rect(gfx::Point(), size);
  TabContentsViewCocoa* view = cocoa_view_.get();
  [view setFrame:[view RectToNSRect:rect]];
}

void TabContentsViewMac::Focus() {
  [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
  [[cocoa_view_.get() window] makeKeyAndOrderFront:GetContentNativeView()];
}

void TabContentsViewMac::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
}

void TabContentsViewMac::StoreFocus() {
  // We're explicitly being asked to store focus, so don't worry if there's
  // already a view saved.
  focus_tracker_.reset(
      [[FocusTracker alloc] initWithWindow:[cocoa_view_ window]]);
}

void TabContentsViewMac::RestoreFocus() {
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

void TabContentsViewMac::UpdateDragCursor(WebDragOperation operation) {
  [cocoa_view_ setCurrentDragOperation: operation];
}

void TabContentsViewMac::GotFocus() {
  // This is only used in the views FocusManager stuff but it bleeds through
  // all subclasses. http://crbug.com/21875
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewMac::TakeFocus(bool reverse) {
  if (reverse) {
    [[cocoa_view_ window] selectPreviousKeyView:cocoa_view_.get()];
  } else {
    [[cocoa_view_ window] selectNextKeyView:cocoa_view_.get()];
  }
}

bool TabContentsViewMac::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  return [cocoa_view_.get() processKeyboardEvent:
      const_cast<NativeWebKeyboardEvent*>(&event)] == YES;
}

void TabContentsViewMac::ShowContextMenu(const ContextMenuParams& params) {
  RenderViewContextMenuMac menu(tab_contents(),
                                params,
                                GetNativeView());
  menu.Init();
}

RenderWidgetHostView* TabContentsViewMac::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  // A RenderWidgetHostViewMac has lifetime scoped to the view. We'll retain it
  // to allow it to survive the trip without being hosted.
  RenderWidgetHostView* widget_view =
      TabContentsView::CreateNewWidgetInternal(route_id, activatable);
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_view);
  [widget_view_mac->native_view() retain];

  // |widget_view_mac| needs to know how to position itself in our view.
  widget_view_mac->set_parent_view(cocoa_view_);

  return widget_view;
}

void TabContentsViewMac::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  TabContentsView::ShowCreatedWidgetInternal(widget_host_view, initial_pos);

  // A RenderWidgetHostViewMac has lifetime scoped to the view. Now that it's
  // properly embedded (or purposefully ignored) we can release the retain we
  // took in CreateNewWidgetInternal().
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_host_view);
  [widget_view_mac->native_view() release];
}

bool TabContentsViewMac::IsEventTracking() const {
  if ([NSApp isKindOfClass:[CrApplication class]] &&
      [static_cast<CrApplication*>(NSApp) isHandlingSendEvent]) {
    return true;
  }
  return false;
}

// Arrange to call CloseTab() after we're back to the main event loop.
// The obvious way to do this would be PostNonNestableTask(), but that
// will fire when the event-tracking loop polls for events.  So we
// need to bounce the message via Cocoa, instead.
void TabContentsViewMac::CloseTabAfterEventTracking() {
  [cocoa_view_ cancelDeferredClose];
  [cocoa_view_ performSelector:@selector(closeTabAfterEvent)
                    withObject:nil
                    afterDelay:0.0];
}

void TabContentsViewMac::CloseTab() {
  tab_contents()->Close(tab_contents()->render_view_host());
}

void TabContentsViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_CONNECTED: {
      if (sad_tab_.get()) {
        [sad_tab_.get() removeFromSuperview];
        sad_tab_.reset();
      }
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

@interface NSApplication(SPI)
- (void)_cycleWindowsReversed:(BOOL)reversed;
@end

@implementation TabContentsViewCocoa

- (id)initWithTabContentsViewMac:(TabContentsViewMac*)w {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    tabContentsView_ = w;
    dropTarget_.reset(
        [[WebDropTarget alloc] initWithTabContents:[self tabContents]]);
    [self registerDragTypes];
  }
  return self;
}

- (void)dealloc {
  // Cancel any deferred tab closes, just in case.
  [self cancelDeferredClose];

  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];
  [super dealloc];
}

// Registers for the view for the appropriate drag types.
// TODO(pinkerton): register for file drags.
- (void)registerDragTypes {
  NSArray* types = [NSArray arrayWithObjects:NSStringPboardType,
      NSHTMLPboardType, NSURLPboardType, nil];
  [self registerForDraggedTypes:types];
}

- (void)setCurrentDragOperation:(NSDragOperation)operation {
  [dropTarget_ setCurrentOperation:operation];
}

- (TabContents*)tabContents {
  return tabContentsView_->tab_contents();
}

- (BOOL)processKeyboardEvent:(NativeWebKeyboardEvent*)wkEvent {
  if (wkEvent->skip_in_browser)
    return NO;

  NSEvent* event = wkEvent->os_event;

  if (!event) {
    // Char events are synthesized and do not contain a real event. We are not
    // interested in them anyway.
    DCHECK(wkEvent->type == WebKit::WebInputEvent::Char);
    return NO;
  }

  // If this tab is no longer active, its window will be |nil|. In that case,
  // best ignore the event.
  if (![self window])
    return NO;
  ChromeEventProcessingWindow* window =
      (ChromeEventProcessingWindow*)[self window];
  DCHECK([window isKindOfClass:[ChromeEventProcessingWindow class]]);

  // Do not fire shortcuts on key up.
  if ([event type] == NSKeyDown) {
    // Send the event to the menu before sending it to the browser/window
    // shortcut handling, so that if a user configures cmd-left to mean
    // "previous tab", it takes precedence over the built-in "history back"
    // binding. Other than that, the |redispatchEvent| call would take care of
    // invoking the original menu item shortcut as well.
    if ([[NSApp mainMenu] performKeyEquivalent:event])
      return YES;

    if ([window handleExtraBrowserKeyboardShortcut:event])
      return YES;
    if ([window handleExtraWindowKeyboardShortcut:event])
      return YES;
  }

  // We need to re-dispatch the event, so that it is sent to the menu or other
  // cocoa mechanisms (such as the cmd-` handler).
  RenderWidgetHostViewCocoa* rwhv = static_cast<RenderWidgetHostViewCocoa*>(
      tabContentsView_->GetContentNativeView());
  DCHECK([rwhv isKindOfClass:[RenderWidgetHostViewCocoa class]]);
  [rwhv setIgnoreKeyEvents:YES];
  BOOL eventHandled = [window redispatchEvent:event];
  [rwhv setIgnoreKeyEvents:NO];
  return eventHandled;
}

- (void)mouseEvent:(NSEvent *)theEvent {
  TabContents* tabContents = [self tabContents];
  if (tabContents->delegate()) {
    if ([theEvent type] == NSMouseMoved)
      tabContents->delegate()->ContentsMouseEvent(tabContents, true);
    if ([theEvent type] == NSMouseExited)
      tabContents->delegate()->ContentsMouseEvent(tabContents, false);
  }
}

- (BOOL)mouseDownCanMoveWindow {
  // This is needed to prevent mouseDowns from moving the window
  // around.  The default implementation returns YES only for opaque
  // views.  TabContentsViewCocoa does not draw itself in any way, but
  // its subviews do paint their entire frames.  Returning NO here
  // saves us the effort of overriding this method in every possible
  // subview.
  return NO;
}

// In the Windows version, we always have cut/copy/paste enabled. This is sub-
// optimal, but we do it too. TODO(avi): Plumb the "can*" methods up from
// WebCore.

- (void)cut:(id)sender {
  [self tabContents]->Cut();
}

- (void)copy:(id)sender {
  [self tabContents]->Copy();
}

- (void)copyToFindPboard:(id)sender {
  [self tabContents]->CopyToFindPboard();
}

- (void)paste:(id)sender {
  [self tabContents]->Paste();
}

- (void)pasteAsPlainText:(id)sender {
  [self tabContents]->PasteAndMatchStyle();
}

- (void)pasteboard:(NSPasteboard*)sender provideDataForType:(NSString*)type {
  [dragSource_ lazyWriteToPasteboard:sender
                             forType:type];
}

- (void)undo:(id)sender {
  [self tabContents]->render_view_host()->Undo();
}

- (void)redo:(id)sender {
  [self tabContents]->render_view_host()->Redo();
}

- (void)startDragWithDropData:(const WebDropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask {
  dragSource_.reset([[WebDragSource alloc]
          initWithContentsView:self
                      dropData:&dropData
                    pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
             dragOperationMask:operationMask]);
  [dragSource_ startDrag];
}

// NSDraggingSource methods

// Returns what kind of drag operations are available. This is a required
// method for NSDraggingSource.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return [dragSource_ draggingSourceOperationMaskForLocal:isLocal];
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
  return [dropTarget_ draggingEntered:sender view:self];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  [dropTarget_ draggingExited:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return [dropTarget_ draggingUpdated:sender view:self];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [dropTarget_ performDragOperation:sender view:self];
}

- (void)cancelDeferredClose {
  SEL aSel = @selector(closeTabAfterEvent);
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:aSel
                                             object:nil];
}

- (void)closeTabAfterEvent {
  tabContentsView_->CloseTab();
}

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

@end
