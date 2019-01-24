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
#include "content/public/common/web_contents_ns_view_bridge.mojom.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/dragdrop/cocoa_dnd_util.h"

using content::mojom::DraggingInfo;
using content::DropData;
using content::WebContentsImpl;
using content::WebContentsViewMac;

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewCocoa

@implementation WebContentsViewCocoa

@synthesize client = client_;

- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    webContentsView_ = w;
    [self registerDragTypes];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(viewDidBecomeFirstResponder:)
               name:kViewDidBecomeFirstResponder
             object:nil];
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

- (WebContentsImpl*)webContents {
  if (!webContentsView_)
    return nullptr;
  return webContentsView_->web_contents();
}

- (void)mouseEvent:(NSEvent*)theEvent {
  if (!client_)
    return;
  client_->OnMouseEvent([theEvent type] == NSMouseMoved,
                        [theEvent type] == NSMouseExited);
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
  if (!client_)
    return NSDragOperationNone;

  // Fill out a DropData from pasteboard.
  DropData dropData;
  content::PopulateDropDataFromPasteboard(&dropData,
                                          [sender draggingPasteboard]);
  client_->SetDropData(dropData);

  auto draggingInfo = DraggingInfo::New();
  [self populateDraggingInfo:draggingInfo.get() fromNSDraggingInfo:sender];
  uint32_t result = 0;
  client_->DraggingEntered(std::move(draggingInfo), &result);
  return result;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  if (!client_)
    return;
  client_->DraggingExited();
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  if (!client_)
    return NSDragOperationNone;
  auto draggingInfo = DraggingInfo::New();
  [self populateDraggingInfo:draggingInfo.get() fromNSDraggingInfo:sender];
  uint32_t result = 0;
  client_->DraggingUpdated(std::move(draggingInfo), &result);
  return result;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  if (!client_)
    return NO;
  auto draggingInfo = DraggingInfo::New();
  [self populateDraggingInfo:draggingInfo.get() fromNSDraggingInfo:sender];
  bool result = false;
  client_->PerformDragOperation(std::move(draggingInfo), &result);
  return result;
}

- (void)cancelDeferredClose {
  SEL aSel = @selector(closeTabAfterEvent);
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:aSel
                                             object:nil];
}

- (void)clearWebContentsView {
  webContentsView_ = nullptr;
  client_ = nullptr;
  [dragSource_ clearWebContentsView];
}

- (void)closeTabAfterEvent {
  if (webContentsView_)
    webContentsView_->CloseTab();
}

- (void)viewDidBecomeFirstResponder:(NSNotification*)notification {
  if (!client_)
    return;

  NSView* view = [notification object];
  if (![[self subviews] containsObject:view])
    return;

  NSSelectionDirection ns_direction =
      static_cast<NSSelectionDirection>([[[notification userInfo]
          objectForKey:kSelectionDirection] unsignedIntegerValue]);

  content::mojom::SelectionDirection direction;
  switch (ns_direction) {
    case NSDirectSelection:
      direction = content::mojom::SelectionDirection::kDirect;
      break;
    case NSSelectingNext:
      direction = content::mojom::SelectionDirection::kForward;
      break;
    case NSSelectingPrevious:
      direction = content::mojom::SelectionDirection::kReverse;
      break;
    default:
      return;
  }
  client_->OnBecameFirstResponder(direction);
}

- (void)updateWebContentsVisibility {
  if (!client_)
    return;
  content::mojom::Visibility visibility = content::mojom::Visibility::kVisible;
  if ([self isHiddenOrHasHiddenAncestor] || ![self window])
    visibility = content::mojom::Visibility::kHidden;
  else if ([[self window] occlusionState] & NSWindowOcclusionStateVisible)
    visibility = content::mojom::Visibility::kVisible;
  else
    visibility = content::mojom::Visibility::kOccluded;
  client_->OnWindowVisibilityChanged(visibility);
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
