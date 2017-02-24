// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

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

@interface ReadingListViewController ()<
    ReadingListToolbarActions,
    ReadingListToolbarHeightDelegate,
    ReadingListCollectionViewControllerAudience> {
  // This constraint control the expanded mode of the toolbar.
  NSLayoutConstraint* _expandedToolbarConstraint;
}

@property(nonatomic, strong, readonly)
    ReadingListCollectionViewController* readingListCollectionViewController;
@property(nonatomic, strong, readonly) ReadingListToolbar* toolbar;

@end

@implementation ReadingListViewController

@synthesize delegate = _delegate;
@synthesize readingListCollectionViewController =
    _readingListCollectionViewController;
@synthesize toolbar = _toolbar;

- (instancetype)initWithCollectionViewController:
                    (ReadingListCollectionViewController*)
                        collectionViewController
                                         toolbar:(ReadingListToolbar*)toolbar {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _toolbar = toolbar;
    toolbar.heightDelegate = self;
    _readingListCollectionViewController = collectionViewController;
    collectionViewController.audience = self;

    // Configure modal presentation.
    [self setModalPresentationStyle:UIModalPresentationFormSheet];
    [self setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  }
  return self;
}

- (void)viewDidLoad {
  [self addChildViewController:self.readingListCollectionViewController];
  [self.view addSubview:self.readingListCollectionViewController.view];
  [self.readingListCollectionViewController didMoveToParentViewController:self];

  [_toolbar setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.readingListCollectionViewController.view
      setTranslatesAutoresizingMaskIntoConstraints:NO];

  NSDictionary* views =
      @{ @"collection" : self.readingListCollectionViewController.view };
  NSArray* constraints = @[ @"V:|[collection]", @"H:|[collection]|" ];
  ApplyVisualConstraints(constraints, views);

  // This constraint has a low priority so it will only take effect when the
  // toolbar isn't present, allowing the collection to take the whole page.
  NSLayoutConstraint* constraint =
      [self.readingListCollectionViewController.view.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor];
  constraint.priority = LayoutPriorityLow;
  constraint.active = YES;
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

#pragma mark UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self.delegate dismissReadingListCollectionViewController:
                     self.readingListCollectionViewController];
  return YES;
}

#pragma mark - ReadingListToolbarActionTarget

- (void)markPressed {
  [self.readingListCollectionViewController markPressed];
}

- (void)deletePressed {
  [self.readingListCollectionViewController deletePressed];
}

- (void)enterEditingModePressed {
  [self.readingListCollectionViewController enterEditingModePressed];
}

- (void)exitEditingModePressed {
  [self.readingListCollectionViewController exitEditingModePressed];
}

#pragma mark - ReadingListCollectionViewControllerAudience

- (void)readingListHasItems:(BOOL)hasItems {
  if (hasItems) {
    // If there are items, add the toolbar.
    [self.view addSubview:_toolbar];
    NSDictionary* views = @{
      @"toolbar" : _toolbar,
      @"collection" : self.readingListCollectionViewController.view
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

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  __weak ReadingListViewController* weakSelf = self;
  return @[ [UIKeyCommand
      cr_keyCommandWithInput:UIKeyInputEscape
               modifierFlags:Cr_UIKeyModifierNone
                       title:nil
                      action:^{
                        [weakSelf.delegate
                            dismissReadingListCollectionViewController:
                                weakSelf.readingListCollectionViewController];
                      }] ];
}

@end
