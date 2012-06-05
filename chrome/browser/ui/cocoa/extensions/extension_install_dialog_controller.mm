// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using content::OpenURLParams;
using content::Referrer;
using extensions::BundleInstaller;

@interface ExtensionInstallDialogController ()
- (BOOL)isBundleInstall;
- (BOOL)isInlineInstall;
- (void)appendRatingStar:(const SkBitmap*)skiaImage;
@end

namespace {

// Padding above the warnings separator, we must also subtract this when hiding
// it.
const CGFloat kWarningsSeparatorPadding = 14;

// Maximum height we will adjust controls to when trying to accomodate their
// contents.
const CGFloat kMaxControlHeight = 400;

// Adjust the |control|'s height so that its content is not clipped.
// This also adds the change in height to the |totalOffset| and shifts the
// control down by that amount.
void OffsetControlVerticallyToFitContent(NSControl* control,
                                         CGFloat* totalOffset) {
  // Adjust the control's height so that its content is not clipped.
  NSRect currentRect = [control frame];
  NSRect fitRect = currentRect;
  fitRect.size.height = kMaxControlHeight;
  CGFloat desiredHeight = [[control cell] cellSizeForBounds:fitRect].height;
  CGFloat offset = desiredHeight - currentRect.size.height;

  [control setFrameSize:NSMakeSize(currentRect.size.width,
                                   currentRect.size.height + offset)];

  *totalOffset += offset;

  // Move the control vertically by the new total offset.
  NSPoint origin = [control frame].origin;
  origin.y -= *totalOffset;
  [control setFrameOrigin:origin];
}

void AppendRatingStarsShim(const SkBitmap* skiaImage, void* data) {
  ExtensionInstallDialogController* controller =
      static_cast<ExtensionInstallDialogController*>(data);
  [controller appendRatingStar:skiaImage];
}

}

@implementation ExtensionInstallDialogController

@synthesize iconView = iconView_;
@synthesize titleField = titleField_;
@synthesize itemsField = itemsField_;
@synthesize subtitleField = subtitleField_;
@synthesize warningsField = warningsField_;
@synthesize cancelButton = cancelButton_;
@synthesize okButton = okButton_;
@synthesize warningsSeparator = warningsSeparator_;
@synthesize ratingStars = ratingStars_;
@synthesize ratingCountField = ratingCountField_;
@synthesize userCountField = userCountField_;

