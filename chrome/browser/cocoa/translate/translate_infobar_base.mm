// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/translate/translate_infobar_base.h"

#include "app/l10n_util.h"
#include "base/histogram.h"
#include "base/logging.h"  // for NOTREACHED()
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/hover_close_button.h"
#include "chrome/browser/cocoa/infobar.h"
#import "chrome/browser/cocoa/infobar_controller.h"
#import "chrome/browser/cocoa/infobar_gradient_view.h"
#include "chrome/browser/cocoa/translate/after_translate_infobar_controller.h"
#import "chrome/browser/cocoa/translate/before_translate_infobar_controller.h"
#include "chrome/browser/cocoa/translate/translate_message_infobar_controller.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

using TranslateInfoBarUtilities::MoveControl;
using TranslateInfoBarUtilities::VerticallyCenterView;
using TranslateInfoBarUtilities::VerifyControlOrderAndSpacing;
using TranslateInfoBarUtilities::CreateLabel;
using TranslateInfoBarUtilities::AddMenuItem;

// Colors for translate infobar gradient background.
const int kGreyTopColor[] = {0xC0, 0xC0, 0xC0};
const int kGreyBottomColor[] = {0xCC, 0xCC, 0xCC};

#pragma mark TranslateInfoBarUtilities helper functions.

namespace TranslateInfoBarUtilities {

// Move the |toMove| view |spacing| pixels before/after the |anchor| view.
// |after| signifies the side of |anchor| on which to place |toMove|.
void MoveControl(NSView* anchor, NSView* toMove, int spacing, bool after) {
  NSRect anchorFrame = [anchor frame];
  NSRect toMoveFrame = [toMove frame];

  // At the time of this writing, OS X doesn't natively support BiDi UIs, but
  // it doesn't hurt to be forward looking.
  bool toRight = after;

  if (toRight) {
    toMoveFrame.origin.x = NSMaxX(anchorFrame) + spacing;
  } else {
    // Place toMove to theleft of anchor.
    toMoveFrame.origin.x = NSMinX(anchorFrame) -
        spacing - NSWidth(toMoveFrame);
  }
  [toMove setFrame:toMoveFrame];
}

// Check that the control |before| is ordered visually before the |after|
// control.
// Also, check that there is space between them.
bool VerifyControlOrderAndSpacing(id before, id after) {
  NSRect beforeFrame = [before frame];
  NSRect afterFrame = [after frame];
  return NSMinX(afterFrame) >= NSMaxX(beforeFrame);
}

// Vertically center |toMove| in its container.
void VerticallyCenterView(NSView *toMove) {
  NSRect superViewFrame = [[toMove superview] frame];
  NSRect viewFrame = [toMove frame];
  viewFrame.origin.y =
      floor((NSHeight(superViewFrame) - NSHeight(viewFrame))/2.0);
  [toMove setFrame:viewFrame];
}

// Creates a label control in the style we need for the translate infobar's
// labels within |bounds|.
NSTextField* CreateLabel(NSRect bounds) {
  NSTextField* ret = [[NSTextField alloc] initWithFrame:bounds];
  [ret setEditable:NO];
  [ret setDrawsBackground:NO];
  [ret setBordered:NO];
  return ret;
}

// Adds an item with the specified properties to |menu|.
void AddMenuItem(NSMenu *menu, id target, SEL selector, NSString* title,
    int tag, bool enabled, bool checked) {
  NSMenuItem* item = [[[NSMenuItem alloc]
      initWithTitle:title
             action:selector
      keyEquivalent:@""] autorelease];
  [item setTag:tag];
  [menu addItem:item];
  [item setTarget:target];
  if (checked)
    [item setState:NSOnState];
  if (!enabled)
    [item setEnabled:NO];
}

}  // namespace TranslateInfoBarUtilities

// For compilation purposes until the linux port goes through and
// the files can be removed.
InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  NOTREACHED();
  return NULL;
}

// TranslateInfoBarDelegate views specific method:
InfoBar* TranslateInfoBarDelegate2::CreateInfoBar() {
  TranslateInfoBarControllerBase* infobar_controller = NULL;
  switch (type_) {
    case kBeforeTranslate:
      infobar_controller =
          [[BeforeTranslateInfobarController alloc] initWithDelegate:this];
      break;
    case kAfterTranslate:
      infobar_controller =
          [[AfterTranslateInfobarController alloc] initWithDelegate:this];
      break;
    case kTranslating:
    case kTranslationError:
      infobar_controller =
          [[TranslateMessageInfobarController alloc] initWithDelegate:this];
      break;
    default:
      NOTREACHED();
  }
  return new InfoBar(infobar_controller);
}

