// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_window_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_input_field.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_loading_shield_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_overlay_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kAccountChooserHeight = 20.0;
const CGFloat kMinimumContentsHeight = 101;

// Height of all decorations & paddings on main dialog together.
const CGFloat kDecorationHeight = kAccountChooserHeight +
                                  autofill::kDetailVerticalPadding +
                                  chrome_style::kClientBottomPadding +
                                  chrome_style::kTitleTopPadding;
}  // namespace

#pragma mark Field Editor

@interface AutofillDialogFieldEditor : NSTextView
@end


@implementation AutofillDialogFieldEditor

- (void)mouseDown:(NSEvent*)event {
  // Delegate _must_ be notified before mouseDown is complete, since it needs
  // to distinguish between mouseDown for already focused fields, and fields
  // that will receive focus as part of the mouseDown.
  AutofillTextField* textfield =
      base::mac::ObjCCastStrict<AutofillTextField>([self delegate]);
  [textfield onEditorMouseDown:self];
  [super mouseDown:event];
}

@end


#pragma mark Window Controller

@interface AutofillDialogWindowController ()

// Compute maximum allowed height for the dialog.
- (CGFloat)maxHeight;

// Update size constraints on sign-in container.
- (void)updateSignInSizeConstraints;

// Notification that the WebContent's view frame has changed.
- (void)onContentViewFrameDidChange:(NSNotification*)notification;

@end


@implementation AutofillDialogWindowController (NSWindowDelegate)

- (id)windowWillReturnFieldEditor:(NSWindow*)window toObject:(id)client {
  AutofillTextField* textfield = base::mac::ObjCCast<AutofillTextField>(client);
  if (!textfield)
    return nil;

  if (!fieldEditor_) {
    fieldEditor_.reset([[AutofillDialogFieldEditor alloc] init]);
    [fieldEditor_ setFieldEditor:YES];
  }
  return fieldEditor_.get();
}

@end


@implementation AutofillDialogWindowController

