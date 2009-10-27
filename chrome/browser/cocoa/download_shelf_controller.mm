// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_controller.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/download_item_controller.h"
#include "chrome/browser/cocoa/download_shelf_mac.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#include "chrome/browser/download/download_manager.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
const size_t kMaxDownloadItemCount = 16;

// Horizontal padding between two download items.
const int kDownloadItemPadding = 0;

// Duration for the open-new-leftmost-item animation, in seconds.
const NSTimeInterval kDownloadItemOpenDuration = 0.8;

}  // namespace

@interface DownloadShelfController(Private)
- (void)applyContentAreaOffset:(BOOL)apply;
- (void)showDownloadShelf:(BOOL)enable;
- (void)resizeDownloadLinkToFit;
- (void)layoutItems:(BOOL)skipFirst;
- (void)closed;
@end


@implementation DownloadShelfController

- (id)initWithBrowser:(Browser*)browser
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithNibName:@"DownloadShelf"
                              bundle:mac_util::MainAppBundle()])) {
    resizeDelegate_ = resizeDelegate;
    shelfHeight_ = [[self view] bounds].size.height;

    // Reset the download shelf's frame height to zero.  It will be properly
    // positioned and sized the first time we try to set its height. (Just
    // setting the rect to NSZeroRect does not work: it confuses Cocoa's view
    // layout logic. If the shelf's width is too small, cocoa makes the download
    // item container view wider than the browser window).
    NSRect frame = [[self view] frame];
    frame.size.height = 0;
    [[self view] setFrame:frame];

    downloadItemControllers_.reset([[NSMutableArray alloc] init]);

    bridge_.reset(new DownloadShelfMac(browser, self));
  }
  return self;
}

