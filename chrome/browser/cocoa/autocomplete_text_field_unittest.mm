// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface AutocompleteTextFieldTestDelegate : NSObject {
  BOOL textShouldPaste_;
  BOOL receivedTextShouldPaste_;
  BOOL receivedFlagsChanged_;
}
- initWithTextShouldPaste:(BOOL)flag;
- (BOOL)receivedTextShouldPaste;
- (BOOL)receivedFlagsChanged;
@end

namespace {

class AutocompleteTextFieldTest : public testing::Test {
 public:
  AutocompleteTextFieldTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    field_.reset([[AutocompleteTextField alloc] initWithFrame:frame]);
    [field_ setStringValue:@"Testing"];
    [cocoa_helper_.contentView() addSubview:field_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<AutocompleteTextField> field_;
};

// Test that we have the right cell class.
TEST_F(AutocompleteTextFieldTest, CellClass) {
  EXPECT_TRUE([[field_ cell] isKindOfClass:[AutocompleteTextFieldCell class]]);
}

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(AutocompleteTextFieldTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [field_ superview]);
  [field_.get() removeFromSuperview];
  EXPECT_FALSE([field_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldTest, Display) {
  [field_ display];
}

// Test that -textShouldPaste: properly queries the delegate.
TEST_F(AutocompleteTextFieldTest, TextShouldPaste) {
  EXPECT_TRUE(![field_ delegate]);
  EXPECT_TRUE([field_ textShouldPaste:nil]);

  scoped_nsobject<AutocompleteTextFieldTestDelegate> shouldPaste(
      [[AutocompleteTextFieldTestDelegate alloc] initWithTextShouldPaste:YES]);
  [field_ setDelegate:shouldPaste];
  EXPECT_FALSE([shouldPaste receivedTextShouldPaste]);
  EXPECT_TRUE([field_ textShouldPaste:nil]);
  EXPECT_TRUE([shouldPaste receivedTextShouldPaste]);

  scoped_nsobject<AutocompleteTextFieldTestDelegate> shouldNotPaste(
      [[AutocompleteTextFieldTestDelegate alloc] initWithTextShouldPaste:NO]);
  [field_ setDelegate:shouldNotPaste];
  EXPECT_FALSE([shouldNotPaste receivedTextShouldPaste]);
  EXPECT_FALSE([field_ textShouldPaste:nil]);
  EXPECT_TRUE([shouldNotPaste receivedTextShouldPaste]);
}

// Test that -control:flagsChanged: properly reaches the delegate.
TEST_F(AutocompleteTextFieldTest, FlagsChanged) {
  EXPECT_TRUE(![field_ delegate]);

  // This shouldn't crash, at least.
  [field_ flagsChanged:nil];

  scoped_nsobject<AutocompleteTextFieldTestDelegate> delegate(
      [[AutocompleteTextFieldTestDelegate alloc] initWithTextShouldPaste:NO]);
  [field_ setDelegate:delegate];
  EXPECT_FALSE([delegate receivedFlagsChanged]);
  [field_ flagsChanged:nil];
  EXPECT_TRUE([delegate receivedFlagsChanged]);
}

// Test that -control:flagsChanged: properly reaches the delegate when
// the -flagsChanged: message goes to the editor.  In that case it is
// forwarded via the responder chain.
TEST_F(AutocompleteTextFieldTest, FieldEditorFlagsChanged) {
  EXPECT_TRUE(![field_ delegate]);

  scoped_nsobject<AutocompleteTextFieldTestDelegate> delegate(
      [[AutocompleteTextFieldTestDelegate alloc] initWithTextShouldPaste:NO]);

  // Setup a field editor for |field_|.
  scoped_nsobject<AutocompleteTextFieldEditor> editor(
      [[AutocompleteTextFieldEditor alloc] init]);
  [field_ setDelegate:delegate];

  [editor setFieldEditor:YES];
  [[field_ cell] setUpFieldEditorAttributes:editor];
  [[field_ cell] editWithFrame:[field_ bounds]
                        inView:field_
                        editor:editor
                      delegate:[field_ delegate]
                         event:nil];
  EXPECT_FALSE([delegate receivedFlagsChanged]);
  [editor flagsChanged:nil];
  EXPECT_TRUE([delegate receivedFlagsChanged]);
}

}  // namespace

@implementation AutocompleteTextFieldTestDelegate

- initWithTextShouldPaste:(BOOL)flag {
  self = [super init];
  if (self) {
    textShouldPaste_ = flag;
    receivedTextShouldPaste_ = NO;
    receivedFlagsChanged_ = NO;
  }
  return self;
}

- (BOOL)receivedTextShouldPaste {
  return receivedTextShouldPaste_;
}

- (BOOL)receivedFlagsChanged {
  return receivedFlagsChanged_;
}

- (BOOL)control:(NSControl*)control textShouldPaste:(NSText*)fieldEditor {
  receivedTextShouldPaste_ = YES;
  return textShouldPaste_;
}

- (void)control:(id)control flagsChanged:(NSEvent*)theEvent {
  receivedFlagsChanged_ = YES;
}

@end
