// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_view_controller.h"

#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupMenuViewController ()
// Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* contentContainer;
@end

@implementation PopupMenuViewController

@synthesize contentContainer = _contentContainer;
@synthesize commandHandler = _commandHandler;

#pragma mark - Public

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    [self setUpContentContainer];
    UITapGestureRecognizer* gestureRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(touchOnScrim:)];
    [self.view addGestureRecognizer:gestureRecognizer];
  }
  return self;
}

- (void)addContent:(UIViewController*)content {
  [self addChildViewController:content];
  content.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentContainer addSubview:content.view];
  AddSameConstraints(self.contentContainer, content.view);
  [content didMoveToParentViewController:self];
}

#pragma mark - Private

// Sets the content container view up.
- (void)setUpContentContainer {
  _contentContainer = [[UIView alloc] init];
  _contentContainer.backgroundColor = [UIColor whiteColor];
  _contentContainer.layer.cornerRadius = 15;
  _contentContainer.layer.shadowRadius = 10;
  _contentContainer.layer.shadowOpacity = 0.3;
  // TODO(crbug.com/821765): Add blur effect and update the shadow.
  [self.view addSubview:_contentContainer];
}

// Handler receiving the touch event on the background scrim.
- (void)touchOnScrim:(UITapGestureRecognizer*)recognizer {
  if (CGRectContainsPoint(self.contentContainer.frame,
                          [recognizer locationInView:self.view])) {
    return;
  }
  [self.commandHandler dismissPopupMenu];
}

@end
