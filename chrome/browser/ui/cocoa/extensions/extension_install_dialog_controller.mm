// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include "base/mac/mac_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Maximum height we will adjust controls to when trying to accomodate their
// contents.
const CGFloat kMaxControlHeight = 400;

// Adjust a control's height so that its content its not clipped. Returns the
// amount the control's height had to be adjusted.
CGFloat AdjustControlHeightToFitContent(NSControl* control) {
  NSRect currentRect = [control frame];
  NSRect fitRect = currentRect;
  fitRect.size.height = kMaxControlHeight;
  CGFloat desiredHeight = [[control cell] cellSizeForBounds:fitRect].height;
  CGFloat offset = desiredHeight - currentRect.size.height;

  [control setFrameSize:NSMakeSize(currentRect.size.width,
                                   currentRect.size.height + offset)];
  return offset;
}

// Moves the control vertically by the specified amount.
void OffsetControlVertically(NSControl* control, CGFloat amount) {
  NSPoint origin = [control frame].origin;
  origin.y += amount;
  [control setFrameOrigin:origin];
}

}

@implementation ExtensionInstallDialogController

@synthesize iconView = iconView_;
@synthesize titleField = titleField_;
@synthesize subtitleField = subtitleField_;
@synthesize warningsField = warningsField_;
@synthesize warningsBox= warningsBox_;
@synthesize cancelButton = cancelButton_;
@synthesize okButton = okButton_;

- (id)initWithParentWindow:(NSWindow*)window
                   profile:(Profile*)profile
                 extension:(const Extension*)extension
                  delegate:(ExtensionInstallUI::Delegate*)delegate
                      icon:(SkBitmap*)icon
                  warnings:(const std::vector<string16>&)warnings
                      type:(ExtensionInstallUI::PromptType)type {
  NSString* nibpath = nil;

  // We use a different XIB in the case of no warnings, that is a little bit
  // more nicely laid out.
  if (warnings.empty()) {
    nibpath = [base::mac::MainAppBundle()
               pathForResource:@"ExtensionInstallPromptNoWarnings"
                        ofType:@"nib"];
  } else {
   nibpath = [base::mac::MainAppBundle()
              pathForResource:@"ExtensionInstallPrompt"
                       ofType:@"nib"];
  }

  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = window;
    profile_ = profile;
    icon_ = *icon;
    delegate_ = delegate;

    title_.reset(
        [l10n_util::GetNSStringF(ExtensionInstallUI::kHeadingIds[type],
                                 UTF8ToUTF16(extension->name())) retain]);
    subtitle_.reset(
         [l10n_util::GetNSString(ExtensionInstallUI::kWarningIds[type])
          retain]);
    button_.reset([l10n_util::GetNSString(ExtensionInstallUI::kButtonIds[type])
                   retain]);

    // We display the warnings as a simple text string, separated by newlines.
    if (!warnings.empty()) {
      string16 joined_warnings;
      for (size_t i = 0; i < warnings.size(); ++i) {
        if (i > 0)
          joined_warnings += UTF8ToUTF16("\n\n");

        joined_warnings += warnings[i];
      }

      warnings_.reset(
          [base::SysUTF16ToNSString(joined_warnings) retain]);
    }
  }
  return self;
}

- (void)runAsModalSheet {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

- (IBAction)cancel:(id)sender {
  delegate_->InstallUIAbort();
  [NSApp endSheet:[self window]];
}

- (IBAction)ok:(id)sender {
  delegate_->InstallUIProceed();
  [NSApp endSheet:[self window]];
}

- (void)awakeFromNib {
  [titleField_ setStringValue:title_.get()];
  [subtitleField_ setStringValue:subtitle_.get()];
  [okButton_ setTitle:button_.get()];

  NSImage* image = gfx::SkBitmapToNSImage(icon_);
  [iconView_ setImage:image];

  // Make sure we're the window's delegate as set in the nib.
  DCHECK_EQ(self, static_cast<ExtensionInstallDialogController*>(
                      [[self window] delegate]));

  // If there are any warnings, then we have to do some special layout.
  if ([warnings_.get() length] > 0) {
    [warningsField_ setStringValue:warnings_.get()];

    // The dialog is laid out in the NIB exactly how we want it assuming that
    // each label fits on one line. However, for each label, we want to allow
    // wrapping onto multiple lines. So we accumulate an offset by measuring how
    // big each label wants to be, and comparing it to how bit it actually is.
    // Then we shift each label down and resize by the appropriate amount, then
    // finally resize the window.
    CGFloat totalOffset = 0.0;

    // Text fields.
    totalOffset += AdjustControlHeightToFitContent(titleField_);
    OffsetControlVertically(titleField_, -totalOffset);

    totalOffset += AdjustControlHeightToFitContent(subtitleField_);
    OffsetControlVertically(subtitleField_, -totalOffset);

    CGFloat warningsOffset = AdjustControlHeightToFitContent(warningsField_);
    OffsetControlVertically(warningsField_, -warningsOffset);
    totalOffset += warningsOffset;

    NSRect warningsBoxRect = [warningsBox_ frame];
    warningsBoxRect.origin.y -= totalOffset;
    warningsBoxRect.size.height += warningsOffset;
    [warningsBox_ setFrame:warningsBoxRect];

    // buttons are positioned automatically in the XIB.

    // Finally, adjust the window size.
    NSRect currentRect = [[self window] frame];
    [[self window] setFrame:NSMakeRect(currentRect.origin.x,
                                       currentRect.origin.y - totalOffset,
                                       currentRect.size.width,
                                       currentRect.size.height + totalOffset)
                    display:NO];
  }
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  [sheet close];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

@end  // ExtensionInstallDialogController

void ShowExtensionInstallDialog(
    Profile* profile,
    ExtensionInstallUI::Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const std::vector<string16>& warnings,
    ExtensionInstallUI::PromptType type) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->InstallUIAbort();
    return;
  }

  BrowserWindow* window = browser->window();
  if (!window) {
    delegate->InstallUIAbort();
    return;
  }

  gfx::NativeWindow native_window = window->GetNativeHandle();

  ExtensionInstallDialogController* controller =
      [[ExtensionInstallDialogController alloc]
        initWithParentWindow:native_window
                     profile:profile
                   extension:extension
                    delegate:delegate
                        icon:icon
                    warnings:warnings
                        type:type];

  [controller runAsModalSheet];
}
