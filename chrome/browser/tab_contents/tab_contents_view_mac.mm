// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_mac.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h" // TODO(beng): this dependency is awful.
#include "chrome/browser/cocoa/nsimage_cache.h"
#include "chrome/browser/cocoa/sad_tab_view.h"
#import "chrome/browser/cocoa/web_drop_target.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "net/base/net_util.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

#include "chrome/common/temp_scaffolding_stubs.h"

@interface TabContentsViewCocoa (Private)
- (id)initWithTabContentsViewMac:(TabContentsViewMac*)w;
- (void)processKeyboardEvent:(NSEvent*)event;
- (TabContents*)tabContents;
- (void)registerDragTypes;
- (void)setIsDropTarget:(BOOL)isTarget;
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
}

void TabContentsViewMac::CreateView() {
  TabContentsViewCocoa* view =
      [[TabContentsViewCocoa alloc] initWithTabContentsViewMac:this];
  cocoa_view_.reset(view);
}

RenderWidgetHostView* TabContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
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

// Returns a drag pasteboard filled with the appropriate data. The types are
// populated in decending order of richness.
NSPasteboard* TabContentsViewMac::FillDragData(
    const WebDropData& drop_data) {
  NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pasteboard declareTypes:[NSArray array] owner:nil];

  // HTML.
  if (!drop_data.text_html.empty()) {
    [pasteboard addTypes:[NSArray arrayWithObject:NSHTMLPboardType]
                   owner:nil];
    [pasteboard setString:base::SysUTF16ToNSString(drop_data.text_html)
                  forType:NSHTMLPboardType];
  }

  // URL.
  if (drop_data.url.is_valid()) {
    // TODO(pinkerton/jrg): special javascript: handling for bookmark bar. Win
    // doesn't allow you to drop js: bookmarks on the desktop (since they're
    // meaningless) but does allow you to drop them on the bookmark bar (where
    // they're intended to go generally). We need to figure out a private
    // flavor for Bookmark dragging and then flag this down in the drag source.
    [pasteboard addTypes:[NSArray arrayWithObject:NSURLPboardType]
                   owner:nil];
    NSString* url = base::SysUTF8ToNSString(drop_data.url.spec());
    NSString* title = base::SysUTF16ToNSString(drop_data.url_title);
    [pasteboard setURLs:[NSArray arrayWithObject:url]
             withTitles:[NSArray arrayWithObject:title]];
  }

  // Files.
  // TODO(pinkerton): Hook up image drags, data is in drop_data.file_contents.
  if (!drop_data.file_contents.empty()) {
#if 0
    // Images without ALT text will only have a file extension so we need to
    // synthesize one from the provided extension and URL.
    std::string filename_utf8 =
        [base::SysUTF16ToNSString(drop_data.file_description_filename)
            fileSystemRepresentation];
    FilePath file_name(filename_utf8);
    file_name = file_name.BaseName().RemoveExtension();
    if (file_name.value().empty()) {
      // Retrieve the name from the URL.
      file_name = FilePath::FromWStringHack(
          net::GetSuggestedFilename(drop_data.url, "", "", L""));
    }
    std::string file_extension_utf8 =
        [base::SysUTF16ToNSString(drop_data.file_extension)
            fileSystemRepresentation];
    file_name = file_name.ReplaceExtension(file_extension_utf8);
    NSArray* types = [NSArray arrayWithObjects:NSFileContentsPboardType,
                                               NSFilenamesPboardType,
                                               nil];
    [pasteboard addTypes:types owner:nil];
    NSArray* file_name_array =
        [NSArray arrayWithObject:base::SysUTF8ToNSString(file_name.value())];
    [pasteboard setPropertyList:file_name_array forType:NSFilenamesPboardType];
    NSData* data = [NSData dataWithBytes:drop_data.file_contents.data()
                                  length:drop_data.file_contents.length()];
    [pasteboard setData:data forType:NSFileContentsPboardType];
#endif
  }

  // Plain text.
  if (!drop_data.plain_text.empty()) {
    [pasteboard addTypes:[NSArray arrayWithObject:NSStringPboardType]
                   owner:nil];
    [pasteboard setString:base::SysUTF16ToNSString(drop_data.plain_text)
                  forType:NSStringPboardType];
  }
  return pasteboard;
}

