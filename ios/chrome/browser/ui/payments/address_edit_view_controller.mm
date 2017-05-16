// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_actions.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kAddressEditCollectionViewAccessibilityID =
    @"kAddressEditCollectionViewAccessibilityID";

}  // namespace

@interface AddressEditViewController ()

// The list of field definitions for the editor.
@property(nonatomic, weak) NSArray<EditorField*>* fields;

@end

@implementation AddressEditViewController

@synthesize delegate = _delegate;
@synthesize fields = _fields;

#pragma mark - Initialization

- (instancetype)init {
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    // Set up leading (cancel) button.
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIBarButtonItemStylePlain
               target:nil
               action:@selector(onCancel)];
    [cancelButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                                forState:UIControlStateDisabled];
    [cancelButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_CANCEL)];
    [self navigationItem].leftBarButtonItem = cancelButton;

    // Set up trailing (done) button.
    UIBarButtonItem* doneButton =
        [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_DONE)
                                         style:UIBarButtonItemStylePlain
                                        target:nil
                                        action:@selector(onDone)];
    [doneButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor lightGrayColor]
    }
                              forState:UIControlStateDisabled];
    [doneButton setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_DONE)];
    [self navigationItem].rightBarButtonItem = doneButton;
  }

  return self;
}

- (void)setDelegate:(id<AddressEditViewControllerDelegate>)delegate {
  [super setDelegate:delegate];
  _delegate = delegate;
}

#pragma mark - CollectionViewController methods

- (void)viewDidLoad {
  [super viewDidLoad];

  self.collectionView.accessibilityIdentifier =
      kAddressEditCollectionViewAccessibilityID;
}

#pragma mark - PaymentRequestEditViewControllerActions methods

- (void)onCancel {
  [self.delegate addressEditViewControllerDidCancel:self];
}

- (void)onDone {
  [self.delegate addressEditViewController:self
                    didFinishEditingFields:self.fields];
}

@end