@interface TranslateInfoBarControllerBase (Private)

// Removes all controls so that layout can add in only the controls
// required.
- (void)clearAllControls;

// Create all the various controls we need for the toolbar.
- (void)constructViews;

// Reloads text for all labels for the current state.
- (void)loadLabelText:(TranslateErrors::Type)error;

// Makes the infobar grey.
- (void)setInfoBarGradientColor;

// Main function to update the toolbar graphic state and data model after
// the state has changed.
// Controls are moved around as needed and visibility changed to match the
// current state.
- (void)updateState;

// Called when the source or target language selection changes in a menu.
// |newLanguageIdx| is the index of the newly selected item in the appropriate
// menu.
- (void)sourceLanguageModified:(NSInteger)newLanguageIdx;
- (void)targetLanguageModified:(NSInteger)newLanguageIdx;

// Completely rebuild "from" and "to" language menus from the data model.
- (void)populateLanguageMenus;

@end

#pragma mark TranslateInfoBarController class

@implementation TranslateInfoBarControllerBase

- (id)initWithDelegate:(InfoBarDelegate*)delegate {
  if ((self = [super initWithDelegate:delegate])) {
      originalLanguageMenuModel_.reset(
          new LanguagesMenuModel2([self delegate],
                                  LanguagesMenuModel2::ORIGINAL));

      targetLanguageMenuModel_.reset(
          new LanguagesMenuModel2([self delegate],
                                  LanguagesMenuModel2::TARGET));
      optionsMenuModel_.reset(new OptionsMenuModel2([self delegate]));
  }
  return self;
}

- (TranslateInfoBarDelegate2*)delegate {
  return reinterpret_cast<TranslateInfoBarDelegate2*>(delegate_);
}

- (void)constructViews {
  // Using a zero or very large frame causes GTMUILocalizerAndLayoutTweaker
  // to not resize the view properly so we take the bounds of the first label
  // which is contained in the nib.
  NSRect bogusFrame = [label_ frame];
  label1_.reset(CreateLabel(bogusFrame));
  label2_.reset(CreateLabel(bogusFrame));
  label3_.reset(CreateLabel(bogusFrame));

  optionsPopUp_.reset([[NSPopUpButton alloc] initWithFrame:bogusFrame
                                                 pullsDown:YES]);
  fromLanguagePopUp_.reset([[NSPopUpButton alloc] initWithFrame:bogusFrame
                                                      pullsDown:NO]);
  toLanguagePopUp_.reset([[NSPopUpButton alloc] initWithFrame:bogusFrame
                                                     pullsDown:NO]);
  showOriginalButton_.reset([[NSButton alloc] initWithFrame:bogusFrame]);
  tryAgainButton_.reset([[NSButton alloc] initWithFrame:bogusFrame]);
}

- (void)sourceLanguageModified:(NSInteger)newLanguageIdx {
  DCHECK_GT(newLanguageIdx, -1);
  if (newLanguageIdx == [self delegate]->original_language_index())
    return;
  [self delegate]->SetOriginalLanguage(newLanguageIdx);
  int commandId = IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE + newLanguageIdx;
  int newMenuIdx = [fromLanguagePopUp_ indexOfItemWithTag:commandId];
  [fromLanguagePopUp_ selectItemAtIndex:newMenuIdx];
}

- (void)targetLanguageModified:(NSInteger)newLanguageIdx {
  DCHECK_GT(newLanguageIdx, -1);
  if (newLanguageIdx == [self delegate]->target_language_index())
    return;
  [self delegate]->SetTargetLanguage(newLanguageIdx);
  int commandId = IDC_TRANSLATE_TARGET_LANGUAGE_BASE + newLanguageIdx;
  int newMenuIdx = [toLanguagePopUp_ indexOfItemWithTag:commandId];
  [toLanguagePopUp_ selectItemAtIndex:newMenuIdx];
}

- (void)loadLabelText {
  // Do nothing by default, should be implemented by subclasses.
}

- (void)updateState {
  [self loadLabelText];
  [self clearAllControls];
  [self showVisibleControls:[self visibleControls]];
  [self layout];
}