- (id)initWithWebContents:(content::WebContents*)webContents
      autofillDialog:(autofill::AutofillDialogCocoa*)autofillDialog {
  DCHECK(webContents);

  base::scoped_nsobject<ConstrainedWindowCustomWindow> window(
      [[ConstrainedWindowCustomWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater]);

  if ((self = [super initWithWindow:window])) {
    [window setDelegate:self];
    webContents_ = webContents;
    autofillDialog_ = autofillDialog;

    mainContainer_.reset([[AutofillMainContainer alloc]
                             initWithDelegate:autofillDialog->delegate()]);
    [mainContainer_ setTarget:self];

    signInContainer_.reset(
        [[AutofillSignInContainer alloc] initWithDialog:autofillDialog]);
    [[signInContainer_ view] setHidden:YES];

    // Set dialog title.
    titleTextField_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [titleTextField_ setEditable:NO];
    [titleTextField_ setBordered:NO];
    [titleTextField_ setDrawsBackground:NO];
    [titleTextField_ setFont:[NSFont systemFontOfSize:15.0]];
    [titleTextField_ setStringValue:
        base::SysUTF16ToNSString(autofillDialog->delegate()->DialogTitle())];
    [titleTextField_ sizeToFit];

    accountChooser_.reset([[AutofillAccountChooser alloc]
                              initWithFrame:NSZeroRect
                                   delegate:autofillDialog->delegate()]);

    loadingShieldController_.reset(
        [[AutofillLoadingShieldController alloc]
            initWithDelegate:autofillDialog->delegate()]);
    [[loadingShieldController_ view] setHidden:YES];

    overlayController_.reset(
        [[AutofillOverlayController alloc] initWithDelegate:
            autofillDialog->delegate()]);
    [[overlayController_ view] setHidden:YES];

    // This needs a flipped content view because otherwise the size
    // animation looks odd. However, replacing the contentView for constrained
    // windows does not work - it does custom rendering.
    base::scoped_nsobject<NSView> flippedContentView(
        [[FlippedView alloc] initWithFrame:
            [[[self window] contentView] frame]]);
    [flippedContentView setSubviews:
        @[accountChooser_,
          titleTextField_,
          [mainContainer_ view],
          [signInContainer_ view],
          [loadingShieldController_ view],
          [overlayController_ view]]];
    [flippedContentView setAutoresizingMask:
        (NSViewWidthSizable | NSViewHeightSizable)];
    [[[self window] contentView] addSubview:flippedContentView];
    [mainContainer_ setAnchorView:[[accountChooser_ subviews] objectAtIndex:1]];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (CGFloat)maxHeight {
  NSRect dialogFrameRect = [[self window] frame];
  NSRect browserFrameRect =
      [webContents_->GetView()->GetTopLevelNativeWindow() frame];
  dialogFrameRect.size.height =
      NSMaxY(dialogFrameRect) - NSMinY(browserFrameRect);
  dialogFrameRect = [[self window] contentRectForFrameRect:dialogFrameRect];
  return NSHeight(dialogFrameRect);
}

- (void)updateSignInSizeConstraints {
  // Adjust for the size of all decorations and paddings outside main content.
  CGFloat minHeight = kMinimumContentsHeight - kDecorationHeight;
  CGFloat maxHeight = std::max([self maxHeight] - kDecorationHeight, minHeight);
  CGFloat width = NSWidth([[[self window] contentView] frame]);

  [signInContainer_ constrainSizeToMinimum:NSMakeSize(width, minHeight)
                                   maximum:NSMakeSize(width, maxHeight)];
}

- (void)onContentViewFrameDidChange:(NSNotification*)notification {
  [self updateSignInSizeConstraints];
  if ([[signInContainer_ view] isHidden])
    [self requestRelayout];
}

- (void)cancelRelayout {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(performLayout)
                                             object:nil];
}

- (void)requestRelayout {
  [self cancelRelayout];
  [self performSelector:@selector(performLayout) withObject:nil afterDelay:0.0];
}

- (NSSize)preferredSize {
  NSSize size;

  // Overall size is determined by either main container or sign in view.
  if ([[signInContainer_ view] isHidden])
    size = [mainContainer_ preferredSize];
  else
    size = [signInContainer_ preferredSize];

  // Always make room for the header.
  size.height += kDecorationHeight;

  if (![[overlayController_ view] isHidden]) {
    CGFloat height = [overlayController_ heightForWidth:size.width];
    if (height != 0.0)
      size.height = height;
  }

  // Show as much of the main view as is possible without going past the
  // bottom of the browser window.
  size.height = std::min(size.height, [self maxHeight]);

  return size;
}

- (void)performLayout {
  NSRect contentRect = NSZeroRect;
  contentRect.size = [self preferredSize];
  NSRect clientRect = contentRect;
  clientRect.origin.y = chrome_style::kTitleTopPadding;
  clientRect.size.height -= chrome_style::kTitleTopPadding +
                            chrome_style::kClientBottomPadding;

  [titleTextField_ setStringValue:
      base::SysUTF16ToNSString(autofillDialog_->delegate()->DialogTitle())];
  [titleTextField_ sizeToFit];

  NSRect headerRect, mainRect, titleRect, dummyRect;
  NSDivideRect(clientRect, &headerRect, &mainRect,
               kAccountChooserHeight, NSMinYEdge);
  NSDivideRect(mainRect, &dummyRect, &mainRect,
               autofill::kDetailVerticalPadding, NSMinYEdge);
  headerRect = NSInsetRect(headerRect, chrome_style::kHorizontalPadding, 0);
  NSDivideRect(headerRect, &titleRect, &headerRect,
               NSWidth([titleTextField_ frame]), NSMinXEdge);

  // Align baseline of title with bottom of accountChooser.
  base::scoped_nsobject<NSLayoutManager> layout_manager(
      [[NSLayoutManager alloc] init]);
  NSFont* titleFont = [titleTextField_ font];
  titleRect.origin.y += NSHeight(titleRect) -
      [layout_manager defaultBaselineOffsetForFont:titleFont];
  [titleTextField_ setFrame:titleRect];

  [accountChooser_ setFrame:headerRect];
  [accountChooser_ performLayout];
  if ([[signInContainer_ view] isHidden]) {
    [[mainContainer_ view] setFrame:mainRect];
    [mainContainer_ performLayout];
  } else {
    [[signInContainer_ view] setFrame:mainRect];
  }

  [[loadingShieldController_ view] setFrame:contentRect];
  [loadingShieldController_ performLayout];

  [[overlayController_ view] setFrame:contentRect];
  [overlayController_ performLayout];

  NSRect frameRect = [[self window] frameRectForContentRect:contentRect];
  [[self window] setFrame:frameRect display:YES];
  [[self window] recalculateKeyViewLoop];
}

- (IBAction)accept:(id)sender {
  if ([mainContainer_ validate])
    autofillDialog_->delegate()->OnAccept();
  else
    [mainContainer_ makeFirstInvalidInputFirstResponder];
}

- (IBAction)cancel:(id)sender {
  autofillDialog_->delegate()->OnCancel();
  autofillDialog_->PerformClose();
}

- (void)show {
  // Resizing the browser causes the ConstrainedWindow to move.
  // Observe that to allow resizes based on browser size.
  // NOTE: This MUST come last after all initial setup is done, because there
  // is an immediate notification post registration.
  DCHECK([self window]);
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onContentViewFrameDidChange:)
             name:NSWindowDidMoveNotification
           object:[self window]];

  [self updateAccountChooser];
  [self updateNotificationArea];
  [self requestRelayout];
}

