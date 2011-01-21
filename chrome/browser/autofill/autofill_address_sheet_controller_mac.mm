// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_sheet_controller_mac.h"

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@implementation AutoFillAddressSheetController

@synthesize addressModel = addressModel_;

- (id)initWithProfile:(const AutoFillProfile&)profile
                 mode:(AutoFillAddressMode)mode {
  NSString* nibPath = [base::mac::MainAppBundle()
                          pathForResource:@"AutoFillAddressSheet"
                                   ofType:@"nib"];
  self = [super initWithWindowNibPath:nibPath owner:self];
  if (self) {
    // Create the model.
    [self setAddressModel:[[[AutoFillAddressModel alloc]
        initWithProfile:profile] autorelease]];

    mode_ = mode;
  }
  return self;
}

- (void)dealloc {
  [addressModel_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  NSString* caption = @"";
  if (mode_ == kAutoFillAddressAddMode)
    caption = l10n_util::GetNSString(IDS_AUTOFILL_ADD_ADDRESS_CAPTION);
  else if (mode_ == kAutoFillAddressEditMode)
    caption = l10n_util::GetNSString(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION);
  else
    NOTREACHED();
  [caption_ setStringValue:caption];
}

- (IBAction)save:(id)sender {
  // Call |makeFirstResponder:| to commit pending text field edits.
  [[self window] makeFirstResponder:[self window]];

  [NSApp endSheet:[self window] returnCode:1];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window] returnCode:0];
}

- (void)copyModelToProfile:(AutoFillProfile*)profile {
  [addressModel_ copyModelToProfile:profile];
}

@end
