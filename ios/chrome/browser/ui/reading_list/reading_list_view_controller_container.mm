// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller_container.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the toolbar in normal state.
const int kToolbarNormalHeight = 48;
// Height of the expanded toolbar (buttons on multiple lines).
const int kToolbarExpandedHeight = 58;

typedef NS_ENUM(NSInteger, LayoutPriority) {
  LayoutPriorityLow = 750,
  LayoutPriorityHigh = 751
};
}

@interface ReadingListViewControllerContainer ()<
    ReadingListToolbarActions,
    ReadingListToolbarHeightDelegate> {
  // Toolbar with the actions.
  ReadingListToolbar* _toolbar;
  ReadingListViewController* _collectionController;
  // This constraint control the expanded mode of the toolbar.
  NSLayoutConstraint* _expandedToolbarConstraint;
}

@end

@implementation ReadingListViewControllerContainer

- (instancetype)initWithModel:(ReadingListModel*)model
                      tabModel:(TabModel*)tabModel
              largeIconService:(favicon::LargeIconService*)largeIconService
    readingListDownloadService:
        (ReadingListDownloadService*)readingListDownloadService {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _toolbar = [[ReadingListToolbar alloc] initWithFrame:CGRectZero];
    _toolbar.heightDelegate = self;
    _collectionController = [[ReadingListViewController alloc]
                     initWithModel:model
                          tabModel:tabModel
                  largeIconService:largeIconService
        readingListDownloadService:readingListDownloadService
                           toolbar:_toolbar];
    _collectionController.audience = self;

    // Configure modal presentation.
    [self setModalPresentationStyle:UIModalPresentationFormSheet];
    [self setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  }
  return self;
}

- (void)viewDidLoad {
  [self addChildViewController:_collectionController];
  [self.view addSubview:[_collectionController view]];
  [_collectionController didMoveToParentViewController:self];

  [_toolbar setTranslatesAutoresizingMaskIntoConstraints:NO];
  [[_collectionController view]
      setTranslatesAutoresizingMaskIntoConstraints:NO];

  NSDictionary* views = @{ @"collection" : [_collectionController view] };
  NSArray* constraints = @[ @"V:|[collection]", @"H:|[collection]|" ];
  ApplyVisualConstraints(constraints, views);

  // This constraints is not required. It will be activated only when the
  // toolbar is not present, allowing the collection to take the whole page.
  NSLayoutConstraint* constraint = [[_collectionController view].bottomAnchor
      constraintEqualToAnchor:[self view].bottomAnchor];
  constraint.priority = LayoutPriorityLow;
  constraint.active = YES;
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

#pragma mark UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self dismiss];
  return YES;
}

#pragma mark - ReadingListViewControllerAudience

- (void)setCollectionHasItems:(BOOL)hasItems {
  if (hasItems) {
    // If there are items, add the toolbar.
    [self.view addSubview:_toolbar];
    NSDictionary* views = @{
      @"toolbar" : _toolbar,
      @"collection" : [_collectionController view]
    };
    NSArray* constraints = @[ @"V:[collection][toolbar]|", @"H:|[toolbar]|" ];
    ApplyVisualConstraints(constraints, views);
    NSLayoutConstraint* height =
        [_toolbar.heightAnchor constraintEqualToConstant:kToolbarNormalHeight];
    height.priority = LayoutPriorityHigh;
    height.active = YES;
    // When the toolbar is added, the only button is the "edit" button. No need
    // to go in expanded mode.
    _expandedToolbarConstraint = [_toolbar.heightAnchor
        constraintEqualToConstant:kToolbarExpandedHeight];
  } else {
    // If there is no item, remove the toolbar. The constraints will make sure
    // the collection takes the whole view.
    [_toolbar removeFromSuperview];
  }
}

- (void)dismiss {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

#pragma mark - ReadingListToolbarActionTarget

- (void)markPressed {
  [_collectionController markPressed];
}

- (void)deletePressed {
  [_collectionController deletePressed];
}

- (void)enterEditingModePressed {
  [_collectionController enterEditingModePressed];
}

- (void)exitEditingModePressed {
  [_collectionController exitEditingModePressed];
}

#pragma mark - ReadingListToolbarHeightDelegate

- (void)toolbar:(id)toolbar onHeightChanged:(ReadingListToolbarHeight)height {
  dispatch_async(dispatch_get_main_queue(), ^{
    switch (height) {
      case NormalHeight:
        _expandedToolbarConstraint.active = NO;
        break;

      case ExpandedHeight:
        _expandedToolbarConstraint.active = YES;
        break;
    }
  });
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  __weak ReadingListViewControllerContainer* weakSelf = self;
  return @[ [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                   modifierFlags:Cr_UIKeyModifierNone
                                           title:nil
                                          action:^{
                                            [weakSelf dismiss];
                                          }] ];
}

@end
