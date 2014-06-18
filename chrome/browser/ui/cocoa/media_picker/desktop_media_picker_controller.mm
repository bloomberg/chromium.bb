// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

#include "base/bind.h"
#import "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_item.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

const int kInitialContentWidth = 620;
const int kMinimumContentWidth = 500;
const int kMinimumContentHeight = 390;
const int kThumbnailWidth = 150;
const int kThumbnailHeight = 150;
const int kFramePadding = 20;
const int kControlSpacing = 10;
const int kExcessButtonPadding = 6;

}  // namespace

@interface DesktopMediaPickerController (Private)

// Populate the window with controls and views.
- (void)initializeContentsWithAppName:(const base::string16&)appName;

// Create a |NSTextField| with label traits given |width|. Frame height is
// automatically adjusted to fit.
- (NSTextField*)createTextFieldWithText:(NSString*)text
                             frameWidth:(CGFloat)width;

// Create a button with |title|, with size adjusted to fit.
- (NSButton*)createButtonWithTitle:(NSString*)title;

// Report result by invoking |doneCallback_|. The callback is invoked only on
// the first call to |reportResult:|. Subsequent calls will be no-ops.
- (void)reportResult:(content::DesktopMediaID)sourceID;

// Action handlers.
- (void)sharePressed:(id)sender;
- (void)cancelPressed:(id)sender;

@end

@implementation DesktopMediaPickerController

- (id)initWithMediaList:(scoped_ptr<DesktopMediaList>)media_list
                 parent:(NSWindow*)parent
               callback:(const DesktopMediaPicker::DoneCallback&)callback
                appName:(const base::string16&)appName
             targetName:(const base::string16&)targetName {
  const NSUInteger kStyleMask =
      NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask;
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:kStyleMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);

  if ((self = [super initWithWindow:window])) {
    [parent addChildWindow:window ordered:NSWindowAbove];
    [window setDelegate:self];
    [self initializeContentsWithAppName:appName targetName:targetName];
    media_list_ = media_list.Pass();
    media_list_->SetViewDialogWindowId([window windowNumber]);
    doneCallback_ = callback;
    items_.reset([[NSMutableArray alloc] init]);
    bridge_.reset(new DesktopMediaPickerBridge(self));
  }
  return self;
}

- (void)dealloc {
  [shareButton_ setTarget:nil];
  [cancelButton_ setTarget:nil];
  [sourceBrowser_ setDelegate:nil];
  [sourceBrowser_ setDataSource:nil];
  [[self window] close];
  [super dealloc];
}