- (void)setInfoBarGradientColor {
  // Use grey gradient for the infobars.
  NSColor* startingColor =
      [NSColor colorWithCalibratedRed:kGreyTopColor[0] / 255.0
                                green:kGreyTopColor[1] / 255.0
                                 blue:kGreyTopColor[2] / 255.0
                                alpha:1.0];
  NSColor* endingColor =
        [NSColor colorWithCalibratedRed:kGreyBottomColor[0] / 255.0
                                  green:kGreyBottomColor[1] / 255.0
                                   blue:kGreyBottomColor[2] / 255.0
                                  alpha:1.0];
  NSGradient* translateInfoBarGradient =
      [[[NSGradient alloc] initWithStartingColor:startingColor
                                     endingColor:endingColor] autorelease];

  [infoBarView_ setGradient:translateInfoBarGradient];
}

- (void)removeOkCancelButtons {
  // Removing okButton_ & cancelButton_ from the view may cause them
  // to be released and since we can still access them from other areas
  // in the code later, we need them to be nil when this happens.
  [okButton_ removeFromSuperview];
  okButton_ = nil;
  [cancelButton_ removeFromSuperview];
  cancelButton_ = nil;
}

- (void)clearAllControls {
  // Step 1: remove all controls from the infobar so we have a clean slate.
  NSArray *allControls = [NSArray arrayWithObjects:label2_.get(), label3_.get(),
      fromLanguagePopUp_.get(), toLanguagePopUp_.get(),
      showOriginalButton_.get(), tryAgainButton_.get(), nil];

  for (NSControl* control in allControls) {
    if ([control superview])
      [control removeFromSuperview];
  }
}

- (void)showVisibleControls:(NSArray*)visibleControls {
  NSRect optionsFrame = [optionsPopUp_ frame];
  for (NSControl* control in visibleControls) {
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:control];
    [control setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin |
        NSViewMaxYMargin];

    // Need to check if a view is already attached since |label1_| is always
    // parented and we don't want to add it again.
    if (![control superview])
      [infoBarView_ addSubview:control];

    if ([control isKindOfClass:[NSButton class]])
      VerticallyCenterView(control);

    // Make "from" and "to" language popup menus the same size as the options
    // menu.
    // We don't autosize since some languages names are really long causing
    // the toolbar to overflow.
    if ([control isKindOfClass:[NSPopUpButton class]])
      [control setFrame:optionsFrame];
  }
}

- (void)layout {

}

- (NSArray*)visibleControls {
  return [NSArray array];
}

- (void) rebuildOptionsMenu {
  // The options model doesn't know how to handle state transitions, so rebuild
  // it each time through here.
  optionsMenuModel_.reset(
              new OptionsMenuModel2([self delegate]));

  [optionsPopUp_ removeAllItems];
  // Set title.
  NSString* optionsLabel =
      l10n_util::GetNSString(IDS_TRANSLATE_INFOBAR_OPTIONS);
  [optionsPopUp_ addItemWithTitle:optionsLabel];

   // Populate options menu.
  NSMenu* optionsMenu = [optionsPopUp_ menu];
  [optionsMenu setAutoenablesItems:NO];
  for (int i = 0; i < optionsMenuModel_->GetItemCount(); ++i) {
    NSString* title = base::SysUTF16ToNSString(
        optionsMenuModel_->GetLabelAt(i));
    int cmd = optionsMenuModel_->GetCommandIdAt(i);
    bool checked = optionsMenuModel_->IsItemCheckedAt(i);
    bool enabled = optionsMenuModel_->IsEnabledAt(i);
    AddMenuItem(optionsMenu,
                self,
                @selector(optionsMenuChanged:),
                title,
                cmd,
                enabled,
                checked);
  }
}

