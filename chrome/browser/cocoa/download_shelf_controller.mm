// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/download_item_controller.h"
#include "chrome/browser/cocoa/download_shelf_mac.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#include "grit/generated_resources.h"

namespace {

// TODO(thakis): These are all temporary until there's a download item view.

// Border padding of a download item.
const int kDownloadItemBorderPadding = 4;

// Width of a download item.
const int kDownloadItemWidth = 200;

// Height of a download item.
const int kDownloadItemHeight = 32;

// Horizontal padding between two download items.
const int kDownloadItemPadding = 10;

}  // namespace

@interface DownloadShelfController(Private)
- (void)applyContentAreaOffset:(BOOL)apply;
- (void)positionBar;
- (void)showDownloadShelf:(BOOL)enable;
@end


@implementation DownloadShelfController

- (id)initWithBrowser:(Browser*)browser
          contentArea:(NSView*)content {
  if ((self = [super initWithNibName:@"DownloadShelf"
                              bundle:mac_util::MainAppBundle()])) {
    contentArea_ = content;
    shelfHeight_ = [[self view] bounds].size.height;

    [self positionBar];
    [[[contentArea_ window] contentView] addSubview:[self view]];

    downloadItemControllers_.reset([[NSMutableArray alloc] init]);

    // This calls show:, so it needs to be last.
    bridge_.reset(new DownloadShelfMac(browser, self));
  }
  return self;
}

- (void)awakeFromNib {
  // Initialize "Show all downloads" link.

  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
  // TODO(thakis): left-align for RTL languages?
  [paragraphStyle.get() setAlignment:NSRightTextAlignment];

  NSDictionary* linkAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
      self, NSLinkAttributeName,
      [NSCursor pointingHandCursor], NSCursorAttributeName,
      paragraphStyle.get(), NSParagraphStyleAttributeName,
      nil];
  NSString* text =
      base::SysWideToNSString(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  scoped_nsobject<NSAttributedString> linkText([[NSAttributedString alloc]
      initWithString:text attributes:linkAttributes]);

  [[showAllDownloadsLink_ textStorage] setAttributedString:linkText.get()];
  [showAllDownloadsLink_ setDelegate:self];
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  bridge_->browser()->ShowDownloadsTab();
  return YES;
}

// Initializes the download shelf at the bottom edge of |contentArea_|.
- (void)positionBar {
  // Set the bar's height to zero and position it at the bottom of the content
  // area, within the window's content view (as opposed to the tab strip, which
  // is a sibling). We'll enlarge it and slide the content area up when we need
  // to show this strip.
  NSRect contentFrame = [contentArea_ frame];
  NSRect barFrame = NSMakeRect(0, 0, contentFrame.size.width, shelfHeight_);
  [[self view] setFrame:barFrame];
}

// Called when the contentArea's frame changes.  Enlarge the view to stay with
// the bottom of the contentArea.
- (void)resizeDownloadShelf {
  NSRect barFrame = [[self view] frame];
  barFrame.origin.y = 0;
  barFrame.size.height = NSMinY([contentArea_ frame]);
  [[self view] setFrame:barFrame];
}

- (void)remove:(DownloadItemController*)download {
  // Look for the download in our controller array and remove it. This will
  // explicity release it so that it removes itself as an Observer of the
  // DownloadItem. We don't want to wait for autorelease since the DownloadItem
  // we are observing will likely be gone by then.
  [[download view] removeFromSuperview];
  [downloadItemControllers_ removeObject:download];

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

  contentAreaHasOffset_ = enable;
  [[self view] setHidden:enable ? NO : YES];
  [self applyContentAreaOffset:enable];

  barIsVisible_ = enable;
}

// Apply a contents box offset to make (or remove) room for the download shelf.
// If apply is YES, always make room (the contentView_ is "full size"). If apply
// is NO, we are trying to undo an offset. If no offset there is nothing to undo.
- (void)applyContentAreaOffset:(BOOL)apply {
  if (!contentAreaHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  NSRect frame = [contentArea_ frame];
  if (apply) {
    frame.origin.y += shelfHeight_;
    frame.size.height -= shelfHeight_;
  } else {
    frame.origin.y -= shelfHeight_;
    frame.size.height += shelfHeight_;
  }

  [[contentArea_ animator] setFrame:frame];
  [[self view] setNeedsDisplay:YES];
  [contentArea_ setNeedsDisplay:YES];
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
}

- (float)height {
  return shelfHeight_;
}

- (void)addDownloadItem:(BaseDownloadItemModel*)model {
  // TODO(thakis): we need to delete these at some point. There's no explicit
  // mass delete on windows, figure out where they do it.

  // TODO(thakis): RTL support?
  // (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
  int startX = kDownloadItemBorderPadding +
               (kDownloadItemWidth + kDownloadItemPadding) *
               [downloadItemControllers_ count];

  NSRect position = NSMakeRect(startX, kDownloadItemBorderPadding,
                               kDownloadItemWidth, kDownloadItemHeight);
  scoped_nsobject<DownloadItemController> controller(
      [[DownloadItemController alloc] initWithFrame:position
                                              model:model
                                              shelf:self]);
  [downloadItemControllers_ addObject:controller.get()];

  [[self view] addSubview:[controller.get() view]];
}

@end
