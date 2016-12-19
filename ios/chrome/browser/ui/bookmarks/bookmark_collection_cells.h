// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_CELLS_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_CELLS_H_

#import <UIKit/UIKit.h>

namespace bookmark_cell {
// The types get cached, which means that their values must not change.
typedef enum {
  ButtonNone = 0,  // No button.
  ButtonMenu,      // 3-vertical-dots button.
} ButtonType;

}  // bookmark_cell

// Views that expect to display an image associated with a bookmark must
// implement this protocol.
// Since the image fetching API is asynchronous and uncancellable, views must
// store some metadata associated with the image to ensure irrelevant callbacks
// are discarded.
@protocol BookmarkImageableView
@property(nonatomic, assign) BOOL shouldAnimateImageChanges;
// Sets the image on the view, animating the change if
// shouldAnimateImageChanges is YES. Hides the overlay placeholder text.
- (void)setImage:(UIImage*)image;
// Sets the placeholder text that is displayed over the image view. Hides the
// image.
- (void)setPlaceholderText:(NSString*)text
                 textColor:(UIColor*)textColor
           backgroundColor:(UIColor*)backgroundColor;
@end

#pragma mark - Base Classes For Both Device Types

// Abstract base class for cells in the bookmark collection view.
// Most controllers that use this cell have an "edit" mode that allows users to
// select multiple bookmarks. When a cell is selected, a translucent overlay
// is layered on top to change the look of the view.
// Subclasses should insert new views below the "highlightCover" property.
// There is also an image and an optional menu button.
@interface BookmarkCell : UICollectionViewCell<BookmarkImageableView>

@property(nonatomic, retain, readonly) UILabel* titleLabel;

+ (NSString*)reuseIdentifier;

// Sets the target/selector for the top-right corner button.
// |action| must take exactly 2 arguments.
// The first object passed to |action| will be of type BookmarkItemCell.
// The second will be the view that was tapped on to trigger the action.
- (void)setButtonTarget:(id)target action:(SEL)action;

// Changes the appearance of the button.
- (void)showButtonOfType:(bookmark_cell::ButtonType)buttonType
                animated:(BOOL)animated;

// The cell has been selected by the user in editing mode.
- (void)setSelectedForEditing:(BOOL)selected animated:(BOOL)animated;

// Sets the title.
- (void)updateWithTitle:(NSString*)title;

@end

#pragma mark - Specialized Cells

// Specialized cell for bookmark urls.
// It is intended to show the title, domain, and parent folder of the bookmark.
@interface BookmarkItemCell : BookmarkCell
// Returns the icon size that is preferred for this cell. Icons are square, and
// the returned value is the side of the square in points.
+ (CGFloat)preferredImageSize;
@end

// Specialized cell for bookmark folders. Uses a default folder image.
@interface BookmarkFolderCell : BookmarkCell
@end

#pragma mark - Header Views

// Standard header view for a section.
@interface BookmarkHeaderView : UICollectionReusableView
+ (NSString*)reuseIdentifier;
+ (CGFloat)handsetHeight;
- (void)setTitle:(NSString*)title;
@end

// Blank white header with thin separator line in the bottom.
@interface BookmarkHeaderSeparatorView : UICollectionReusableView
+ (NSString*)reuseIdentifier;
+ (CGFloat)preferredHeight;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_COLLECTION_CELLS_H_