- (void)hide {
  autofillDialog_->delegate()->OnCancel();
  autofillDialog_->PerformClose();
}

- (void)updateNotificationArea {
  [mainContainer_ updateNotificationArea];
}

- (void)updateAccountChooser {
  [accountChooser_ update];
  [mainContainer_ updateLegalDocuments];

  // For the duration of the loading shield, hide the main contents.
  // This prevents the currently focused text field "shining through".
  // No need to remember previous state, because the loading shield
  // always flows through to the main container.
  [loadingShieldController_ update];
  [[mainContainer_ view] setHidden:![[loadingShieldController_ view] isHidden]];
}

- (void)updateButtonStrip {
  [overlayController_ updateState];
}

- (void)updateSection:(autofill::DialogSection)section {
  [[mainContainer_ sectionForId:section] update];
  [mainContainer_ updateSaveInChrome];
}

- (void)fillSection:(autofill::DialogSection)section
           forInput:(const autofill::DetailInput&)input {
  [[mainContainer_ sectionForId:section] fillForInput:input];
  [mainContainer_ updateSaveInChrome];
}

- (content::NavigationController*)showSignIn {
  [self updateSignInSizeConstraints];
  [signInContainer_ loadSignInPage];
  [[mainContainer_ view] setHidden:YES];
  [[signInContainer_ view] setHidden:NO];
  [self requestRelayout];

  return [signInContainer_ navigationController];
}

- (void)getInputs:(autofill::DetailOutputMap*)output
       forSection:(autofill::DialogSection)section {
  [[mainContainer_ sectionForId:section] getInputs:output];
}

- (NSString*)getCvc {
  autofill::DialogSection section = autofill::SECTION_CC;
  NSString* value = [[mainContainer_ sectionForId:section] suggestionText];
  if (!value) {
    section = autofill::SECTION_CC_BILLING;
    value = [[mainContainer_ sectionForId:section] suggestionText];
  }
  return value;
}

- (BOOL)saveDetailsLocally {
  return [mainContainer_ saveDetailsLocally];
}

- (void)hideSignIn {
  [[signInContainer_ view] setHidden:YES];
  [[mainContainer_ view] setHidden:NO];
  [self requestRelayout];
}

- (void)modelChanged {
  [mainContainer_ modelChanged];
}

- (void)updateErrorBubble {
  [mainContainer_ updateErrorBubble];
}

- (void)onSignInResize:(NSSize)size {
  [signInContainer_ setPreferredSize:size];
  [self requestRelayout];
}

@end


@implementation AutofillDialogWindowController (TestableAutofillDialogView)

- (void)setTextContents:(NSString*)text
               forInput:(const autofill::DetailInput&)input {
  for (size_t i = autofill::SECTION_MIN; i <= autofill::SECTION_MAX; ++i) {
    autofill::DialogSection section = static_cast<autofill::DialogSection>(i);
    // TODO(groby): Need to find the section for an input directly - wasteful.
    [[mainContainer_ sectionForId:section] setFieldValue:text forInput:input];
  }
}

- (void)setTextContents:(NSString*)text
 ofSuggestionForSection:(autofill::DialogSection)section {
  [[mainContainer_ sectionForId:section] setSuggestionFieldValue:text];
}

- (void)activateFieldForInput:(const autofill::DetailInput&)input {
  for (size_t i = autofill::SECTION_MIN; i <= autofill::SECTION_MAX; ++i) {
    autofill::DialogSection section = static_cast<autofill::DialogSection>(i);
    [[mainContainer_ sectionForId:section] activateFieldForInput:input];
  }
}

- (content::WebContents*)getSignInWebContents {
  return [signInContainer_ webContents];
}

- (BOOL)isShowingOverlay {
  return ![[overlayController_ view] isHidden];
}

@end
