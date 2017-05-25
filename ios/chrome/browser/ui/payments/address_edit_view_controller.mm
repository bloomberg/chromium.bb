// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller+internal.h"
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

#pragma mark - Setters

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
  [super onCancel];

  [self.delegate addressEditViewControllerDidCancel:self];
}

- (void)onDone {
  [super onDone];

  if (![self validateForm])
    return;

  [self.delegate addressEditViewController:self
                    didFinishEditingFields:self.fields];
}

@end
