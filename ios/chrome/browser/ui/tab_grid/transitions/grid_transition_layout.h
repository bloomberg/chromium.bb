// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TRANSITION_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TRANSITION_LAYOUT_H_

#import <UIKit/UIKit.h>

@class GridTransitionLayoutItem;

// An encapsulation of information for the layout of a grid of cells that will
// be used in an animated transition. The layout object is composed of layout
// items (see below).
@interface GridTransitionLayout : NSObject

// All of the items in the layout.
@property(nonatomic, copy, readonly) NSArray<GridTransitionLayoutItem*>* items;
// The item in the layout (if any) that's the 'active' item (the one that will
// expand and contract).
// Note that |activeItem.cell.selected| doesn't need to be YES; the transition
// animation may set or unset that selection state as part of the animation.
@property(nonatomic, strong, readonly) GridTransitionLayoutItem* activeItem;

// An item that may be used to *only* show the selection state.
// |selectionItem| is not one of the items in |items|.
@property(nonatomic, strong, readonly) GridTransitionLayoutItem* selectionItem;

// The rect, in UIWindow coordinates, that an "expanded" item should occupy.
@property(nonatomic) CGRect expandedRect;

// Creates a new layout object with |items|, and |activeItem| selected.
// |items| should be non-nil, but it may be empty.
// |activeItem| must either be nil, or one of the members of |items|.
+ (instancetype)layoutWithItems:(NSArray<GridTransitionLayoutItem*>*)items
                     activeItem:(GridTransitionLayoutItem*)activeItem
                  selectionItem:(GridTransitionLayoutItem*)selectionItem;

@end

// An encapsulation of information for the layout of a single grid cell, in the
// form of UICollectionView classes.
@interface GridTransitionLayoutItem : NSObject

// A cell object with the desired appearance for the animation. This should
// correspond to an actual cell in the collection view involved in the trans-
// ition, but the value of thie property should not be in any view hierarchy
// when the layout item is created.
@property(nonatomic, strong, readonly) UICollectionViewCell* cell;

// An auxillary view in |cell|'s view hierarchy that may also be animated.
@property(nonatomic, weak, readonly) UIView* auxillaryView;

// The layout attributes for the cell in the collection view, normalized to
// UIWindow coordinates. It's the responsibility of the setter to do this
// normalization.
@property(nonatomic, strong, readonly)
    UICollectionViewLayoutAttributes* attributes;

// Creates a new layout item instance will |cell| and |attributes|, neither of
// which can be nil.
// It's an error if |cell| has a superview.
// It's an error if |auxillaryView| is not a subview of |cell|; it may be nil.
// The properties (size, etc) of |attributes| don't need to match the corres-
// ponding properties of |cell| when the item is created.
+ (instancetype)itemWithCell:(UICollectionViewCell*)cell
               auxillaryView:(UIView*)auxillaryView
                  attributes:(UICollectionViewLayoutAttributes*)attributes;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TRANSITION_LAYOUT_H_
