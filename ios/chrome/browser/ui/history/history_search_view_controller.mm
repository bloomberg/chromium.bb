// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_search_view_controller.h"

#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/history/history_search_view.h"

@interface HistorySearchViewController ()<UITextFieldDelegate> {
  // Delegate for forwarding interactions with the search view.
  base::WeakNSProtocol<id<HistorySearchViewControllerDelegate>> _delegate;
  // View displayed by the HistorySearchViewController
  base::scoped_nsobject<HistorySearchView> _searchView;
}

// Action for the cancel button.
- (void)cancelButtonClicked:(id)sender;

@end

@implementation HistorySearchViewController

@synthesize enabled = _enabled;

- (void)loadView {
  _searchView.reset([[HistorySearchView alloc] init]);
  [_searchView setSearchBarDelegate:self];
  [_searchView setCancelTarget:self action:@selector(cancelButtonClicked:)];
  self.view = _searchView;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_searchView becomeFirstResponder];
}

- (void)setDelegate:(id<HistorySearchViewControllerDelegate>)delegate {
  _delegate.reset(delegate);
}

- (id<HistorySearchViewControllerDelegate>)delegate {
  return _delegate;
}

- (void)setEnabled:(BOOL)enabled {
  _enabled = enabled;
  [_searchView setEnabled:enabled];
}

- (void)cancelButtonClicked:(id)sender {
  [_searchView clearText];
  [_searchView endEditing:YES];
  [self.delegate historySearchViewControllerDidCancel:self];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  NSMutableString* text = [NSMutableString stringWithString:[textField text]];
  [text replaceCharactersInRange:range withString:string];
  [self.delegate historySearchViewController:self didRequestSearchForTerm:text];
  return YES;
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  [self.delegate historySearchViewController:self didRequestSearchForTerm:@""];
  return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

@end
