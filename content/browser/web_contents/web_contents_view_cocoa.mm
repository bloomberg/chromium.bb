// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_contents_view_cocoa.h"

#import "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_mac.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#import "content/browser/web_contents/web_drag_source_mac.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/dragdrop/cocoa_dnd_util.h"

using content::DraggingInfo;
using content::DropData;
using content::WebContentsImpl;
using content::WebContentsViewMac;

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewCocoa

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
      [dragDest_
          setDragDelegate:webContentsView_->delegate()->GetDragDestDelegate()];
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

- (void)populateDraggingInfo:(DraggingInfo*)info
          fromNSDraggingInfo:(id<NSDraggingInfo>)nsInfo {
  NSPoint windowPoint = [nsInfo draggingLocation];

  NSPoint viewPoint = [self convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [self frame];
  info->location_in_view =
      gfx::PointF(viewPoint.x, viewFrame.size.height - viewPoint.y);

  NSPoint screenPoint =
      ui::ConvertPointFromWindowToScreen([self window], windowPoint);
  NSScreen* screen = [[self window] screen];
  NSRect screenFrame = [screen frame];
  info->location_in_screen =
      gfx::PointF(screenPoint.x, screenFrame.size.height - screenPoint.y);

  info->location_in_view = gfx::PointF(viewPoint.x, viewPoint.y);
  info->location_in_screen = gfx::PointF(screenPoint.x, screenPoint.y);
  NSPasteboard* pboard = [nsInfo draggingPasteboard];
  if ([pboard containsURLDataConvertingTextToURL:YES]) {
    GURL url;
    ui::PopulateURLAndTitleFromPasteboard(&url, NULL, pboard, YES);
    info->url.emplace(url);
  }
  info->operation_mask = [nsInfo draggingSourceOperationMask];
}

- (BOOL)allowsVibrancy {
  // Returning YES will allow rendering this view with vibrancy effect if it is
  // incorporated into a view hierarchy that uses vibrancy, it will have no
  // effect otherwise.
  // For details see Apple documentation on NSView and NSVisualEffectView.
  return YES;
}

// Registers for the view for the appropriate drag types.
- (void)registerDragTypes {
  NSArray* types =
      [NSArray arrayWithObjects:ui::kChromeDragDummyPboardType,
                                kWebURLsWithTitlesPboardType, NSURLPboardType,
                                NSStringPboardType, NSHTMLPboardType,
                                NSRTFPboardType, NSFilenamesPboardType,
                                ui::kWebCustomDataPboardType, nil];
  [self registerForDraggedTypes:types];
}

- (void)setCurrentDragOperation:(NSDragOperation)operation {
  [dragDest_ setCurrentOperation:operation];
}

- (DropData*)dropData {
  return [dragDest_ currentDropData];
}

- (WebContentsImpl*)webContents {
  if (!webContentsView_)
    return nullptr;
  return webContentsView_->web_contents();
}

- (void)mouseEvent:(NSEvent*)theEvent {
  WebContentsImpl* webContents = [self webContents];
  if (webContents && webContents->GetDelegate()) {
    webContents->GetDelegate()->ContentsMouseEvent(
        webContents, [theEvent type] == NSMouseMoved,
        [theEvent type] == NSMouseExited);
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
  [dragSource_ lazyWriteToPasteboard:sender forType:type];
}

- (void)startDragWithDropData:(const DropData&)dropData
                    sourceRWH:(content::RenderWidgetHostImpl*)sourceRWH
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset {
  if (![self webContents])
    return;
  [dragDest_ setDragStartTrackersForProcess:sourceRWH->GetProcess()->GetID()];
  dragSource_.reset([[WebDragSource alloc]
       initWithContents:[self webContents]
                   view:self
               dropData:&dropData
              sourceRWH:sourceRWH
                  image:image
                 offset:offset
             pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
      dragOperationMask:operationMask]);
  [dragSource_ startDrag];
}

// NSDraggingSource methods

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  if (dragSource_)
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
}

// Called when a file drag is dropped and the promised files need to be written.
- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDest {
  if (![dropDest isFileURL])
    return nil;

  NSString* fileName = [dragSource_ dragPromisedFileTo:[dropDest path]];
  if (!fileName)
    return nil;

  return @[ fileName ];
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  // Fill out a DropData from pasteboard.
  DropData dropData;
  content::PopulateDropDataFromPasteboard(&dropData,
                                          [sender draggingPasteboard]);
  [dragDest_ setDropData:dropData];

  DraggingInfo draggingInfo;
  [self populateDraggingInfo:&draggingInfo fromNSDraggingInfo:sender];
  return [dragDest_ draggingEntered:draggingInfo];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  [dragDest_ draggingExited];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  DraggingInfo draggingInfo;
  [self populateDraggingInfo:&draggingInfo fromNSDraggingInfo:sender];
  return [dragDest_ draggingUpdated:draggingInfo];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  DraggingInfo draggingInfo;
  [self populateDraggingInfo:&draggingInfo fromNSDraggingInfo:sender];
  return [dragDest_ performDragOperation:draggingInfo];
}

- (void)cancelDeferredClose {
  SEL aSel = @selector(closeTabAfterEvent);
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:aSel
                                             object:nil];
}