- (void)populateLanguageMenus {
  NSMenu* originalLanguageMenu = [fromLanguagePopUp_ menu];
  [originalLanguageMenu setAutoenablesItems:NO];
  int selectedMenuIndex = 0;
  int selectedLangIndex = [self delegate]->original_language_index();
  for (int i = 0; i < originalLanguageMenuModel_->GetItemCount(); ++i) {
    NSString* title = base::SysUTF16ToNSString(
        originalLanguageMenuModel_->GetLabelAt(i));
    int cmd = originalLanguageMenuModel_->GetCommandIdAt(i);
    bool checked = (cmd == selectedLangIndex);
    if (checked)
      selectedMenuIndex = i;
    bool enabled = originalLanguageMenuModel_->IsEnabledAt(i);
    cmd += IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE;
    AddMenuItem(originalLanguageMenu,
                self,
                @selector(languageMenuChanged:),
                title,
                cmd,
                enabled,
                checked);
  }
  [fromLanguagePopUp_ selectItemAtIndex:selectedMenuIndex];

  NSMenu* targetLanguageMenu = [toLanguagePopUp_ menu];
  [targetLanguageMenu setAutoenablesItems:NO];
  selectedLangIndex = [self delegate]->target_language_index();
  for (int i = 0; i < targetLanguageMenuModel_->GetItemCount(); ++i) {
    NSString* title = base::SysUTF16ToNSString(
        targetLanguageMenuModel_->GetLabelAt(i));
    int cmd = targetLanguageMenuModel_->GetCommandIdAt(i);
    bool checked = (cmd == selectedLangIndex);
    if (checked)
      selectedMenuIndex = i;
    bool enabled = targetLanguageMenuModel_->IsEnabledAt(i);
    cmd += IDC_TRANSLATE_TARGET_LANGUAGE_BASE;
    AddMenuItem(targetLanguageMenu,
                self,
                @selector(languageMenuChanged:),
                title,
                cmd,
                enabled,
                checked);
  }
  [toLanguagePopUp_ selectItemAtIndex:selectedMenuIndex];
}

- (void)addAdditionalControls {
  using l10n_util::GetNSString;
  using l10n_util::GetNSStringWithFixup;

  // Get layout information from the NIB.
  NSRect okButtonFrame = [okButton_ frame];
  NSRect cancelButtonFrame = [cancelButton_ frame];
  spaceBetweenControls_ = NSMinX(cancelButtonFrame) - NSMaxX(okButtonFrame);

  // Set infobar background color.
  [self setInfoBarGradientColor];

  // Instantiate additional controls.
  [self constructViews];

  // Set ourselves as the delegate for the options menu so we can populate it
  // dynamically.
  [[optionsPopUp_ menu] setDelegate:self];

  // Replace label_ with label1_ so we get a consistent look between all the
  // labels we display in the translate view.
  [[label_ superview] replaceSubview:label_ with:label1_.get()];
  label_.reset(); // Now released.

  // Populate contextual menus.
  [self rebuildOptionsMenu];
  [self populateLanguageMenus];

  // Set OK & Cancel text.
  [okButton_ setTitle:GetNSStringWithFixup(IDS_TRANSLATE_INFOBAR_ACCEPT)];
  [cancelButton_ setTitle:GetNSStringWithFixup(IDS_TRANSLATE_INFOBAR_DENY)];

  // Set up "Show original" and "Try again" buttons.
  [showOriginalButton_ setBezelStyle:NSRoundRectBezelStyle];
  [showOriginalButton_ setFrame:okButtonFrame];
  [tryAgainButton_ setBezelStyle:NSRoundRectBezelStyle];
  [tryAgainButton_ setFrame:okButtonFrame];

  [showOriginalButton_ setTarget:self];
  [showOriginalButton_ setAction:@selector(showOriginal:)];
  [tryAgainButton_ setTarget:self];
  [tryAgainButton_ setAction:@selector(ok:)];

  [showOriginalButton_
      setTitle:GetNSStringWithFixup(IDS_TRANSLATE_INFOBAR_REVERT)];
  [tryAgainButton_
      setTitle:GetNSStringWithFixup(IDS_TRANSLATE_INFOBAR_RETRY)];

  // Add and configure controls that are visible in all modes.
  [optionsPopUp_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin |
          NSViewMaxYMargin];
  // Add "options" popup z-ordered below all other controls so when we
  // resize the toolbar it doesn't hide them.
  [infoBarView_ addSubview:optionsPopUp_
                positioned:NSWindowBelow
                relativeTo:nil];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:optionsPopUp_];
  MoveControl(closeButton_, optionsPopUp_, spaceBetweenControls_, false);
  VerticallyCenterView(optionsPopUp_);

  // Show and place GUI elements.
  [self updateState];
}

