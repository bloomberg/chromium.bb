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
#include "testing/platform_test.h"

@interface AutocompleteTextFieldTestDelegate : NSObject {
  BOOL textShouldPaste_;
  BOOL receivedTextShouldPaste_;
  BOOL receivedFlagsChanged_;
  BOOL receivedControlTextDidBeginEditing_;
  BOOL receivedControlTextShouldEndEditing_;
}
- initWithTextShouldPaste:(BOOL)flag;
- (BOOL)receivedTextShouldPaste;
- (BOOL)receivedFlagsChanged;
- (BOOL)receivedControlTextDidBeginEditing;
- (BOOL)receivedControlTextShouldEndEditing;
@end

@interface AutocompleteTextFieldWindowTestDelegate : NSObject {
  scoped_nsobject<AutocompleteTextFieldEditor> editor_;
}
- (id)windowWillReturnFieldEditor:(NSWindow *)sender toObject:(id)anObject;
@end

namespace {

class AutocompleteTextFieldTest : public PlatformTest {
 public:
  AutocompleteTextFieldTest() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    NSRect frame = NSMakeRect(0, 0, 300, 30);
    field_.reset([[AutocompleteTextField alloc] initWithFrame:frame]);
    [field_ setStringValue:@"Testing"];
    [cocoa_helper_.contentView() addSubview:field_.get()];

    window_delegate_.reset(
        [[AutocompleteTextFieldWindowTestDelegate alloc] init]);
    [cocoa_helper_.window() setDelegate:window_delegate_];
  }

  // The removeFromSuperview call is needed to prevent crashes in later tests.
  ~AutocompleteTextFieldTest() {
    [cocoa_helper_.window() setDelegate:nil];
    [field_ removeFromSuperview];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<AutocompleteTextField> field_;
  scoped_nsobject<AutocompleteTextFieldWindowTestDelegate> window_delegate_;
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

// Test that we get the same cell from -cell and
// -autocompleteTextFieldCell.
TEST_F(AutocompleteTextFieldTest, Cell) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  EXPECT_EQ(cell, [field_ cell]);
  EXPECT_TRUE(cell != nil);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldTest, Display) {
  [field_ display];

  // Test display of various cell configurations.
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];

  [cell setSearchHintString:@"Type to search"];
  [field_ display];

  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"prefix" image:image suffix:@"suffix"];
  [field_ display];

  [cell setKeywordString:@"Search Engine:"];
  [field_ display];

  [cell clearKeywordAndHint];
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
  [field_ setDelegate:nil];
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
  [field_ setDelegate:nil];
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
  [field_ setDelegate:nil];
}

