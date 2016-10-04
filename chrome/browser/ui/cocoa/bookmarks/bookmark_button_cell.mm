// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_context_menu_cocoa_controller.h"
#include "chrome/grit/generated_resources.h"
#import "components/bookmarks/browser/bookmark_model.h"
#include "content/public/browser/user_metrics.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNode;

namespace {

// Padding on the right side of the arrow icon.
const int kHierarchyButtonRightPadding = 4;

// Padding on the left side of the arrow icon.
const int kHierarchyButtonLeftPadding = 11;

const int kIconTextSpacer = 4;
const int kTextRightPadding = 4;
const int kIconLeftPadding = 4;

const int kDefaultFontSize = 12;

};  // namespace

@interface OffTheSideButtonCell : BookmarkButtonCell

- (NSString*)accessibilityTitle;

@end
@implementation OffTheSideButtonCell

- (BOOL)isOffTheSideButtonCell {
  return YES;
}

- (NSString*)accessibilityTitle {
  return l10n_util::GetNSString(IDS_ACCNAME_BOOKMARKS_CHEVRON);
}

- (NSRect)imageRectForBounds:(NSRect)theRect {
  NSRect imageRect = [super imageRectForBounds:theRect];
  // Make sure the chevron icon stays centered. Normally a bookmark bar item
  // with no label has its icon placed at a fixed x-position.
  CGFloat totalWidth = NSMaxX(theRect);
  imageRect.origin.x = (totalWidth - [self image].size.width) / 2;
  return imageRect;
}

@end

@interface BookmarkButtonCell(Private)
// Returns YES if the cell is the offTheSide button cell.
- (BOOL)isOffTheSideButtonCell;
- (void)configureBookmarkButtonCell;
- (void)applyTextColor;
// Returns the title the button cell displays. Note that a button cell can
// have a title string assigned but it won't be visible if its image position
// is NSImageOnly.
- (NSString*)visibleTitle;
// Returns the dictionary of attributes to associate with the button title.
- (NSDictionary*)titleTextAttributes;
@end


@implementation BookmarkButtonCell

@synthesize startingChildIndex = startingChildIndex_;
@synthesize drawFolderArrow = drawFolderArrow_;

+ (id)buttonCellForNode:(const BookmarkNode*)node
                   text:(NSString*)text
                  image:(NSImage*)image
         menuController:(BookmarkContextMenuCocoaController*)menuController {
  id buttonCell =
      [[[BookmarkButtonCell alloc] initForNode:node
                                          text:text
                                         image:image
                                menuController:menuController]
       autorelease];
  return buttonCell;
}

+ (id)buttonCellWithText:(NSString*)text
                   image:(NSImage*)image
          menuController:(BookmarkContextMenuCocoaController*)menuController {
  id buttonCell =
      [[[BookmarkButtonCell alloc] initWithText:text
                                          image:image
                                 menuController:menuController]
       autorelease];
  return buttonCell;
}

- (id)initForNode:(const BookmarkNode*)node
             text:(NSString*)text
            image:(NSImage*)image
   menuController:(BookmarkContextMenuCocoaController*)menuController {
  if ((self = [super initTextCell:text])) {
    menuController_ = menuController;
    [self configureBookmarkButtonCell];
    [self setTextColor:[NSColor blackColor]];
    [self setBookmarkNode:node];
    // When opening a bookmark folder, the default behavior is that the
    // favicon is greyed when menu item is hovered with the mouse cursor.
    // When using NSNoCellMask, the favicon won't be greyed when menu item
    // is hovered.
    // In the bookmark bar, the favicon is not greyed when the bookmark is
    // hovered with the mouse cursor.
    // It makes the behavior of the bookmark folder consistent with hovering
    // on the bookmark bar.
    [self setHighlightsBy:NSNoCellMask];

    if (node) {
      NSString* title = base::SysUTF16ToNSString(node->GetTitle());
      [self setBookmarkCellText:title image:image];
    } else {
      [self setEmpty:YES];
      [self setBookmarkCellText:l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU)
                          image:nil];
    }
  }

  return self;
}

- (id)initWithText:(NSString*)text
             image:(NSImage*)image
    menuController:(BookmarkContextMenuCocoaController*)menuController {
  if ((self = [super initTextCell:text])) {
    menuController_ = menuController;
    [self configureBookmarkButtonCell];
    [self setTextColor:[NSColor blackColor]];
    [self setBookmarkNode:NULL];
    [self setBookmarkCellText:text image:image];
    // This is a custom button not attached to any node. It is no considered
    // empty even if its bookmark node is NULL.
    [self setEmpty:NO];
  }

  return self;
}

- (id)initTextCell:(NSString*)string {
  return [self initForNode:nil text:string image:nil menuController:nil];
}

// Used by the off-the-side menu, the only case where a
// BookmarkButtonCell is loaded from a nib.
- (void)awakeFromNib {
  [self configureBookmarkButtonCell];
}

