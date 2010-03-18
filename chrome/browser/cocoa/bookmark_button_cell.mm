// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#import "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_menu.h"

@implementation BookmarkButtonCell

- (id)initTextCell:(NSString*)string {
  if ((self = [super initTextCell:string])) {
    [self setButtonType:NSMomentaryPushInButton];
    [self setBezelStyle:NSShadowlessSquareBezelStyle];
    [self setShowsBorderOnlyWhileMouseInside:YES];
    [self setControlSize:NSSmallControlSize];
    [self setAlignment:NSLeftTextAlignment];
    [self setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [self setWraps:NO];
    // NSLineBreakByTruncatingMiddle seems more common on OSX but let's
    // try to match Windows for a bit to see what happens.
    [self setLineBreakMode:NSLineBreakByTruncatingTail];

    // Theming doesn't work for bookmark buttons yet (text chucked).
    [super setShouldTheme:NO];

  }
  return self;
}

- (BOOL)empty {
  return empty_;
}

- (void)setEmpty:(BOOL)empty {
  empty_ = empty;
  [self setShowsBorderOnlyWhileMouseInside:!empty];
}

- (NSSize)cellSizeForBounds:(NSRect)aRect {
  NSSize size = [super cellSizeForBounds:aRect];
  size.width += 2;
  size.height += 4;
  return size;
}

- (void)setBookmarkCellText:(NSString*)title
                      image:(NSImage*)image {
  title = [title stringByReplacingOccurrencesOfString:@"\n"
                                           withString:@" "];
  title = [title stringByReplacingOccurrencesOfString:@"\r"
                                           withString:@" "];
  // Center the image if we have a title, or if there already was a
  // title set.
  BOOL hasTitle = (([title length] > 0) ||
                   ([[self title] length] > 0));
  if (image) {
    [self setImage:image];
    if (hasTitle) {
      [self setImagePosition:NSImageLeft];
    } else {
      [self setImagePosition:NSImageOnly];
    }
  }

  if (title)
    [self setTitle:title];
}

- (void)setBookmarkNode:(const BookmarkNode*)node {
  [self setRepresentedObject:[NSValue valueWithPointer:node]];
}

- (const BookmarkNode*)bookmarkNode {
  return static_cast<const BookmarkNode*>([[self representedObject]
                                            pointerValue]);
}

// We share the context menu among all bookmark buttons.  To allow us
// to disambiguate when needed (e.g. "open bookmark"), we set the
// menu's associated bookmark node ID to be our represented object.
- (NSMenu*)menu {
  if (empty_)
    return nil;
  BookmarkMenu* menu = (BookmarkMenu*)[super menu];
  const BookmarkNode* node =
      static_cast<const BookmarkNode*>([[self representedObject] pointerValue]);
  [menu setRepresentedObject:[NSNumber numberWithLongLong:node->id()]];
  return menu;
}

// Unfortunately, NSCell doesn't already have something like this.
// TODO(jrg): consider placing in GTM.
- (void)setTextColor:(NSColor*)color {

  // We can't properly set the cell's text color without a control.
  // In theory we could just save the next for later and wait until
  // the cell is moved to a control, but there is no obvious way to
  // accomplish that (e.g. no "cellDidMoveToControl" notification.)
  DCHECK([self controlView]);

  scoped_nsobject<NSMutableParagraphStyle> style([NSMutableParagraphStyle new]);
  [style setAlignment:NSLeftTextAlignment];
  NSDictionary* dict = [NSDictionary
                         dictionaryWithObjectsAndKeys:color,
                         NSForegroundColorAttributeName,
                         [self font], NSFontAttributeName,
                         style.get(), NSParagraphStyleAttributeName,
                         nil];
  scoped_nsobject<NSAttributedString> ats([[NSAttributedString alloc]
                                            initWithString:[self title]
                                                attributes:dict]);
  NSButton* button = static_cast<NSButton*>([self controlView]);
  if (button) {
    DCHECK([button isKindOfClass:[NSButton class]]);
    [button setAttributedTitle:ats.get()];
  }
}

// To implement "hover open a bookmark button to open the folder"
// which feels like menus, we override NSButtonCell's mouseEntered:
// and mouseExited:, then and pass them along to our owning control.
- (void)mouseEntered:(NSEvent*)event {
  [super mouseEntered:event];
  [[self controlView] mouseEntered:event];
}

// See comment above mouseEntered:, above.
- (void)mouseExited:(NSEvent*)event {
  [super mouseExited:event];
  [[self controlView] mouseExited:event];
}

@end