- (void)initializeContentsWithAppName:(const base::string16&)appName
                           targetName:(const base::string16&)targetName {
  // Use flipped coordinates to facilitate manual layout.
  const CGFloat kPaddedWidth = kInitialContentWidth - (kFramePadding * 2);
  base::scoped_nsobject<FlippedView> content(
      [[FlippedView alloc] initWithFrame:NSZeroRect]);
  [[self window] setContentView:content];
  NSPoint origin = NSMakePoint(kFramePadding, kFramePadding);

  // Set the dialog's title.
  NSString* titleText = l10n_util::GetNSStringF(
      IDS_DESKTOP_MEDIA_PICKER_TITLE, appName);
  [[self window] setTitle:titleText];

  // Set the dialog's description.
  NSString* descriptionText;
  if (appName == targetName) {
    descriptionText = l10n_util::GetNSStringF(
        IDS_DESKTOP_MEDIA_PICKER_TEXT, appName);
  } else {
    descriptionText = l10n_util::GetNSStringF(
        IDS_DESKTOP_MEDIA_PICKER_TEXT_DELEGATED, appName, targetName);
  }
  NSTextField* description = [self createTextFieldWithText:descriptionText
                                                frameWidth:kPaddedWidth];
  [description setFrameOrigin:origin];
  [content addSubview:description];
  origin.y += NSHeight([description frame]) + kControlSpacing;

  // Create the image browser.
  sourceBrowser_.reset([[IKImageBrowserView alloc] initWithFrame:NSZeroRect]);
  NSUInteger cellStyle = IKCellsStyleShadowed | IKCellsStyleTitled;
  [sourceBrowser_ setDelegate:self];
  [sourceBrowser_ setDataSource:self];
  [sourceBrowser_ setCellsStyleMask:cellStyle];
  [sourceBrowser_ setCellSize:NSMakeSize(kThumbnailWidth, kThumbnailHeight)];

  // Create a scroll view to host the image browser.
  NSRect imageBrowserScrollFrame = NSMakeRect(
      origin.x, origin.y, kPaddedWidth, 350);
  base::scoped_nsobject<NSScrollView> imageBrowserScroll(
      [[NSScrollView alloc] initWithFrame:imageBrowserScrollFrame]);
  [imageBrowserScroll setHasVerticalScroller:YES];
  [imageBrowserScroll setDocumentView:sourceBrowser_];
  [imageBrowserScroll setBorderType:NSBezelBorder];
  [imageBrowserScroll setAutoresizingMask:
      NSViewWidthSizable | NSViewHeightSizable];
  [content addSubview:imageBrowserScroll];
  origin.y += NSHeight(imageBrowserScrollFrame) + kControlSpacing;

  // Create the share button.
  shareButton_ = [self createButtonWithTitle:l10n_util::GetNSString(
      IDS_DESKTOP_MEDIA_PICKER_SHARE)];
  origin.x = kInitialContentWidth - kFramePadding -
      (NSWidth([shareButton_ frame]) - kExcessButtonPadding);
  [shareButton_ setEnabled:NO];
  [shareButton_ setFrameOrigin:origin];
  [shareButton_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [shareButton_ setTarget:self];
  [shareButton_ setAction:@selector(sharePressed:)];
  [content addSubview:shareButton_];

  // Create the cancel button.
  cancelButton_ =
      [self createButtonWithTitle:l10n_util::GetNSString(IDS_CANCEL)];
  origin.x -= kControlSpacing +
      (NSWidth([cancelButton_ frame]) - (kExcessButtonPadding * 2));
  [cancelButton_ setFrameOrigin:origin];
  [cancelButton_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(cancelPressed:)];
  [content addSubview:cancelButton_];
  origin.y += kFramePadding +
      (NSHeight([cancelButton_ frame]) - kExcessButtonPadding);

  // Resize window to fit.
  [[[self window] contentView] setAutoresizesSubviews:NO];
  [[self window] setContentSize:NSMakeSize(kInitialContentWidth, origin.y)];
  [[self window] setContentMinSize:
      NSMakeSize(kMinimumContentWidth, kMinimumContentHeight)];
  [[[self window] contentView] setAutoresizesSubviews:YES];
}

- (void)showWindow:(id)sender {
  // Signal the media_list to start sending thumbnails. |bridge_| is used as the
  // observer, and will forward notifications to this object.
  media_list_->SetThumbnailSize(gfx::Size(kThumbnailWidth, kThumbnailHeight));
  media_list_->StartUpdating(bridge_.get());

  [self.window center];
  [super showWindow:sender];
}

- (void)reportResult:(content::DesktopMediaID)sourceID {
  if (doneCallback_.is_null()) {
    return;
  }

  // Notify the |callback_| asynchronously because it may release the
  // controller.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(doneCallback_, sourceID));
  doneCallback_.Reset();
}

- (void)sharePressed:(id)sender {
  NSIndexSet* indexes = [sourceBrowser_ selectionIndexes];
  NSUInteger selectedIndex = [indexes firstIndex];
  DesktopMediaPickerItem* item =
      [items_ objectAtIndex:selectedIndex];
  [self reportResult:[item sourceID]];
  [self close];
}

- (void)cancelPressed:(id)sender {
  [self reportResult:content::DesktopMediaID()];
  [self close];
}

- (NSTextField*)createTextFieldWithText:(NSString*)text
                             frameWidth:(CGFloat)width {
  NSRect frame = NSMakeRect(0, 0, width, 1);
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:frame]);
  [textField setEditable:NO];
  [textField setSelectable:YES];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setStringValue:text];
  [textField setFont:[NSFont systemFontOfSize:13]];
  [textField setAutoresizingMask:NSViewWidthSizable];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:textField];
  return textField.autorelease();
}

