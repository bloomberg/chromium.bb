// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller_container.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the toolbar.
const int kToolbarHeight = 48;
}

@interface ReadingListViewControllerContainer ()<ReadingListToolbarActions> {
  // Toolbar with the actions.
  ReadingListToolbar* _toolbar;
  ReadingListViewController* _collectionController;
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
  constraint.priority = UILayoutPriorityDefaultHigh;
  constraint.active = YES;
}

#pragma mark - ReadingListViewControllerDelegate

- (void)setCollectionHasItems:(BOOL)hasItems {
  if (hasItems) {
    // If there are items, add the toolbar.
    [self.view addSubview:_toolbar];
    NSDictionary* views = @{
      @"toolbar" : _toolbar,
      @"collection" : [_collectionController view]
    };
    NSDictionary* metrics = @{ @"toolbarHeight" : @(kToolbarHeight) };
    NSArray* constraints =
        @[ @"V:[collection][toolbar(==toolbarHeight)]|", @"H:|[toolbar]|" ];
    ApplyVisualConstraintsWithMetrics(constraints, views, metrics);
  } else {
    // If there is no item, remove the toolbar. The constraints will make sure
    // the collection takes the whole view.
    [_toolbar removeFromSuperview];
  }
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

@end
