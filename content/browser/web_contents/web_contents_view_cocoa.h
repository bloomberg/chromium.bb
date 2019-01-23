// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_COCOA_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "content/common/content_export.h"
#import "ui/base/cocoa/base_view.h"
#import "ui/base/cocoa/views_hostable.h"

namespace content {
struct DropData;
class RenderWidgetHostImpl;
class WebContentsImpl;
class WebContentsViewMac;
namespace mojom {
class WebContentsNSViewClient;
}  // namespace mojom
}  // namespace content

@class WebDragSource;

CONTENT_EXPORT
@interface WebContentsViewCocoa : BaseView <ViewsHostable> {
 @private
  // Instances of this class are owned by both webContentsView_ and AppKit. It
  // is possible for an instance to outlive its webContentsView_. The
  // webContentsView_ must call -clearWebContentsView in its destructor.
  content::WebContentsViewMac* webContentsView_;
  content::mojom::WebContentsNSViewClient* client_;
  base::scoped_nsobject<WebDragSource> dragSource_;
  base::scoped_nsobject<id> accessibilityParent_;
  BOOL mouseDownCanMoveWindow_;
}

// The mojo interface through which to communicate with the browser process.
@property(nonatomic, assign) content::mojom::WebContentsNSViewClient* client;

- (void)setMouseDownCanMoveWindow:(BOOL)canMove;

// Sets |accessibilityParent| as the object returned when the
// receiver is queried for its accessibility parent.
// TODO(lgrey/ellyjones): Remove this in favor of setAccessibilityParent:
// when we switch to the new accessibility API.
- (void)setAccessibilityParentElement:(id)accessibilityParent;

// Returns the available drag operations. This is a required method for
// NSDraggingSource. It is supposedly deprecated, but the non-deprecated API
// -[NSWindow dragImage:...] still relies on it.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal;

// Private interface.
// TODO(ccameron): Document these functions.
- (id)initWithWebContentsViewMac:(content::WebContentsViewMac*)w;
- (void)registerDragTypes;
- (void)startDragWithDropData:(const content::DropData&)dropData
                    sourceRWH:(content::RenderWidgetHostImpl*)sourceRWH
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset;
- (void)cancelDeferredClose;
- (void)clearWebContentsView;
- (void)closeTabAfterEvent;
- (void)updateWebContentsVisibility;
- (void)viewDidBecomeFirstResponder:(NSNotification*)notification;
- (content::WebContentsImpl*)webContents;
@end

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_COCOA_H_