// Called when "Translate" button is clicked.
- (IBAction)ok:(id)sender {
  TranslateInfoBarDelegate2* delegate = [self delegate];
  TranslateInfoBarDelegate2::Type state = delegate->type();
  DCHECK(state == TranslateInfoBarDelegate2::kBeforeTranslate ||
      state == TranslateInfoBarDelegate2::kTranslationError);
  delegate->Translate();
  UMA_HISTOGRAM_COUNTS("Translate.Translate", 1);
}

// Called when someone clicks on the "Nope" button.
- (IBAction)cancel:(id)sender {
  DCHECK(
      [self delegate]->type() == TranslateInfoBarDelegate2::kBeforeTranslate);
  [self delegate]->TranslationDeclined();
  UMA_HISTOGRAM_COUNTS("Translate.DeclineTranslate", 1);
  [super dismiss:nil];
}

- (IBAction)showOriginal:(id)sender {
  [self delegate]->RevertTranslation();
}

// Called when any of the language drop down menus are changed.
- (void)languageMenuChanged:(id)item {
  if ([item respondsToSelector:@selector(tag)]) {
    int cmd = [item tag];
    if (cmd >= IDC_TRANSLATE_TARGET_LANGUAGE_BASE) {
      cmd -= IDC_TRANSLATE_TARGET_LANGUAGE_BASE;
      [self targetLanguageModified:cmd];
      return;
    } else if (cmd >= IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE) {
      cmd -= IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE;
      [self sourceLanguageModified:cmd];
      return;
    }
  }
  NOTREACHED() << "Language menu was changed with a bad language ID";
}

// Called when the options menu is changed.
- (void)optionsMenuChanged:(id)item {
  if ([item respondsToSelector:@selector(tag)]) {
    int cmd = [item tag];
    // Danger Will Robinson! : This call can release the infobar (e.g. invoking
    // "About Translate" can open a new tab).
    // Do not access member variables after this line!
    optionsMenuModel_->ExecuteCommand(cmd);
  } else {
    NOTREACHED();
  }
}

#pragma mark NSMenuDelegate

// Invoked by virtue of us being set as the delegate for the options menu.
- (void)menuNeedsUpdate:(NSMenu *)menu {
  [self rebuildOptionsMenu];
}

@end

@implementation TranslateInfoBarControllerBase (TestingAPI)

- (NSMenu*)optionsMenu {
  return [optionsPopUp_ menu];
}

- (NSButton*)tryAgainButton {
  return tryAgainButton_.get();
}

- (bool)verifyLayout {
  // All the controls available to translate infobars, except the options popup.
  // The options popup is shown/hidden instead of actually removed.  This gets
  // checked in the subclasses.
  NSArray* allControls = [NSArray arrayWithObjects:label1_.get(),
      fromLanguagePopUp_.get(), label2_.get(), toLanguagePopUp_.get(),
      label3_.get(), showOriginalButton_.get(), tryAgainButton_.get(), nil];
  NSArray* visibleControls = [self visibleControls];

  // Step 1: Make sure control visibility is what we expect.
  for (NSUInteger i = 0; i < [allControls count]; ++i) {
    id control = [allControls objectAtIndex:i];
    bool hasSuperView = [control superview];
    bool expectedVisibility = [visibleControls containsObject:control];

    if (expectedVisibility != hasSuperView) {
      NSString *title = @"";
      if ([control isKindOfClass:[NSPopUpButton class]]) {
        title = [[[control menu] itemAtIndex:0] title];
      }

      LOG(ERROR) <<
          "State: " << [self description] <<
          " Control @" << i << (hasSuperView ? " has" : " doesn't have") <<
          " a superview" << [[control description] UTF8String] <<
          " Title=" << [title UTF8String];
      return false;
    }
  }

  // Step 2: Check that controls are ordered correctly with no overlap.
  id previousControl = nil;
  for (NSUInteger i = 0; i < [visibleControls count]; ++i) {
    id control = [visibleControls objectAtIndex:i];
    if (previousControl && !VerifyControlOrderAndSpacing(previousControl, control)) {
      NSString *title = @"";
      if ([control isKindOfClass:[NSPopUpButton class]]) {
        title = [[[control menu] itemAtIndex:0] title];
      }
      LOG(ERROR) <<
          "State: " << [self description] <<
          " Control @" << i << " not ordered correctly: " <<
          [[control description] UTF8String] <<[title UTF8String];
      return false;
    }
    previousControl = control;
  }

  return true;
}

@end // TranslateInfoBarControllerBase (TestingAPI)

