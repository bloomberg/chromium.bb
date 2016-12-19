// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_view.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"

@implementation NewTabPageView {
 @private
  // The objects pointed to by |tabBar_| and |scrollView_| are owned as
  // subviews already.
  __unsafe_unretained NewTabPageBar* tabBar_;     // weak
  __unsafe_unretained UIScrollView* scrollView_;  // weak

  base::mac::ObjCPropertyReleaser propertyReleaser_NewTabPageView_;
}

@synthesize scrollView = scrollView_;
@synthesize tabBar = tabBar_;

- (instancetype)initWithFrame:(CGRect)frame
                andScrollView:(UIScrollView*)scrollView
                    andTabBar:(NewTabPageBar*)tabBar {
  self = [super initWithFrame:frame];
  if (self) {
    propertyReleaser_NewTabPageView_.Init(self, [NewTabPageView class]);
    [self addSubview:scrollView];
    [self addSubview:tabBar];
    scrollView_ = scrollView;
    tabBar_ = tabBar;
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)setFrame:(CGRect)frame {
  // When transitioning the iPhone xib to an iPad idiom, the setFrame call below
  // can sometimes fire a scrollViewDidScroll event which changes the
  // selectedIndex underneath us.  Save the selected index and remove the
  // delegate so scrollViewDidScroll isn't called. Then fix the scrollView
  // offset after updating the frame.
  NSUInteger selectedIndex = self.tabBar.selectedIndex;
  id<UIScrollViewDelegate> delegate = self.scrollView.delegate;
  self.scrollView.delegate = nil;
  [super setFrame:frame];
  self.scrollView.delegate = delegate;

  // Set the scrollView content size.
  [self updateScrollViewContentSize];

  // Set the frame of the laid out NTP panels on iPad.
  if (IsIPadIdiom()) {
    NSUInteger index = 0;
    CGFloat selectedItemXOffset = 0;
    for (NewTabPageBarItem* item in self.tabBar.items) {
      item.view.frame = [self panelFrameForItemAtIndex:index];
      if (index == selectedIndex)
        selectedItemXOffset = CGRectGetMinX(item.view.frame);
      index++;
    }

    // Ensure the selected NTP panel is lined up correctly when the frame
    // changes.
    CGPoint point = CGPointMake(selectedItemXOffset, 0);
    [self.scrollView setContentOffset:point animated:NO];
  }

  // Trigger a layout.  The |-layoutIfNeeded| call is required because sometimes
  // |-layoutSubviews| is not successfully triggered when |-setNeedsLayout| is
  // called after frame changes due to autoresizing masks.
  [self setNeedsLayout];
  [self layoutIfNeeded];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  self.tabBar.hidden = !self.tabBar.items.count;
  if (self.tabBar.hidden) {
    self.scrollView.frame = self.bounds;
  } else {
    CGSize barSize = [self.tabBar sizeThatFits:self.bounds.size];
    self.tabBar.frame = CGRectMake(CGRectGetMinX(self.bounds),
                                   CGRectGetMaxY(self.bounds) - barSize.height,
                                   barSize.width, barSize.height);
    self.scrollView.frame = CGRectMake(
        CGRectGetMinX(self.bounds), CGRectGetMinY(self.bounds),
        CGRectGetWidth(self.bounds), CGRectGetMinY(self.tabBar.frame));
  }
}

- (void)updateScrollViewContentSize {
  CGSize contentSize = self.scrollView.bounds.size;
  // On iPhone, NTP doesn't scroll horizontally, as alternate panels are shown
  // modally. On iPad, panels are laid out side by side in the scroll view.
  if (IsIPadIdiom()) {
    contentSize.width *= self.tabBar.items.count;
  }
  self.scrollView.contentSize = contentSize;
}

- (CGRect)panelFrameForItemAtIndex:(NSUInteger)index {
  CGRect contentBounds = CGRectMake(0, 0, self.scrollView.contentSize.width,
                                    self.scrollView.contentSize.height);
  LayoutRect layout =
      LayoutRectForRectInBoundingRect(self.scrollView.bounds, contentBounds);
  layout.position.leading = layout.size.width * index;
  return LayoutRectGetRect(layout);
}

@end
