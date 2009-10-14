// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_editor.h"

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#import "chrome/browser/cocoa/autocomplete_text_field_unittest_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::testing::Return;

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

// TODO(shess): Very similar to AutocompleteTextFieldTest.  Maybe
// those can be shared.

class AutocompleteTextFieldEditorTest : public PlatformTest {
 public:
  AutocompleteTextFieldEditorTest()
      : pb_([NSPasteboard pasteboardWithUniqueName]) {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    field_.reset([[AutocompleteTextField alloc] initWithFrame:frame]);
    [field_ setStringValue:@"Testing"];
    [field_ setObserver:&field_observer_];
    [cocoa_helper_.contentView() addSubview:field_.get()];

    // Arrange for |field_| to get the right field editor.
    window_delegate_.reset(
        [[AutocompleteTextFieldWindowTestDelegate alloc] init]);
    [cocoa_helper_.window() setDelegate:window_delegate_.get()];

    // Get the field editor setup.
    cocoa_helper_.makeFirstResponder(field_);
    id editor = [field_.get() currentEditor];
    editor_.reset([static_cast<AutocompleteTextFieldEditor*>(editor) retain]);
  }

  virtual void SetUp() {
    EXPECT_TRUE(editor_.get() != nil);
    EXPECT_TRUE(
        [editor_.get() isKindOfClass:[AutocompleteTextFieldEditor class]]);
  }

  // The removeFromSuperview call is needed to prevent crashes in
  // later tests.
  // TODO(shess): -removeromSuperview should not be necessary.  Fix
  // it.  Also in autocomplete_text_field_unittest.mm.
  virtual ~AutocompleteTextFieldEditorTest() {
    [cocoa_helper_.window() setDelegate:nil];
    [field_ removeFromSuperview];
    [pb_ releaseGlobally];
  }

  NSPasteboard *clipboard() {
    DCHECK(pb_);
    return pb_;
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<AutocompleteTextFieldEditor> editor_;
  scoped_nsobject<AutocompleteTextField> field_;
  MockAutocompleteTextFieldObserver field_observer_;
  scoped_nsobject<AutocompleteTextFieldWindowTestDelegate> window_delegate_;

 private:
  NSPasteboard *pb_;
};

// Test that the field editor is linked in correctly.
TEST_F(AutocompleteTextFieldEditorTest, FirstResponder) {
  EXPECT_EQ(editor_.get(), [field_ currentEditor]);
  EXPECT_TRUE([editor_.get() isDescendantOf:field_.get()]);
  EXPECT_EQ([editor_.get() delegate], field_.get());
  EXPECT_EQ([editor_.get() observer], [field_.get() observer]);
}

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
  [editor_.get() performCopy:clipboard()];
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));
  ASSERT_TRUE(ClipboardContainsText(clipboard(), test_string_1));

  // Check that cutting it works and we only get plain text.
  [editor_.get() setString:test_string_2];
  [editor_.get() selectAll:nil];
  [editor_.get() alignLeft:nil];  // Add a rich text attribute.
  [editor_.get() performCut:clipboard()];
  ASSERT_TRUE(NoRichTextOnClipboard(clipboard()));
  ASSERT_TRUE(ClipboardContainsText(clipboard(), test_string_2));
  ASSERT_EQ([[editor_.get() textStorage] length], 0U);
}

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(AutocompleteTextFieldEditorTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [field_ superview]);

  // TODO(shess): For some reason, -removeFromSuperview while |field_|
  // is first-responder causes AutocompleteTextFieldWindowTestDelegate
  // -windowWillReturnFieldEditor:toObject: to be passed an object of
  // class AutocompleteTextFieldEditor.  Which is weird.  Changing
  // first responder will remove the field editor.
  cocoa_helper_.makeFirstResponder(nil);
  EXPECT_FALSE([field_.get() currentEditor]);
  EXPECT_FALSE([editor_.get() superview]);

  [field_.get() removeFromSuperview];
  EXPECT_FALSE([field_.get() superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldEditorTest, Display) {
  [field_ display];
  [editor_ display];
}

// Test that -paste: is correctly delegated to the observer.
TEST_F(AutocompleteTextFieldEditorTest, Paste) {
  EXPECT_CALL(field_observer_, OnPaste());
  [editor_.get() paste:nil];
}

// Test that -pasteAndGo: is correctly delegated to the observer.
TEST_F(AutocompleteTextFieldEditorTest, PasteAndGo) {
  EXPECT_CALL(field_observer_, OnPasteAndGo());
  [editor_.get() pasteAndGo:nil];
}

// Test that the menu is constructed correctly when CanPasteAndGo().
TEST_F(AutocompleteTextFieldEditorTest, CanPasteAndGoMenu) {
  EXPECT_CALL(field_observer_, CanPasteAndGo())
      .WillOnce(Return(true));
  EXPECT_CALL(field_observer_, GetPasteActionStringId())
      .WillOnce(Return(IDS_PASTE_AND_GO));

  NSMenu* menu = [editor_.get() menuForEvent:nil];
  NSArray* items = [menu itemArray];
  ASSERT_EQ([items count], 6U);
  // TODO(shess): Check the titles, too?
  NSUInteger i = 0;  // Use an index to make future changes easier.
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(cut:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(copy:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(paste:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(pasteAndGo:));
  EXPECT_TRUE([[items objectAtIndex:i++] isSeparatorItem]);

  EXPECT_EQ([[items objectAtIndex:i] action], @selector(commandDispatch:));
  EXPECT_EQ([[items objectAtIndex:i] tag], IDC_EDIT_SEARCH_ENGINES);
  i++;
}

// Test that the menu is constructed correctly when !CanPasteAndGo().
TEST_F(AutocompleteTextFieldEditorTest, CannotPasteAndGoMenu) {
  EXPECT_CALL(field_observer_, CanPasteAndGo())
      .WillOnce(Return(false));

  NSMenu* menu = [editor_.get() menuForEvent:nil];
  NSArray* items = [menu itemArray];
  ASSERT_EQ([items count], 5U);
  // TODO(shess): Check the titles, too?
  NSUInteger i = 0;  // Use an index to make future changes easier.
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(cut:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(copy:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(paste:));
  EXPECT_TRUE([[items objectAtIndex:i++] isSeparatorItem]);

  EXPECT_EQ([[items objectAtIndex:i] action], @selector(commandDispatch:));
  EXPECT_EQ([[items objectAtIndex:i] tag], IDC_EDIT_SEARCH_ENGINES);
  i++;
}

// Test that the menu is constructed correctly when field isn't
// editable.
TEST_F(AutocompleteTextFieldEditorTest, CanPasteAndGoMenuNotEditable) {
  [field_.get() setEditable:NO];
  [editor_.get() setEditable:NO];

  // Never call these when not editable.
  EXPECT_CALL(field_observer_, CanPasteAndGo())
      .Times(0);
  EXPECT_CALL(field_observer_, GetPasteActionStringId())
      .Times(0);

  NSMenu* menu = [editor_.get() menuForEvent:nil];
  NSArray* items = [menu itemArray];
  ASSERT_EQ([items count], 3U);
  // TODO(shess): Check the titles, too?
  NSUInteger i = 0;  // Use an index to make future changes easier.
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(cut:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(copy:));
  EXPECT_EQ([[items objectAtIndex:i++] action], @selector(paste:));
}

}  // namespace
