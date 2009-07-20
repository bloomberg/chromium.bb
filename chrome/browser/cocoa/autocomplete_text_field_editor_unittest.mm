// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface AutocompleteTextFieldEditorTestDelegate : NSObject {
  BOOL textShouldPaste_;
  BOOL receivedTextShouldPaste_;
}
- initWithTextShouldPaste:(BOOL)flag;
- (BOOL)receivedTextShouldPaste;
@end

namespace {
int NumTypesOnPasteboard(NSPasteboard* pb) {
  return [[pb types] count];
}

void ClearPasteboard(NSPasteboard* pb) {
  [pb declareTypes:[NSArray array] owner:nil];
}

bool NoRichTextOnClipboard(NSPasteboard* pb) {
  return ([pb dataForType:NSRTFPboardType] == nil) &&
      ([pb dataForType:NSRTFDPboardType] == nil) &&
      ([pb dataForType:NSHTMLPboardType] == nil);
}

bool ClipboardContainsText(NSPasteboard* pb, NSString* cmp) {
  NSString* clipboard_text = [pb stringForType:NSStringPboardType];
  return [clipboard_text isEqualToString:cmp];
}

class AutocompleteTextFieldEditorTest : public PlatformTest {
 public:
  AutocompleteTextFieldEditorTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    editor_.reset([[AutocompleteTextFieldEditor alloc] initWithFrame:frame]);
    [editor_ setString:@"Testing"];
    [cocoa_helper_.contentView() addSubview:editor_.get()];
  }

  virtual void SetUp() {
    PlatformTest::SetUp();
    pb_ = [NSPasteboard pasteboardWithUniqueName];
  }

  virtual void TearDown() {
    [pb_ releaseGlobally];
    pb_ = nil;
    PlatformTest::TearDown();
  }

  NSPasteboard *clipboard() {
    DCHECK(pb_);
    return pb_;
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<AutocompleteTextFieldEditor> editor_;

 private:
  NSPasteboard *pb_;
};

TEST_F(AutocompleteTextFieldEditorTest, CutCopyTest) {
  // Make sure pasteboard is empty before we start.
  ASSERT_EQ(NumTypesOnPasteboard(clipboard()), 0);

  NSString* test_string_1 = @"astring";
  NSString* test_string_2 = @"another string";

  [editor_.get() setRichText:YES];

  // Put some text on the clipboard.
  [editor_.get() setString:test_string_1];
  [editor_.get() selectAll:nil];
  [editor_.get() alignRight:nil];  // Add a rich text attribute.
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));

  // Check that copying it works and we only get plain text.
  NSPasteboard* pb = clipboard();
  [editor_.get() performCopy:pb];
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));
  ASSERT_TRUE(ClipboardContainsText(clipboard(), test_string_1));

  // Check that cutting it works and we only get plain text.
  [editor_.get() setString:test_string_2];
  [editor_.get() selectAll:nil];
  [editor_.get() alignLeft:nil];  // Add a rich text attribute.
  [editor_.get() performCut:pb];
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));
  ASSERT_TRUE(ClipboardContainsText(clipboard(), test_string_2));
  ASSERT_EQ([[editor_.get() textStorage] length], 0U);
}

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(AutocompleteTextFieldEditorTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [editor_ superview]);
  [editor_.get() removeFromSuperview];
  EXPECT_FALSE([editor_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldEditorTest, Display) {
  [editor_ display];
}

// Test that -shouldPaste properly queries the delegate.
TEST_F(AutocompleteTextFieldEditorTest, TextShouldPaste) {
  EXPECT_TRUE(![editor_ delegate]);
  EXPECT_TRUE([editor_ shouldPaste]);

  scoped_nsobject<AutocompleteTextFieldEditorTestDelegate> shouldPaste(
      [[AutocompleteTextFieldEditorTestDelegate alloc]
        initWithTextShouldPaste:YES]);
  [editor_ setDelegate:shouldPaste];
  EXPECT_FALSE([shouldPaste receivedTextShouldPaste]);
  EXPECT_TRUE([editor_ shouldPaste]);
  EXPECT_TRUE([shouldPaste receivedTextShouldPaste]);

  scoped_nsobject<AutocompleteTextFieldEditorTestDelegate> shouldNotPaste(
      [[AutocompleteTextFieldEditorTestDelegate alloc]
        initWithTextShouldPaste:NO]);
  [editor_ setDelegate:shouldNotPaste];
  EXPECT_FALSE([shouldNotPaste receivedTextShouldPaste]);
  EXPECT_FALSE([editor_ shouldPaste]);
  EXPECT_TRUE([shouldNotPaste receivedTextShouldPaste]);
}

} // namespace

@implementation AutocompleteTextFieldEditorTestDelegate

- initWithTextShouldPaste:(BOOL)flag {
  self = [super init];
  if (self) {
    textShouldPaste_ = flag;
    receivedTextShouldPaste_ = NO;
  }
  return self;
}

- (BOOL)receivedTextShouldPaste {
  return receivedTextShouldPaste_;
}

- (BOOL)textShouldPaste:(NSText*)fieldEditor {
  receivedTextShouldPaste_ = YES;
  return textShouldPaste_;
}

@end