void TabContentsViewMac::StartDragging(const WebDropData& drop_data) {
  // We are only allowed to call dragImage:... from inside mouseDragged:, which
  // we will never be (we're called back async), but it seems that the mouse
  // event is still always the proper left mouse drag, so everything works out
  // in the end. However, we occasionally get spurrious "start drag" messages
  // from the back-end when we shouldn't. If we go through with the drag, Cocoa
  // asserts in a bad way. Just bail for now until we can figure out the root of
  // why we're getting the messages.
  // TODO(pinkerton): http://crbug.com/16811
  NSEvent* currentEvent = [NSApp currentEvent];
  if ([currentEvent type] != NSLeftMouseDragged) {
    LOG(INFO) << "Spurious StartDragging() message";
    RenderViewHost* rvh = tab_contents()->render_view_host();
    if (rvh)
      rvh->DragSourceSystemDragEnded();
    return;
  }

  // Create an image to use for the drag.
  // TODO(pinkerton): Generate the proper image. This one will do in a pinch.
  NSImage* dragImage = nsimage_cache::ImageNamed(@"nav.pdf");

  NSPasteboard* pasteboard = FillDragData(drop_data);

  // Tell the view to start a drag using |cocoa_view_| as the drag source. The
  // source will get notified when the drag completes (success or failure) so
  // it can tell the render view host the drag is done. Windows does this with
  // a nested event loop, we get called back.
  NSPoint mousePoint = [currentEvent locationInWindow];
  mousePoint = [cocoa_view_ convertPoint:mousePoint fromView:nil];
  [cocoa_view_ dragImage:dragImage
                      at:mousePoint
                  offset:NSZeroSize
                   event:currentEvent
              pasteboard:pasteboard
                  source:cocoa_view_
               slideBack:YES];
}

void TabContentsViewMac::OnContentsDestroy() {
}

void TabContentsViewMac::RenderViewCreated(RenderViewHost* host) {
  // We want updates whenever the intrinsic width of the webpage
  // changes. Put the RenderView into that mode.
  int routing_id = host->routing_id();
  host->Send(new ViewMsg_EnableIntrinsicWidthChangedMode(routing_id));
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
  // TODO(brettw) this is a hack and should be removed. See tab_contents_view.h.
  NOTIMPLEMENTED();  // Leaving the hack unimplemented.
}

void TabContentsViewMac::Focus() {
  [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
}

void TabContentsViewMac::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
}

void TabContentsViewMac::StoreFocus() {
  NSResponder* current_focus = [[cocoa_view_.get() window] firstResponder];
  if ([current_focus isKindOfClass:[NSView class]]) {
    NSView* current_focus_view = (NSView*)current_focus;

    if ([current_focus_view isDescendantOf:cocoa_view_.get()]) {
      latent_focus_view_.reset([current_focus_view retain]);
      return;
    }
  }

  latent_focus_view_.reset();
}

void TabContentsViewMac::RestoreFocus() {
  // TODO(avi): Could we be restoring a view that's no longer in the key view
  // chain?
  if (latent_focus_view_.get()) {
    [[cocoa_view_ window] makeFirstResponder:latent_focus_view_.get()];
    latent_focus_view_.reset();
  } else {
    // TODO(shess): If location-bar gets focus by default, this will
    // select-all in the field.  If there was a specific selection in
    // the field when we navigated away from it, we should restore
    // that selection.
    SetInitialFocus();
  }
}

void TabContentsViewMac::UpdateDragCursor(bool is_drop_target) {
  [cocoa_view_ setIsDropTarget:is_drop_target ? YES : NO];
}

void TabContentsViewMac::GotFocus() {
  NOTIMPLEMENTED();
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

void TabContentsViewMac::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  [cocoa_view_.get() processKeyboardEvent:event.os_event];
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

- (void)setIsDropTarget:(BOOL)isTarget {
  [dropTarget_ setIsDropTarget:isTarget];
}

- (TabContents*)tabContents {
  return tabContentsView_->tab_contents();
}

- (void)processKeyboardEvent:(NSEvent*)event {
  if ([event type] == NSKeyDown)
    [super keyDown:event];
  else if ([event type] == NSKeyUp)
    [super keyUp:event];
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

- (void)paste:(id)sender {
  [self tabContents]->Paste();
}

// NSDraggingSource methods

// Returns what kind of drag operations are available. This is a required
// method for NSDraggingSource.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  // TODO(pinkerton): I think this is right...
  return NSDragOperationCopy;
}

// Called when a drag initiated in our view ends. We need to make sure that
// we tell WebCore so that it can go about processing things as normal.
- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)aPoint
           operation:(NSDragOperation)operation {
  RenderViewHost* rvh = [self tabContents]->render_view_host();
  if (rvh)
    rvh->DragSourceSystemDragEnded();
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

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

@end
