// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"

#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_edit_view.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarViewController ()
// Location bar view that contains the omnibox and leading/trailing buttons.
@property(nonatomic, strong) LocationBarEditView* locationBarEditView;

// The view that displays current location when the omnibox is not focused.
@property(nonatomic, strong) LocationBarSteadyView* locationBarSteadyView;
@end

@implementation LocationBarViewController
@synthesize locationBarEditView = _locationBarEditView;
@synthesize locationBarSteadyView = _locationBarSteadyView;
@synthesize incognito = _incognito;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;

#pragma mark - public

- (instancetype)initWithFrame:(CGRect)frame
                         font:(UIFont*)font
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor {
  self = [super init];
  if (self) {
    _locationBarEditView =
        [[LocationBarEditView alloc] initWithFrame:frame
                                              font:font
                                         textColor:textColor
                                         tintColor:tintColor];
    _locationBarSteadyView = [[LocationBarSteadyView alloc] init];
    [_locationBarSteadyView addTarget:self
                               action:@selector(locationBarSteadyViewTapped)
                     forControlEvents:UIControlEventTouchUpInside];
  }
  return self;
}

- (void)switchToEditing:(BOOL)editing {
  self.locationBarEditView.hidden = !editing;
  self.locationBarSteadyView.hidden = editing;
}

- (OmniboxTextFieldIOS*)textField {
  return self.locationBarEditView.textField;
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.locationBarEditView.incognito = incognito;
}

- (void)setDispatcher:(id<ActivityServiceCommands>)dispatcher {
  _dispatcher = dispatcher;

  [self.locationBarSteadyView.trailingButton
             addTarget:dispatcher
                action:@selector(sharePage)
      forControlEvents:UIControlEventTouchUpInside];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.view addSubview:self.locationBarEditView];
  self.locationBarEditView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.locationBarEditView, self.view);

  [self.view addSubview:self.locationBarSteadyView];
  self.locationBarSteadyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.locationBarSteadyView, self.view);

  [self switchToEditing:NO];
}

#pragma mark - LocationBarConsumer

- (void)updateLocationText:(NSString*)text {
  self.locationBarSteadyView.locationLabel.text = text;
}

- (void)updateLocationIcon:(UIImage*)icon {
  self.locationBarSteadyView.locationIconImageView.image = icon;
  self.locationBarSteadyView.locationIconImageView.tintColor =
      self.incognito ? [UIColor whiteColor] : [UIColor blackColor];
}

#pragma mark - private

- (void)locationBarSteadyViewTapped {
  [self.delegate locationBarSteadyViewTapped];
}

@end
