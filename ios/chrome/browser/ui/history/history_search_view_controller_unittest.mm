// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_search_view_controller.h"

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/history/history_search_view.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// HistorySearchView category to expose the text field and cancel button.
@interface HistorySearchView (Testing)
@property(nonatomic, strong) UITextField* textField;
@property(nonatomic, strong) UIButton* cancelButton;
@end

// Test fixture for HistorySearchViewController.
class HistorySearchViewControllerTest : public PlatformTest {
 public:
  HistorySearchViewControllerTest() {
    search_view_controller_.reset([[HistorySearchViewController alloc] init]);
    [search_view_controller_ loadView];
    mock_delegate_.reset([[OCMockObject
        mockForProtocol:@protocol(HistorySearchViewControllerDelegate)]
        retain]);
    [search_view_controller_ setDelegate:mock_delegate_];
  }

 protected:
  base::scoped_nsobject<HistorySearchViewController> search_view_controller_;
  base::scoped_nsprotocol<id<HistorySearchViewControllerDelegate>>
      mock_delegate_;
};

// Test that pressing the cancel button invokes delegate callback to cancel
// search.
TEST_F(HistorySearchViewControllerTest, CancelButtonPressed) {
  UIButton* cancel_button = base::mac::ObjCCastStrict<HistorySearchView>(
                                search_view_controller_.get().view)
                                .cancelButton;
  OCMockObject* mock_delegate = (OCMockObject*)mock_delegate_.get();
  [[mock_delegate expect]
      historySearchViewControllerDidCancel:search_view_controller_];
  [cancel_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Test that invocation of
// textField:shouldChangeCharactersInRange:replacementString: on the text field
// delegate results invokes delegate callback to request search.
TEST_F(HistorySearchViewControllerTest, SearchButtonPressed) {
  UITextField* text_field = base::mac::ObjCCastStrict<HistorySearchView>(
                                search_view_controller_.get().view)
                                .textField;
  OCMockObject* mock_delegate = (OCMockObject*)mock_delegate_.get();
  [[mock_delegate expect] historySearchViewController:search_view_controller_
                              didRequestSearchForTerm:@"a"];
  [text_field.delegate textField:text_field
      shouldChangeCharactersInRange:NSMakeRange(0, 0)
                  replacementString:@"a"];
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Test that disabling HistorySearchViewController disables the search view text
// field.
TEST_F(HistorySearchViewControllerTest, DisableSearchBar) {
  UITextField* text_field = base::mac::ObjCCastStrict<HistorySearchView>(
                                search_view_controller_.get().view)
                                .textField;
  DCHECK(text_field);
  EXPECT_TRUE(text_field.enabled);

  search_view_controller_.get().enabled = NO;
  EXPECT_FALSE(text_field.enabled);

  search_view_controller_.get().enabled = YES;
  EXPECT_TRUE(text_field.enabled);
}