- (void)awakeFromNib {
  // Initialize "Show all downloads" link.

  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
  [paragraphStyle.get() setAlignment:NSRightTextAlignment];

  NSFont* font = [NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  NSDictionary* linkAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
      @"", NSLinkAttributeName,
      [NSCursor pointingHandCursor], NSCursorAttributeName,
      paragraphStyle.get(), NSParagraphStyleAttributeName,
      font, NSFontAttributeName,
      nil];
  NSString* text =
      base::SysWideToNSString(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  scoped_nsobject<NSAttributedString> linkText([[NSAttributedString alloc]
      initWithString:text attributes:linkAttributes]);

  [[showAllDownloadsLink_ textStorage] setAttributedString:linkText.get()];
  [showAllDownloadsLink_ setDelegate:self];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* favicon = rb.GetNSImageNamed(IDR_DOWNLOADS_FAVICON);
  DCHECK(favicon);
  [image_ setImage:favicon];

  [self resizeDownloadLinkToFit];
}

- (void)dealloc {
  // The controllers will unregister themselves as observers when they are
  // deallocated. No need to do that here.
  [super dealloc];
}

- (void)resizeDownloadLinkToFit {
  // Get width required by localized download link text.
  // http://developer.apple.com/documentation/Cocoa/Conceptual/TextLayout/Tasks/StringHeight.html
  [[showAllDownloadsLink_ textContainer] setLineFragmentPadding:0.0];
  (void)[[showAllDownloadsLink_ layoutManager] glyphRangeForTextContainer:
      [showAllDownloadsLink_ textContainer]];
  NSRect textRect = [[showAllDownloadsLink_ layoutManager]
      usedRectForTextContainer:[showAllDownloadsLink_ textContainer]];

  int offsetX = [showAllDownloadsLink_ frame].size.width - textRect.size.width;

  // Fit link itself.
  NSRect linkFrame = [linkContainer_ frame];
  linkFrame.origin.x += offsetX;
  linkFrame.size.width -= offsetX;
  [linkContainer_ setFrame:linkFrame];
  [linkContainer_ setNeedsDisplay:YES];

  // Move image.
  NSRect imageFrame = [image_ frame];
  imageFrame.origin.x += offsetX;
  [image_ setFrame:imageFrame];
  [image_ setNeedsDisplay:YES];

  // Change item container size.
  NSRect itemFrame = [itemContainerView_ frame];
  itemFrame.size.width += offsetX;
  [itemContainerView_ setFrame:itemFrame];
  [itemContainerView_ setNeedsDisplay:YES];
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  bridge_->browser()->ShowDownloadsTab();
  return YES;
}

- (void)remove:(DownloadItemController*)download {
  // Look for the download in our controller array and remove it. This will
  // explicity release it so that it removes itself as an Observer of the
  // DownloadItem. We don't want to wait for autorelease since the DownloadItem
  // we are observing will likely be gone by then.
  [[NSNotificationCenter defaultCenter] removeObserver:download];

  // TODO(dmaclach): Remove -- http://crbug.com/25845
  [[download view] removeFromSuperview];

  [downloadItemControllers_ removeObject:download];

  [self layoutItems];

  // Check to see if we have any downloads remaining and if not, hide the shelf.
  if (![downloadItemControllers_ count])
    [self showDownloadShelf:NO];
}

// We need to explicitly release our download controllers here since they need
// to remove themselves as observers before the remaining shutdown happens.
- (void)exiting {
  downloadItemControllers_.reset();
}

// Show or hide the bar based on the value of |enable|. Handles animating the
// resize of the content view.
- (void)showDownloadShelf:(BOOL)enable {
  if ([self isVisible] == enable)
    return;

  [resizeDelegate_ resizeView:[self view]
                    newHeight:(enable ? shelfHeight_ : 0)];
  barIsVisible_ = enable;
}

- (DownloadShelf*)bridge {
  return bridge_.get();
}

- (BOOL)isVisible {
  return barIsVisible_;
}

- (void)show:(id)sender {
  [self showDownloadShelf:YES];
}

- (void)hide:(id)sender {
  // If |sender| isn't nil, then we're being closed from the UI by the user and
  // we need to tell our shelf implementation to close. Otherwise, we're being
  // closed programmatically by our shelf implementation.
  if (sender)
    bridge_->Close();
  else
    [self showDownloadShelf:NO];

  // TODO(port): When closing the shelf is animated, call this only after the
  // animation has ended:
  [self closed];
}

- (float)height {
  return shelfHeight_;
}

// If |skipFirst| is true, the frame of the leftmost item is not set.
- (void)layoutItems:(BOOL)skipFirst {
  CGFloat currentX = 0;
  for (DownloadItemController* itemController
      in downloadItemControllers_.get()) {
    NSRect frame = [[itemController view] frame];
    frame.origin.x = currentX;
    frame.size.width = [itemController preferredSize].width;
    if (!skipFirst)
      [[[itemController view] animator] setFrame:frame];
    currentX += frame.size.width + kDownloadItemPadding;
    skipFirst = NO;
  }
}

- (void)layoutItems {
  [self layoutItems:NO];
}

- (void)addDownloadItem:(BaseDownloadItemModel*)model {
  // Insert new item at the left.
  scoped_nsobject<DownloadItemController> controller(
      [[DownloadItemController alloc] initWithModel:model shelf:self]);

  // Adding at index 0 in NSMutableArrays is O(1).
  [downloadItemControllers_ insertObject:controller.get() atIndex:0];

  [itemContainerView_ addSubview:[controller.get() view]];

  // The controller is in charge of removing itself as an observer in its
  // dealloc.
  [[NSNotificationCenter defaultCenter]
    addObserver:controller
       selector:@selector(updateVisibility:)
           name:NSViewFrameDidChangeNotification
         object:[controller view]];
  [[NSNotificationCenter defaultCenter]
    addObserver:controller
       selector:@selector(updateVisibility:)
           name:NSViewFrameDidChangeNotification
         object:itemContainerView_];

  // Start at width 0...
  NSSize size = [controller.get() preferredSize];
  NSRect frame = NSMakeRect(0, 0, 0, size.height);
  [[controller.get() view] setFrame:frame];

  // ...then animate in
  frame.size.width = size.width;
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kDownloadItemOpenDuration];
  [[[controller.get() view] animator] setFrame:frame];
  [NSAnimationContext endGrouping];

  // Keep only a limited number of items in the shelf.
  if ([downloadItemControllers_ count] > kMaxDownloadItemCount) {
    DCHECK(kMaxDownloadItemCount > 0);

    // Since no user will ever see the item being removed (needs a horizontal
    // screen resolution greater than 3200 at 16 items at 200 pixels each),
    // there's no point in animating the removal.
    [self remove:[downloadItemControllers_ lastObject]];
  }

  // Finally, move the remaining items to the right. Skip the first item when
  // laying out the items, so that the longer animation duration we set up above
  // is not overwritten.
  [self layoutItems:YES];
}

- (void)closed {
  NSUInteger i = 0;
  while (i < [downloadItemControllers_.get() count]) {
    DownloadItemController* itemController = [downloadItemControllers_.get()
        objectAtIndex:i];
    bool isTransferDone =
        [itemController download]->state() == DownloadItem::COMPLETE ||
        [itemController download]->state() == DownloadItem::CANCELLED;
    if (isTransferDone &&
        [itemController download]->safety_state() != DownloadItem::DANGEROUS) {
      [self remove:itemController];
    } else {
      ++i;
    }
  }
}

@end