- (NSButton*)createButtonWithTitle:(NSString*)title {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRoundedBezelStyle];
  [button setTitle:title];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button];
  return button.autorelease();
}

#pragma mark NSWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
  // Report the result if it hasn't been reported yet. |reportResult:| ensures
  // that the result is only reported once.
  [self reportResult:content::DesktopMediaID()];

  // Remove self from the parent.
  NSWindow* window = [self window];
  [[window parentWindow] removeChildWindow:window];
}

#pragma mark IKImageBrowserDataSource

- (NSUInteger)numberOfItemsInImageBrowser:(IKImageBrowserView*)browser {
  return [items_ count];
}

- (id)imageBrowser:(IKImageBrowserView *)browser
       itemAtIndex:(NSUInteger)index {
  return [items_ objectAtIndex:index];
}

#pragma mark IKImageBrowserDelegate

- (void)imageBrowser:(IKImageBrowserView *)browser
      cellWasDoubleClickedAtIndex:(NSUInteger)index {
  DesktopMediaPickerItem* item = [items_ objectAtIndex:index];
  [self reportResult:[item sourceID]];
  [self close];
}

- (void)imageBrowserSelectionDidChange:(IKImageBrowserView*) aBrowser {
  // Enable or disable the OK button based on whether we have a selection.
  [shareButton_ setEnabled:([[sourceBrowser_ selectionIndexes] count] > 0)];
}

#pragma mark DesktopMediaPickerObserver

- (void)sourceAddedAtIndex:(int)index {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  NSString* imageTitle = base::SysUTF16ToNSString(source.name);
  base::scoped_nsobject<DesktopMediaPickerItem> item(
      [[DesktopMediaPickerItem alloc] initWithSourceId:source.id
                                              imageUID:++lastImageUID_
                                            imageTitle:imageTitle]);
  [items_ insertObject:item atIndex:index];
  [sourceBrowser_ reloadData];
}

- (void)sourceRemovedAtIndex:(int)index {
  if ([[sourceBrowser_ selectionIndexes] containsIndex:index]) {
    // Selected item was removed. Clear selection.
    [sourceBrowser_ setSelectionIndexes:[NSIndexSet indexSet]
                      byExtendingSelection:FALSE];
  }
  [items_ removeObjectAtIndex:index];
  [sourceBrowser_ reloadData];
}

- (void)sourceMovedFrom:(int)oldIndex to:(int)newIndex {
  base::scoped_nsobject<DesktopMediaPickerItem> item(
      [[items_ objectAtIndex:oldIndex] retain]);
  [items_ removeObjectAtIndex:oldIndex];
  [items_ insertObject:item atIndex:newIndex];
  [sourceBrowser_ reloadData];
}

- (void)sourceNameChangedAtIndex:(int)index {
  DesktopMediaPickerItem* item = [items_ objectAtIndex:index];
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  [item setImageTitle:base::SysUTF16ToNSString(source.name)];
  [sourceBrowser_ reloadData];
}

- (void)sourceThumbnailChangedAtIndex:(int)index {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  NSImage* image = gfx::NSImageFromImageSkia(source.thumbnail);

  DesktopMediaPickerItem* item = [items_ objectAtIndex:index];
  [item setImageRepresentation:image];
  [sourceBrowser_ reloadData];
}

@end  // @interface DesktopMediaPickerController