- (void)clearWebContentsView {
  webContentsView_ = nullptr;
  [dragSource_ clearWebContentsView];
}

- (void)closeTabAfterEvent {
  if (webContentsView_)
    webContentsView_->CloseTab();
}

- (void)viewDidBecomeFirstResponder:(NSNotification*)notification {
  if (![self webContents])
    return;

  NSView* view = [notification object];
  if (![[self subviews] containsObject:view])
    return;

  NSSelectionDirection direction =
      static_cast<NSSelectionDirection>([[[notification userInfo]
          objectForKey:kSelectionDirection] unsignedIntegerValue]);
  if (direction == NSDirectSelection)
    return;

  [self webContents] -> FocusThroughTabTraversal(direction ==
                                                 NSSelectingPrevious);
}

- (void)updateWebContentsVisibility {
  if (!webContentsView_)
    return;
  content::Visibility visibility = content::Visibility::VISIBLE;
  if ([self isHiddenOrHasHiddenAncestor] || ![self window])
    visibility = content::Visibility::HIDDEN;
  else if ([[self window] occlusionState] & NSWindowOcclusionStateVisible)
    visibility = content::Visibility::VISIBLE;
  else
    visibility = content::Visibility::OCCLUDED;
  if (webContentsView_)
    webContentsView_->OnWindowVisibilityChanged(visibility);
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  // Subviews do not participate in auto layout unless the the size this view
  // changes. This allows RenderWidgetHostViewMac::SetBounds(..) to select a
  // size of the subview that differs from its superview in preparation for an
  // upcoming WebContentsView resize.
  // See http://crbug.com/264207 and http://crbug.com/655112.
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];

  // Perform manual layout of subviews, e.g., when the window size changes.
  for (NSView* subview in [self subviews])
    [subview setFrame:[self bounds]];
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSWindow* oldWindow = [self window];
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  if (oldWindow) {
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidChangeOcclusionStateNotification
                object:oldWindow];
  }
  if (newWindow) {
    [notificationCenter addObserver:self
                           selector:@selector(windowChangedOcclusionState:)
                               name:NSWindowDidChangeOcclusionStateNotification
                             object:newWindow];
  }
}

- (void)windowChangedOcclusionState:(NSNotification*)notification {
  [self updateWebContentsVisibility];
}

- (void)viewDidMoveToWindow {
  [self updateWebContentsVisibility];
}

- (void)viewDidHide {
  [self updateWebContentsVisibility];
}

- (void)viewDidUnhide {
  [self updateWebContentsVisibility];
}

- (void)setAccessibilityParentElement:(id)accessibilityParent {
  accessibilityParent_.reset([accessibilityParent retain]);
}

// ViewsHostable protocol implementation.
- (ui::ViewsHostableView*)viewsHostableView {
  return webContentsView_;
}

// NSAccessibility informal protocol implementation.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (accessibilityParent_ &&
      [attribute isEqualToString:NSAccessibilityParentAttribute]) {
    return accessibilityParent_;
  }
  return [super accessibilityAttributeValue:attribute];
}

@end
