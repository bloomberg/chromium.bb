// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
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
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface ExtensionInstallDialogController ()
- (bool)isInlineInstall;
- (void)appendRatingStar:(const SkBitmap*)skiaImage;
@end

namespace {

// Padding above the warnings separator, we must also subtract this when hiding
// it.
const CGFloat kWarningsSeparatorPadding = 14;

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

void AppendRatingStarsShim(const SkBitmap* skiaImage, void* data) {
  ExtensionInstallDialogController* controller =
      static_cast<ExtensionInstallDialogController*>(data);
  [controller appendRatingStar:skiaImage];
}

}

@implementation ExtensionInstallDialogController

@synthesize iconView = iconView_;
@synthesize titleField = titleField_;
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
                 extension:(const Extension*)extension
                  delegate:(ExtensionInstallUI::Delegate*)delegate
                      icon:(SkBitmap*)icon
                    prompt:(const ExtensionInstallUI::Prompt&)prompt {
  NSString* nibpath = nil;

  // We use a different XIB in the case of inline installs or no permission
  // warnings, that respectively show webstore ratings data and are a more
  // nicely laid out.
  if (prompt.type() == ExtensionInstallUI::INLINE_INSTALL_PROMPT) {
    nibpath = [base::mac::MainAppBundle()
               pathForResource:@"ExtensionInstallPromptInline"
                        ofType:@"nib"];
  } else if (prompt.GetPermissionCount() == 0) {
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
    extension_ = extension;
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
  GURL store_url(
      extension_urls::GetWebstoreItemDetailURLPrefix() + extension_->id());
  BrowserList::GetLastActiveWithProfile(profile_)->
      OpenURL(store_url, GURL(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK);

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
  [titleField_ setStringValue:base::SysUTF16ToNSString(
      prompt_->GetHeading(extension_->name()))];
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

  NSImage* image = gfx::SkBitmapToNSImage(icon_);
  [iconView_ setImage:image];

  // Resize |titleField_| to fit the title.
  CGFloat originalTitleWidth = [titleField_ frame].size.width;
  [titleField_ sizeToFit];
  CGFloat newTitleWidth = [titleField_ frame].size.width;
  if (newTitleWidth > originalTitleWidth) {
    NSRect frame = [[self window] frame];
    frame.size.width += newTitleWidth - originalTitleWidth;
    [[self window] setFrame:frame display:NO];
  }

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

  CGFloat totalOffset = 0.0;
  // If there are any warnings, then we have to do some special layout.
  if (prompt_->GetPermissionCount() > 0) {
    [subtitleField_ setStringValue:base::SysUTF16ToNSString(
        prompt_->GetPermissionsHeader())];

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

    // The dialog is laid out in the NIB exactly how we want it assuming that
    // each label fits on one line. However, for each label, we want to allow
    // wrapping onto multiple lines. So we accumulate an offset by measuring how
    // big each label wants to be, and comparing it to how big it actually is.
    // Then we shift each label down and resize by the appropriate amount, then
    // finally resize the window.

    // Additionally, in the store version of the dialog the icon extends past
    // the one-line version of the permission field. Therefore when increasing
    // the window size for multi-line permissions we don't have to add the full
    // offset, only the part that extends past the icon.
    CGFloat warningsGrowthSlack = 0;
    if ([warningsField_ frame].origin.y > [iconView_ frame].origin.y) {
      warningsGrowthSlack =
          [warningsField_ frame].origin.y - [iconView_ frame].origin.y;
    }

    totalOffset += AdjustControlHeightToFitContent(subtitleField_);
    OffsetControlVertically(subtitleField_, -totalOffset);

    totalOffset += AdjustControlHeightToFitContent(warningsField_);
    OffsetControlVertically(warningsField_, -totalOffset);
    totalOffset = MAX(totalOffset - warningsGrowthSlack, 0);
  } else if ([self isInlineInstall]) {
    // Inline installs that don't have a permissions section need to hide
    // controls related to that and shrink the window by the space they take
    // up.
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

- (bool)isInlineInstall {
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
    const Extension* extension,
    SkBitmap* icon,
    const ExtensionInstallUI::Prompt& prompt) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
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
                   extension:extension
                    delegate:delegate
                        icon:icon
                      prompt:prompt];

  // TODO(mihaip): Switch this to be tab-modal (http://crbug.com/95455)
  [controller runAsModalSheet];
}