- (BOOL)isFolderButtonCell {
  return NO;
}

- (BOOL)isOffTheSideButtonCell {
  return NO;
}

// Perform all normal init routines specific to the BookmarkButtonCell.
- (void)configureBookmarkButtonCell {
  [self setButtonType:NSMomentaryPushInButton];
  [self setShowsBorderOnlyWhileMouseInside:YES];
  [self setControlSize:NSSmallControlSize];
  [self setAlignment:NSLeftTextAlignment];
  [self setFont:[NSFont systemFontOfSize:kDefaultFontSize]];
  [self setBordered:NO];
  [self setBezeled:NO];
  [self setWraps:NO];
  // NSLineBreakByTruncatingMiddle seems more common on OSX but let's
  // try to match Windows for a bit to see what happens.
  [self setLineBreakMode:NSLineBreakByTruncatingTail];

  // The overflow button chevron bitmap is not 16 units high, so it'd be scaled
  // at paint time without this.
  [self setImageScaling:NSImageScaleNone];

  // Theming doesn't work for bookmark buttons yet (cell text is chucked).
  [super setShouldTheme:NO];
}

- (BOOL)empty {
  return empty_;
}

- (void)setEmpty:(BOOL)empty {
  empty_ = empty;
  [self setShowsBorderOnlyWhileMouseInside:!empty];
}

- (NSSize)cellSizeForBounds:(NSRect)aRect {
  // There's no bezel or border so return cellSize.
  NSSize size = [self cellSize];
  size.width = std::min(aRect.size.width, size.width);
  size.height = std::min(aRect.size.height, size.height);
  return size;
}

- (void)setBookmarkCellText:(NSString*)title
                      image:(NSImage*)image {
  title = [title stringByReplacingOccurrencesOfString:@"\n"
                                           withString:@" "];
  title = [title stringByReplacingOccurrencesOfString:@"\r"
                                           withString:@" "];

  if ([title length]) {
    [self setImagePosition:NSImageLeft];
    [self setTitle:title];
  } else if ([self isFolderButtonCell]) {
    // Left-align icons for bookmarks within folders, regardless of whether
    // there is a title.
    [self setImagePosition:NSImageLeft];
  } else {
    // For bookmarks without a title that aren't visible directly in the
    // bookmarks bar, squeeze things tighter by displaying only the image.
    // By default, Cocoa leaves extra space in an attempt to display an
    // empty title.
    [self setImagePosition:NSImageOnly];
  }

  if (image)
    [self setImage:image];
}

- (void)setBookmarkNode:(const BookmarkNode*)node {
  [self setRepresentedObject:[NSValue valueWithPointer:node]];
}

- (const BookmarkNode*)bookmarkNode {
  return static_cast<const BookmarkNode*>([[self representedObject]
                                            pointerValue]);
}

- (NSMenu*)menu {
  // If node is NULL, this is a custom button, the menu does not represent
  // anything.
  const BookmarkNode* node = [self bookmarkNode];

  if (node && node->parent() &&
      node->parent()->type() == BookmarkNode::FOLDER) {
    content::RecordAction(UserMetricsAction("BookmarkBarFolder_CtxMenu"));
  } else {
    content::RecordAction(UserMetricsAction("BookmarkBar_CtxMenu"));
  }
  return [menuController_ menuForBookmarkNode:node];
}

- (void)setTitle:(NSString*)title {
  if ([[self title] isEqualTo:title])
    return;
  [super setTitle:title];
  [self applyTextColor];
}

- (void)setTextColor:(NSColor*)color {
  if ([textColor_ isEqualTo:color])
    return;
  textColor_.reset([color copy]);
  [self applyTextColor];
}

// We must reapply the text color after any setTitle: call
- (void)applyTextColor {
  base::scoped_nsobject<NSAttributedString> ats(
      [[NSAttributedString alloc] initWithString:[self title]
                                      attributes:[self titleTextAttributes]]);
  [self setAttributedTitle:ats.get()];
}

// To implement "hover open a bookmark button to open the folder"
// which feels like menus, we override NSButtonCell's mouseEntered:
// and mouseExited:, then and pass them along to our owning control.
// Note: as verified in a debugger, mouseEntered: does NOT increase
// the retainCount of the cell or its owning control.
- (void)mouseEntered:(NSEvent*)event {
  [super mouseEntered:event];
  [[self controlView] mouseEntered:event];
}

// See comment above mouseEntered:, above.
- (void)mouseExited:(NSEvent*)event {
  [[self controlView] mouseExited:event];
  [super mouseExited:event];
}

- (void)setDrawFolderArrow:(BOOL)draw {
  drawFolderArrow_ = draw;
  if (draw && !arrowImage_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    arrowImage_.reset(
        [rb.GetNativeImageNamed(IDR_MENU_HIERARCHY_ARROW).ToNSImage() retain]);
  }
}

