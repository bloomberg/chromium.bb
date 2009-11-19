// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/app/keystone_glue.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/about_window_controller.h"
#import "chrome/browser/cocoa/background_tile_view.h"
#include "chrome/browser/cocoa/restart_browser.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/locale_settings.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace {

void AttributedStringAppendString(NSMutableAttributedString* attr_str,
                                  NSString* str) {
  // You might think doing [[attr_str mutableString] appendString:str] would
  // work, but it causes any trailing style to get extened, meaning as we
  // append links, they grow to include the new text, not what we want.
  NSAttributedString* new_attr_str =
      [[[NSAttributedString alloc] initWithString:str] autorelease];
  [attr_str appendAttributedString:new_attr_str];
}

void AttributedStringAppendHyperlink(NSMutableAttributedString* attr_str,
                                     NSString* text, NSString* url_str) {
  // Figure out the range of the text we're adding and add the text.
  NSRange range = NSMakeRange([attr_str length], [text length]);
  AttributedStringAppendString(attr_str, text);

  // Add the link
  [attr_str addAttribute:NSLinkAttributeName value:url_str range:range];

  // Blue and underlined
  [attr_str addAttribute:NSForegroundColorAttributeName
                   value:[NSColor blueColor]
                   range:range];
  [attr_str addAttribute:NSUnderlineStyleAttributeName
                   value:[NSNumber numberWithInt:NSSingleUnderlineStyle]
                   range:range];
  [attr_str addAttribute:NSCursorAttributeName
                   value:[NSCursor pointingHandCursor]
                   range:range];
}

}  // namespace

@interface AboutWindowController(Private)

// Launches a check for available updates.
- (void)checkForUpdate;

// Notification callback, called with the status of asynchronous
// -checkForUpdate and -updateNow: operations.
- (void)updateStatus:(NSNotification*)notification;

// These methods maintain the image (or throbber) and text displayed regarding
// update status.  -setUpdateThrobberMessage: starts a progress throbber and
// sets the text.  -setUpdateImage:message: displays an image and sets the
// text.
- (void)setUpdateThrobberMessage:(NSString*)message;
- (void)setUpdateImage:(int)imageID message:(NSString*)message;

@end  // @interface AboutWindowController(Private)

@implementation AboutWindowController