- (id)initWithParentWindow:(NSWindow*)window
                   profile:(Profile*)profile
                  delegate:(ExtensionInstallUI::Delegate*)delegate
                    prompt:(const ExtensionInstallUI::Prompt&)prompt {
  NSString* nibpath = nil;

  // We use a different XIB in the case of bundle installs, inline installs or
  // no permission warnings. These are laid out nicely for the data they
  // display.
  if (prompt.type() == ExtensionInstallUI::BUNDLE_INSTALL_PROMPT) {
    nibpath = [base::mac::FrameworkBundle()
               pathForResource:@"ExtensionInstallPromptBundle"
                        ofType:@"nib"];
  } else if (prompt.type() == ExtensionInstallUI::INLINE_INSTALL_PROMPT) {
    nibpath = [base::mac::FrameworkBundle()
               pathForResource:@"ExtensionInstallPromptInline"
                        ofType:@"nib"];
  } else if (prompt.GetPermissionCount() == 0) {
    nibpath = [base::mac::FrameworkBundle()
               pathForResource:@"ExtensionInstallPromptNoWarnings"
                        ofType:@"nib"];
  } else {
   nibpath = [base::mac::FrameworkBundle()
              pathForResource:@"ExtensionInstallPrompt"
                       ofType:@"nib"];
  }

  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = window;
    profile_ = profile;
    delegate_ = delegate;
    prompt_.reset(new ExtensionInstallUI::Prompt(prompt));
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

- (IBAction)storeLinkClicked:(id)sender {
  GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                 prompt_->extension()->id());
  browser::FindLastActiveWithProfile(profile_)->OpenURL(OpenURLParams(
      store_url, Referrer(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK,
      false));

  delegate_->InstallUIAbort(/*user_initiated=*/true);
  [NSApp endSheet:[self window]];
}

- (IBAction)cancel:(id)sender {
  delegate_->InstallUIAbort(/*user_initiated=*/true);
  [NSApp endSheet:[self window]];
}

- (IBAction)ok:(id)sender {
  delegate_->InstallUIProceed();
  [NSApp endSheet:[self window]];
}

- (void)awakeFromNib {
  // Make sure we're the window's delegate as set in the nib.
  DCHECK_EQ(self, static_cast<ExtensionInstallDialogController*>(
                      [[self window] delegate]));

  // Set control labels.
  [titleField_ setStringValue:base::SysUTF16ToNSString(prompt_->GetHeading())];
  [okButton_ setTitle:base::SysUTF16ToNSString(
      prompt_->GetAcceptButtonLabel())];
  [cancelButton_ setTitle:prompt_->HasAbortButtonLabel() ?
      base::SysUTF16ToNSString(prompt_->GetAbortButtonLabel()) :
      l10n_util::GetNSString(IDS_CANCEL)];
  if ([self isInlineInstall]) {
    prompt_->AppendRatingStars(AppendRatingStarsShim, self);
    [ratingCountField_ setStringValue:base::SysUTF16ToNSString(
        prompt_->GetRatingCount())];
    [userCountField_ setStringValue:base::SysUTF16ToNSString(
        prompt_->GetUserCount())];
  }

  // The bundle install dialog has no icon.
  if (![self isBundleInstall])
    [iconView_ setImage:prompt_->icon().ToNSImage()];

  // The dialog is laid out in the NIB exactly how we want it assuming that
  // each label fits on one line. However, for each label, we want to allow
  // wrapping onto multiple lines. So we accumulate an offset by measuring how
  // big each label wants to be, and comparing it to how big it actually is.
  // Then we shift each label down and resize by the appropriate amount, then
  // finally resize the window.
  CGFloat totalOffset = 0.0;

  OffsetControlVerticallyToFitContent(titleField_, &totalOffset);

  // Resize |okButton_| and |cancelButton_| to fit the button labels, but keep
  // them right-aligned.
  NSSize buttonDelta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:okButton_];
  if (buttonDelta.width) {
    [okButton_ setFrame:NSOffsetRect([okButton_ frame], -buttonDelta.width, 0)];
    [cancelButton_ setFrame:NSOffsetRect([cancelButton_ frame],
                                         -buttonDelta.width, 0)];
  }
  buttonDelta = [GTMUILocalizerAndLayoutTweaker sizeToFitView:cancelButton_];
  if (buttonDelta.width) {
    [cancelButton_ setFrame:NSOffsetRect([cancelButton_ frame],
                                         -buttonDelta.width, 0)];
  }

  if ([self isBundleInstall]) {
    [subtitleField_ setStringValue:base::SysUTF16ToNSString(
        prompt_->GetPermissionsHeading())];

    // We display the list of extension names as a simple text string, seperated
    // by newlines.
    BundleInstaller::ItemList items = prompt_->bundle()->GetItemsWithState(
        BundleInstaller::Item::STATE_PENDING);

    NSMutableString* joinedItems = [NSMutableString string];
    for (size_t i = 0; i < items.size(); ++i) {
      if (i > 0)
        [joinedItems appendString:@"\n"];
      [joinedItems appendString:base::SysUTF16ToNSString(
          items[i].GetNameForDisplay())];
    }
    [itemsField_ setStringValue:joinedItems];

    // Adjust the controls to fit the list of extensions.
    OffsetControlVerticallyToFitContent(itemsField_, &totalOffset);
  }

  // If there are any warnings, then we have to do some special layout.
  if (prompt_->GetPermissionCount() > 0) {
    [subtitleField_ setStringValue:base::SysUTF16ToNSString(
        prompt_->GetPermissionsHeading())];

    // We display the permission warnings as a simple text string, separated by
    // newlines.
    NSMutableString* joinedWarnings = [NSMutableString string];
    for (size_t i = 0; i < prompt_->GetPermissionCount(); ++i) {
      if (i > 0)
        [joinedWarnings appendString:@"\n"];
      [joinedWarnings appendString:base::SysUTF16ToNSString(
          prompt_->GetPermission(i))];
    }
    [warningsField_ setStringValue:joinedWarnings];

    // In the store version of the dialog the icon extends past the one-line
    // version of the permission field. Therefore when increasing the window
    // size for multi-line permissions we don't have to add the full offset,
    // only the part that extends past the icon.
    CGFloat warningsGrowthSlack = 0;
    if (![self isBundleInstall] &&
        [warningsField_ frame].origin.y > [iconView_ frame].origin.y) {
      warningsGrowthSlack =
          [warningsField_ frame].origin.y - [iconView_ frame].origin.y;
    }

    // Adjust the controls to fit the permission warnings.
    OffsetControlVerticallyToFitContent(subtitleField_, &totalOffset);
    OffsetControlVerticallyToFitContent(warningsField_, &totalOffset);

    totalOffset = MAX(totalOffset - warningsGrowthSlack, 0);
  } else if ([self isInlineInstall] || [self isBundleInstall]) {
    // Inline and bundle installs that don't have a permissions section need to
    // hide controls related to that and shrink the window by the space they
    // take up.
    NSRect hiddenRect = NSUnionRect([warningsSeparator_ frame],
                                    [subtitleField_ frame]);
    hiddenRect = NSUnionRect(hiddenRect, [warningsField_ frame]);
    [warningsSeparator_ setHidden:YES];
    [subtitleField_ setHidden:YES];
    [warningsField_ setHidden:YES];
    totalOffset -= hiddenRect.size.height + kWarningsSeparatorPadding;
  }

  // If necessary, adjust the window size.
  if (totalOffset) {
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

- (BOOL)isBundleInstall {
  return prompt_->type() == ExtensionInstallUI::BUNDLE_INSTALL_PROMPT;
}

- (BOOL)isInlineInstall {
  return prompt_->type() == ExtensionInstallUI::INLINE_INSTALL_PROMPT;
}

- (void)appendRatingStar:(const SkBitmap*)skiaImage {
  NSImage* image = gfx::SkBitmapToNSImageWithColorSpace(
      *skiaImage, base::mac::GetSystemColorSpace());
  NSRect frame = NSMakeRect(0, 0, skiaImage->width(), skiaImage->height());
  scoped_nsobject<NSImageView> view([[NSImageView alloc] initWithFrame:frame]);
  [view setImage:image];

  // Add this star after all the other ones
  CGFloat maxStarRight = 0;
  if ([[ratingStars_ subviews] count]) {
    maxStarRight = NSMaxX([[[ratingStars_ subviews] lastObject] frame]);
  }
  NSRect starBounds = NSMakeRect(maxStarRight, 0,
                                 skiaImage->width(), skiaImage->height());
  [view setFrame:starBounds];
  [ratingStars_ addSubview:view];
}

@end  // ExtensionInstallDialogController

void ShowExtensionInstallDialogImpl(
    Profile* profile,
    ExtensionInstallUI::Delegate* delegate,
    const ExtensionInstallUI::Prompt& prompt) {
  Browser* browser = browser::FindLastActiveWithProfile(profile);
  if (!browser) {
    delegate->InstallUIAbort(false);
    return;
  }

  BrowserWindow* window = browser->window();
  if (!window) {
    delegate->InstallUIAbort(false);
    return;
  }

  gfx::NativeWindow native_window = window->GetNativeHandle();

  ExtensionInstallDialogController* controller =
      [[ExtensionInstallDialogController alloc]
        initWithParentWindow:native_window
                     profile:profile
                    delegate:delegate
                      prompt:prompt];

  // TODO(mihaip): Switch this to be tab-modal (http://crbug.com/95455)
  [controller runAsModalSheet];
}