- (NSDictionary*)titleTextAttributes {
  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [NSMutableParagraphStyle new]);
  [style setAlignment:NSNaturalTextAlignment];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  NSColor* textColor = textColor_.get();
  if (!textColor) {
    textColor = [NSColor blackColor];
  }
  if (![self isEnabled]) {
    textColor = [textColor colorWithAlphaComponent:0.5];
  }
  NSFont* theFont = [self font];
  if (!theFont) {
    theFont = [NSFont systemFontOfSize:kDefaultFontSize];
  }

  return @{
     NSFontAttributeName : theFont,
     NSForegroundColorAttributeName : textColor,
     NSParagraphStyleAttributeName : style.get(),
     NSKernAttributeName : [NSNumber numberWithFloat:0.2]
  };
}

- (NSString*)visibleTitle {
  return [self imagePosition] != NSImageOnly ? [self title] : @"";
}

// Add extra size for the arrow so it doesn't overlap the text.
// Does not sanity check to be sure this is actually a folder node.
- (NSSize)cellSize {
  NSSize cellSize = NSZeroSize;
  // Return the space needed to display the image and title, with a little
  // distance between them.
  cellSize = NSMakeSize(kIconLeftPadding + [[self image] size].width,
                        bookmarks::kBookmarkButtonHeight);
  NSString* title = [self visibleTitle];
  if ([title length] > 0) {
    CGFloat textWidth =
        [title sizeWithAttributes:[self titleTextAttributes]].width;
    cellSize.width +=
        kIconTextSpacer + std::ceil(textWidth) + kTextRightPadding;
  } else {
    // Make buttons without visible titles 20pts wide (18 plus padding).
    cellSize.width += kIconLeftPadding;
  }

  if (drawFolderArrow_) {
    cellSize.width += [arrowImage_ size].width +
                      kHierarchyButtonLeftPadding +
                      kHierarchyButtonRightPadding;
  }
  return cellSize;
}

- (NSRect)imageRectForBounds:(NSRect)theRect {
  NSRect imageRect = [super imageRectForBounds:theRect];
  // Add a little space between the image and the button's left edge, but only
  // if there's a visible title.
  imageRect.origin.y -= 1;
  imageRect.origin.x = kIconLeftPadding;
  return imageRect;
}

- (CGFloat)textStartXOffset {
  return kIconLeftPadding + [[self image] size].width + kIconTextSpacer;
}

- (void)drawFocusRingMaskWithFrame:(NSRect)cellFrame
                            inView:(NSView*)controlView {
  // We have to adjust the focus ring slightly for the chevron and regular
  // bookmark icons.
  if ([self isOffTheSideButtonCell]) {
    cellFrame.origin.y -= 2;
  } else if ([self visibleTitle].length > 0) {
    cellFrame.origin.x += 4;
  }
  if ([controlView cr_lineWidth] < 1) {
    cellFrame.origin.y -= 0.5;
  }
  [super drawFocusRingMaskWithFrame:cellFrame inView:controlView];
}

// Override cell drawing to add a submenu arrow like a real menu.
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // First draw "everything else".
  [super drawInteriorWithFrame:cellFrame inView:controlView];

  // If asked to do so, and if a folder, draw the arrow.
  if (!drawFolderArrow_)
    return;
  BookmarkButton* button = static_cast<BookmarkButton*>([self controlView]);
  DCHECK([button respondsToSelector:@selector(isFolder)]);
  if ([button isFolder]) {
    NSRect imageRect = NSZeroRect;
    imageRect.size = [arrowImage_ size];
    const CGFloat kArrowOffset = 1.0;  // Required for proper centering.
    CGFloat dX =
        NSWidth(cellFrame) - NSWidth(imageRect) - kHierarchyButtonRightPadding;
    CGFloat dY = (NSHeight(cellFrame) / 2.0) - (NSHeight(imageRect) / 2.0) +
        kArrowOffset;
    NSRect drawRect = NSOffsetRect(imageRect, dX, dY);
    [arrowImage_ drawInRect:drawRect
                    fromRect:imageRect
                   operation:NSCompositeSourceOver
                    fraction:[self isEnabled] ? 1.0 : 0.5
              respectFlipped:YES
                       hints:nil];
  }
}

- (int)verticalTextOffset {
  return -1;
}

- (CGFloat)hoverBackgroundVerticalOffsetInControlView:(NSView*)controlView {
  // In Material Design on Retina, and not in a folder menu, nudge the hover
  // background by 1px.
  const CGFloat kLineWidth = [controlView cr_lineWidth];
  if ([self isMaterialDesignButtonType] && ![self isFolderButtonCell] &&
      kLineWidth < 1) {
    return -kLineWidth;
  }
  return 0.0;
}


@end