- (id)initWithProfile:(Profile*)profile {
  NSString* nibPath = [mac_util::MainAppBundle() pathForResource:@"About"
                                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    profile_ = profile;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(updateStatus:)
                   name:kAutoupdateStatusNotification
                 object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// YES when an About box is currently showing the kAutoupdateInstallFailed
// status, or if no About box is visible, if the most recent About box to be
// closed was closed while showing this status.  When an About box opens, if
// the recent status is kAutoupdateInstallFailed and
// recentShownInstallFailedStatus is NO, the failure needs to be shown instead
// of launching a new update check.  recentShownInstallFailedStatus is
// maintained by -updateStatus:.
static BOOL recentShownInstallFailedStatus = NO;

- (void)awakeFromNib {
  NSBundle* bundle = mac_util::MainAppBundle();
  NSString* chromeVersion =
      [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

#if defined(GOOGLE_CHROME_BUILD)
  NSString* version = chromeVersion;
#else  // GOOGLE_CHROME_BUILD
  // The format string is not localized, but this is how the displayed version
  // is built on Windows too.
  NSString* svnRevision = [bundle objectForInfoDictionaryKey:@"SVNRevision"];
  NSString* version =
      [NSString stringWithFormat:@"%@ (%@)", chromeVersion, svnRevision];
#endif  // GOOGLE_CHROME_BUILD

  [version_ setStringValue:version];

  // Put the two images into the UI.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* backgroundImage = rb.GetNSImageNamed(IDR_ABOUT_BACKGROUND_COLOR);
  DCHECK(backgroundImage);
  [backgroundView_ setTileImage:backgroundImage];
  NSImage* logoImage = rb.GetNSImageNamed(IDR_ABOUT_BACKGROUND);
  DCHECK(logoImage);
  [logoView_ setImage:logoImage];

  [[legalText_ textStorage] setAttributedString:[[self class] legalTextBlock]];

  // Resize our text view now so that the |updateShift| below is set
  // correctly. The About box has its controls manually positioned, so we need
  // to calculate how much larger (or smaller) our text box is and store that
  // difference in |legalShift|. We do something similar with |updateShift|
  // below, which is either 0, or the amount of space to offset the window size
  // because the view that contains the update button has been removed because
  // this build doesn't have KeyStone.
  NSRect oldLegalRect = [legalBlock_ frame];
  [legalText_ sizeToFit];
  NSRect newRect = oldLegalRect;
  newRect.size.height = [legalText_ frame].size.height;
  [legalBlock_ setFrame:newRect];
  CGFloat legalShift = newRect.size.height - oldLegalRect.size.height;

  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  CGFloat updateShift;
  if (keystoneGlue) {
    if ([keystoneGlue asyncOperationPending] ||
        ([keystoneGlue recentStatus] == kAutoupdateInstallFailed &&
         !recentShownInstallFailedStatus)) {
      // If an asynchronous update operation is currently pending, such as a
      // check for updates or an update installation attempt, set the status
      // up correspondingly without launching a new update check.
      //
      // If a previous update attempt was unsuccessful but no About box was
      // around to report the error, show it now, and allow another chance to
      // install the update.
      [self updateStatus:[keystoneGlue recentNotification]];
    } else {
      // Launch a new update check, even if one was already completed, because
      // a new update may be available or a new update may have been installed
      // in the background since the last time an About box was displayed.
      [self checkForUpdate];
    }

    updateShift = 0.0;
  } else {
    // Hide all the update UI
    [updateBlock_ setHidden:YES];

    // Figure out the amount being removed by taking out the update block
    // and its spacing.
    updateShift = NSMinY([legalBlock_ frame]) - NSMinY([updateBlock_ frame]);

    NSRect legalFrame = [legalBlock_ frame];
    legalFrame.origin.y -= updateShift;
    [legalBlock_ setFrame:legalFrame];
  }

  NSRect backgroundFrame = [backgroundView_ frame];
  backgroundFrame.origin.y += legalShift - updateShift;
  [backgroundView_ setFrame:backgroundFrame];

  NSSize windowDelta = NSMakeSize(0.0, legalShift - updateShift);

  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:[self window]
                                        delta:windowDelta];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

- (void)setUpdateThrobberMessage:(NSString*)message {
  [updateStatusIndicator_ setHidden:YES];

  [spinner_ setHidden:NO];
  [spinner_ startAnimation:self];

  [updateText_ setStringValue:message];
}

- (void)setUpdateImage:(int)imageID message:(NSString*)message {
  [spinner_ stopAnimation:self];
  [spinner_ setHidden:YES];

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* statusImage = rb.GetNSImageNamed(imageID);
  DCHECK(statusImage);
  [updateStatusIndicator_ setImage:statusImage];
  [updateStatusIndicator_ setHidden:NO];

  [updateText_ setStringValue:message];
}

- (void)checkForUpdate {
  [[KeystoneGlue defaultKeystoneGlue] checkForUpdate];

  // Immediately, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with status kAutoupdateChecking.
  //
  // Upon completion, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with a status indicating the result of the
  // check.
}

- (IBAction)updateNow:(id)sender {
  updateTriggered_ = YES;

  [[KeystoneGlue defaultKeystoneGlue] installUpdate];

  // Immediately, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with status kAutoupdateInstalling.
  //
  // Upon completion, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with a status indicating the result of the
  // installation attempt.
}

- (void)updateStatus:(NSNotification*)notification {
  recentShownInstallFailedStatus = NO;

  NSDictionary* dictionary = [notification userInfo];
  AutoupdateStatus status = static_cast<AutoupdateStatus>(
      [[dictionary objectForKey:kAutoupdateStatusStatus] intValue]);

  // Don't assume |version| is a real string.  It may be nil.
  NSString* version = [dictionary objectForKey:kAutoupdateStatusVersion];

  bool throbber = false;
  int imageID = 0;
  NSString* message;

  switch (status) {
    case kAutoupdateChecking:
      throbber = true;
      message = l10n_util::GetNSStringWithFixup(IDS_UPGRADE_CHECK_STARTED);

      break;

    case kAutoupdateCurrent:
      imageID = IDR_UPDATE_UPTODATE;
      message = l10n_util::GetNSStringFWithFixup(
          IDS_UPGRADE_ALREADY_UP_TO_DATE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          base::SysNSStringToUTF16(version));

      break;

    case kAutoupdateAvailable:
      imageID = IDR_UPDATE_AVAILABLE;
      message = l10n_util::GetNSStringFWithFixup(
          IDS_UPGRADE_AVAILABLE, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
      [updateNowButton_ setEnabled:YES];

      break;

    case kAutoupdateInstalling:
      // Don't let anyone click "Update Now" twice.
      [updateNowButton_ setEnabled:NO];

      throbber = true;
      message = l10n_util::GetNSStringWithFixup(IDS_UPGRADE_STARTED);

      break;

    case kAutoupdateInstalled:
      {
        imageID = IDR_UPDATE_UPTODATE;
        string16 productName = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
        if (version) {
          message = l10n_util::GetNSStringFWithFixup(
              IDS_UPGRADE_SUCCESSFUL,
              productName,
              base::SysNSStringToUTF16(version));
        } else {
          message = l10n_util::GetNSStringFWithFixup(
              IDS_UPGRADE_SUCCESSFUL_NOVERSION, productName);
        }

        // TODO(mark): Turn the button in the dialog into a restart button
        // instead of springing this sheet or dialog.
        NSWindow* window = [self window];
        NSWindow* restartDialogParent = [window isVisible] ? window : nil;
        restart_browser::RequestRestart(restartDialogParent);
      }

      break;

    case kAutoupdateInstallFailed:
      recentShownInstallFailedStatus = YES;

      // Allow another chance.
      [updateNowButton_ setEnabled:YES];

      // Fall through.

    case kAutoupdateCheckFailed:
      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 IntToString16(status));

      break;

    default:
      NOTREACHED();
      return;
  }

  if (throbber) {
    [self setUpdateThrobberMessage:message];
  } else {
    DCHECK_NE(imageID, 0);
    [self setUpdateImage:imageID message:message];
  }
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  // We always create a new window, so there's no need to try to re-use
  // an existing one just to pass in the NEW_WINDOW disposition.
  Browser* browser = Browser::Create(profile_);
  if (browser) {
    browser->OpenURL(GURL([link UTF8String]), GURL(), NEW_WINDOW,
                     PageTransition::LINK);
  }
  return YES;
}

- (NSTextView*)legalText {
  return legalText_;
}

- (NSButton*)updateButton {
  return updateNowButton_;
}

- (NSTextField*)updateText {
  return updateText_;
}

+ (NSAttributedString*)legalTextBlock {
  // Windows builds this up in a very complex way, we're just trying to model
  // it the best we can to get all the information in (they actually do it
  // but created Labels and Links that they carefully place to make it appear
  // to be a paragraph of text).
  // src/chrome/browser/views/about_chrome_view.cc AboutChromeView::Init()

  NSMutableAttributedString* legal_block =
      [[[NSMutableAttributedString alloc] init] autorelease];
  [legal_block beginEditing];

  NSString* copyright =
      l10n_util::GetNSStringWithFixup(IDS_ABOUT_VERSION_COPYRIGHT);
  AttributedStringAppendString(legal_block, copyright);

  // These are the markers directly in IDS_ABOUT_VERSION_LICENSE
  NSString* kBeginLinkChr = @"BEGIN_LINK_CHR";
  NSString* kBeginLinkOss = @"BEGIN_LINK_OSS";
  NSString* kEndLinkChr = @"END_LINK_CHR";
  NSString* kEndLinkOss = @"END_LINK_OSS";
  // The CHR link should go to here
  NSString* kChromiumProject = l10n_util::GetNSString(IDS_CHROMIUM_PROJECT_URL);
  // The OSS link should go to here
  NSString* kAcknowledgements = @"about:credits";

  // Now fetch the license string and deal with the markers

  NSString* license =
      l10n_util::GetNSStringWithFixup(IDS_ABOUT_VERSION_LICENSE);

  NSRange begin_chr = [license rangeOfString:kBeginLinkChr];
  NSRange begin_oss = [license rangeOfString:kBeginLinkOss];
  NSRange end_chr = [license rangeOfString:kEndLinkChr];
  NSRange end_oss = [license rangeOfString:kEndLinkOss];
  DCHECK_NE(begin_chr.location, NSNotFound);
  DCHECK_NE(begin_oss.location, NSNotFound);
  DCHECK_NE(end_chr.location, NSNotFound);
  DCHECK_NE(end_oss.location, NSNotFound);

  // We don't know which link will come first, so we have to deal with things
  // like this:
  //   [text][begin][text][end][text][start][text][end][text]

  bool chromium_link_first = begin_chr.location < begin_oss.location;

  NSRange* begin1 = &begin_chr;
  NSRange* begin2 = &begin_oss;
  NSRange* end1 = &end_chr;
  NSRange* end2 = &end_oss;
  NSString* link1 = kChromiumProject;
  NSString* link2 = kAcknowledgements;
  if (!chromium_link_first) {
    // OSS came first, switch!
    begin2 = &begin_chr;
    begin1 = &begin_oss;
    end2 = &end_chr;
    end1 = &end_oss;
    link2 = kChromiumProject;
    link1 = kAcknowledgements;
  }

  NSString *sub_str;

  AttributedStringAppendString(legal_block, @"\n");
  sub_str = [license substringWithRange:NSMakeRange(0, begin1->location)];
  AttributedStringAppendString(legal_block, sub_str);
  sub_str = [license substringWithRange:NSMakeRange(NSMaxRange(*begin1),
                                                    end1->location -
                                                      NSMaxRange(*begin1))];
  AttributedStringAppendHyperlink(legal_block, sub_str, link1);
  sub_str = [license substringWithRange:NSMakeRange(NSMaxRange(*end1),
                                                    begin2->location -
                                                      NSMaxRange(*end1))];
  AttributedStringAppendString(legal_block, sub_str);
  sub_str = [license substringWithRange:NSMakeRange(NSMaxRange(*begin2),
                                                    end2->location -
                                                      NSMaxRange(*begin2))];
  AttributedStringAppendHyperlink(legal_block, sub_str, link2);
  sub_str = [license substringWithRange:NSMakeRange(NSMaxRange(*end2),
                                                    [license length] -
                                                      NSMaxRange(*end2))];
  AttributedStringAppendString(legal_block, sub_str);

#if defined(GOOGLE_CHROME_BUILD)
  // Terms of service is only valid for Google Chrome

  // The url within terms should point here:
  NSString* kTOS = @"about:terms";
  // Following Window. There is one marker in the string for where the terms
  // link goes, but the text of the link comes from a second string resources.
  std::vector<size_t> url_offsets;
  std::wstring w_about_terms = l10n_util::GetStringF(IDS_ABOUT_TERMS_OF_SERVICE,
                                                     std::wstring(),
                                                     std::wstring(),
                                                     &url_offsets);
  DCHECK_EQ(url_offsets.size(), 1U);
  NSString* about_terms = base::SysWideToNSString(w_about_terms);
  NSString* terms_link_text =
      l10n_util::GetNSStringWithFixup(IDS_TERMS_OF_SERVICE);

  AttributedStringAppendString(legal_block, @"\n\n");
  sub_str = [about_terms substringToIndex:url_offsets[0]];
  AttributedStringAppendString(legal_block, sub_str);
  AttributedStringAppendHyperlink(legal_block, terms_link_text, kTOS);
  sub_str = [about_terms substringFromIndex:url_offsets[0]];
  AttributedStringAppendString(legal_block, sub_str);
#endif  // GOOGLE_CHROME_BUILD

  // We need to explicitly select Lucida Grande because once we click on
  // the NSTextView, it changes to Helvetica 12 otherwise.
  NSRange string_range = NSMakeRange(0, [legal_block length]);
  [legal_block addAttribute:NSFontAttributeName
                      value:[NSFont labelFontOfSize:11]
                      range:string_range];

  [legal_block endEditing];
  return legal_block;
}

@end  // @implementation AboutWindowController