// Test that the field editor is reset correctly when search keyword
// or hints change.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditor) {
  EXPECT_EQ(nil, [field_ currentEditor]);
  EXPECT_EQ([[field_ subviews] count], 0U);
  [[field_ window] makeFirstResponder:field_];
  EXPECT_FALSE(nil == [field_ currentEditor]);
  EXPECT_EQ([[field_ subviews] count], 1U);

  // Check that the window delegate is working right.
  {
    Class c = [AutocompleteTextFieldEditor class];
    EXPECT_TRUE([[field_ currentEditor] isKindOfClass:c]);
  }

  // The field editor may not be an immediate subview of |field_|, it
  // may be a subview of a clipping view (for purposes of scrolling).
  // So just look at the immediate subview.
  EXPECT_EQ([[field_ subviews] count], 1U);
  const NSRect baseEditorFrame([[[field_ subviews] objectAtIndex:0] frame]);

  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Asking the cell to add a search hint should leave the field
  // editor alone until -resetFieldEditorFrameIfNeeded is called.
  // Then the field editor should be moved to a smaller region with
  // the same left-hand side.
  [cell setSearchHintString:@"Type to search"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  NSRect r = [[[field_ subviews] objectAtIndex:0] frame];
  EXPECT_TRUE(NSEqualRects(r, baseEditorFrame));
  [field_ resetFieldEditorFrameIfNeeded];
  r = [[[field_ subviews] objectAtIndex:0] frame];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_FALSE(NSEqualRects(r, baseEditorFrame));
  EXPECT_TRUE(NSContainsRect(baseEditorFrame, r));
  EXPECT_EQ(NSMinX(r), NSMinX(baseEditorFrame));
  EXPECT_LT(NSWidth(r), NSWidth(baseEditorFrame));

  // Save the search-hint editor frame for later.
  const NSRect searchHintEditorFrame(r);

  // Asking the cell to change to keyword mode should leave the field
  // editor alone until -resetFieldEditorFrameIfNeeded is called.
  // Then the field editor should be moved to a smaller region with
  // the same right-hand side.
  [cell setKeywordString:@"Search Engine:"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  r = [[[field_ subviews] objectAtIndex:0] frame];
  EXPECT_TRUE(NSEqualRects(r, searchHintEditorFrame));
  [field_ resetFieldEditorFrameIfNeeded];
  r = [[[field_ subviews] objectAtIndex:0] frame];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_FALSE(NSEqualRects(r, baseEditorFrame));
  EXPECT_FALSE(NSEqualRects(r, searchHintEditorFrame));
  EXPECT_TRUE(NSContainsRect(baseEditorFrame, r));
  EXPECT_EQ(NSMaxX(r), NSMaxX(baseEditorFrame));
  EXPECT_LT(NSWidth(r), NSWidth(baseEditorFrame));

  // Asking the cell to clear everything should leave the field editor
  // alone until -resetFieldEditorFrameIfNeeded is called.  Then the
  // field editor should be back to baseEditorFrame.
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [field_ resetFieldEditorFrameIfNeeded];
  r = [[[field_ subviews] objectAtIndex:0] frame];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  EXPECT_TRUE(NSEqualRects(r, baseEditorFrame));
}

// Test that the field editor is reset correctly when search keyword
// or hints change.
TEST_F(AutocompleteTextFieldTest, ResetFieldEditorBlocksEndEditing) {
  [[field_ window] makeFirstResponder:field_];

  // First, test that -makeFirstResponder: sends
  // -controlTextDidBeginEditing: and -control:textShouldEndEditing at
  // the expected times.
  {
    scoped_nsobject<AutocompleteTextFieldTestDelegate> delegate(
        [[AutocompleteTextFieldTestDelegate alloc] initWithTextShouldPaste:NO]);
    EXPECT_FALSE([delegate receivedControlTextDidBeginEditing]);
    EXPECT_FALSE([delegate receivedControlTextShouldEndEditing]);

    [field_ setDelegate:delegate];
    [[field_ window] makeFirstResponder:field_];
    NSTextView* editor = static_cast<NSTextView*>([field_ currentEditor]);
    EXPECT_TRUE(nil != editor);
    EXPECT_FALSE([delegate receivedControlTextDidBeginEditing]);
    EXPECT_FALSE([delegate receivedControlTextShouldEndEditing]);

    // This should start the begin/end editing state.
    [editor shouldChangeTextInRange:NSMakeRange(0, 0) replacementString:@""];
    EXPECT_TRUE([delegate receivedControlTextDidBeginEditing]);
    EXPECT_FALSE([delegate receivedControlTextShouldEndEditing]);

    // This should send the end-editing message.
    [[field_ window] makeFirstResponder:field_];
    EXPECT_TRUE([delegate receivedControlTextShouldEndEditing]);
    [field_ setDelegate:nil];
  }

  // Then test that -resetFieldEditorFrameIfNeeded manages without
  // sending that message.
  {
    scoped_nsobject<AutocompleteTextFieldTestDelegate> delegate(
        [[AutocompleteTextFieldTestDelegate alloc] initWithTextShouldPaste:NO]);
    [field_ setDelegate:delegate];
    EXPECT_FALSE([delegate receivedControlTextDidBeginEditing]);
    EXPECT_FALSE([delegate receivedControlTextShouldEndEditing]);

    AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
    EXPECT_FALSE([cell fieldEditorNeedsReset]);
    [cell setSearchHintString:@"Type to search"];
    EXPECT_TRUE([cell fieldEditorNeedsReset]);
    EXPECT_FALSE([delegate receivedControlTextShouldEndEditing]);
    [field_ resetFieldEditorFrameIfNeeded];
    EXPECT_FALSE([delegate receivedControlTextShouldEndEditing]);
    EXPECT_TRUE([delegate receivedControlTextDidBeginEditing]);
    [field_ setDelegate:nil];
  }
}

}  // namespace

@implementation AutocompleteTextFieldTestDelegate

- initWithTextShouldPaste:(BOOL)flag {
  self = [super init];
  if (self) {
    textShouldPaste_ = flag;
    receivedTextShouldPaste_ = NO;
    receivedFlagsChanged_ = NO;
    receivedControlTextDidBeginEditing_ = NO;
    receivedControlTextShouldEndEditing_ = NO;
  }
  return self;
}

- (BOOL)receivedTextShouldPaste {
  return receivedTextShouldPaste_;
}

- (BOOL)receivedFlagsChanged {
  return receivedFlagsChanged_;
}

- (BOOL)receivedControlTextDidBeginEditing {
  return receivedControlTextDidBeginEditing_;
}

- (BOOL)receivedControlTextShouldEndEditing {
  return receivedControlTextShouldEndEditing_;
}

- (BOOL)control:(NSControl*)control textShouldPaste:(NSText*)fieldEditor {
  receivedTextShouldPaste_ = YES;
  return textShouldPaste_;
}

- (void)control:(id)control flagsChanged:(NSEvent*)theEvent {
  receivedFlagsChanged_ = YES;
}

- (void)controlTextDidBeginEditing:(NSNotification*)aNotification {
  receivedControlTextDidBeginEditing_ = YES;
}

- (BOOL)control:(NSControl*)control textShouldEndEditing:(NSText*)fieldEditor {
  receivedControlTextShouldEndEditing_ = YES;
  return YES;
}

@end

@implementation AutocompleteTextFieldWindowTestDelegate

- (id)windowWillReturnFieldEditor:(NSWindow *)sender toObject:(id)anObject {
  EXPECT_TRUE([anObject isKindOfClass:[AutocompleteTextField class]]);

  if (editor_ == nil) {
    editor_.reset([[AutocompleteTextFieldEditor alloc] init]);
  }
  EXPECT_TRUE(editor_ != nil);

  // This needs to be called every time, otherwise notifications
  // aren't sent correctly.
  [editor_ setFieldEditor:YES];
  return editor_;
}

@end
