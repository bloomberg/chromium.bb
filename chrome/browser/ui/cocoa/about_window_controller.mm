// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/about_window_controller.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/keystone_glue.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/platform_util.h"
#import "chrome/browser/ui/cocoa/background_tile_view.h"
#include "chrome/browser/ui/cocoa/restart_browser.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/locale_settings.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

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

// Turns the update and promotion blocks on and off as needed based on whether
// updates are possible and promotion is desired or required.
- (void)adjustUpdateUIVisibility;

// Maintains the update and promotion block visibility and window sizing.
// This uses bool instead of BOOL for the convenience of the internal
// implementation.
- (void)setAllowsUpdate:(bool)update allowsPromotion:(bool)promotion;

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

@implementation AboutLegalTextView

// Never draw the insertion point (otherwise, it shows up without any user
// action if full keyboard accessibility is enabled).
- (BOOL)shouldDrawInsertionPoint {
  return NO;
}

@end

@implementation AboutWindowController

- (id)initWithProfile:(Profile*)profile {
  NSString* nibPath = [base::mac::MainAppBundle() pathForResource:@"About"
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
// the recent status is kAutoupdateInstallFailed or kAutoupdatePromoteFailed
// and recentShownUserActionFailedStatus is NO, the failure needs to be shown
// instead of launching a new update check.  recentShownInstallFailedStatus is
// maintained by -updateStatus:.
static BOOL recentShownUserActionFailedStatus = NO;

- (void)awakeFromNib {
  NSBundle* bundle = base::mac::MainAppBundle();
  NSString* chromeVersion =
      [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

  NSString* versionModifier = @"";
  NSString* svnRevision = @"";
  std::string modifier = platform_util::GetVersionStringModifier();
  if (!modifier.empty())
    versionModifier = [NSString stringWithFormat:@" %@",
                                base::SysUTF8ToNSString(modifier)];

#if !defined(GOOGLE_CHROME_BUILD)
  svnRevision = [NSString stringWithFormat:@" (%@)",
                          [bundle objectForInfoDictionaryKey:@"SVNRevision"]];
#endif
  // The format string is not localized, but this is how the displayed version
  // is built on Windows too.
  NSString* version =
    [NSString stringWithFormat:@"%@%@%@",
              chromeVersion, svnRevision, versionModifier];

  [version_ setStringValue:version];

  // Put the two images into the UI.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* backgroundImage = rb.GetNativeImageNamed(IDR_ABOUT_BACKGROUND_COLOR);
  DCHECK(backgroundImage);
  [backgroundView_ setTileImage:backgroundImage];
  NSImage* logoImage = rb.GetNativeImageNamed(IDR_ABOUT_BACKGROUND);
  DCHECK(logoImage);
  [logoView_ setImage:logoImage];

  [[legalText_ textStorage] setAttributedString:[[self class] legalTextBlock]];

  // Resize our text view now so that the |updateShift| below is set
  // correctly. The About box has its controls manually positioned, so we need
  // to calculate how much larger (or smaller) our text box is and store that
  // difference in |legalShift|. We do something similar with |updateShift|
  // below, which is either 0, or the amount of space to offset the window size
  // because the view that contains the update button has been removed because
  // this build doesn't have Keystone.
  NSRect oldLegalRect = [legalBlock_ frame];
  [legalText_ sizeToFit];
  NSRect newRect = oldLegalRect;
  newRect.size.height = [legalText_ frame].size.height;
  [legalBlock_ setFrame:newRect];
  CGFloat legalShift = newRect.size.height - oldLegalRect.size.height;

  NSRect backgroundFrame = [backgroundView_ frame];
  backgroundFrame.origin.y += legalShift;
  [backgroundView_ setFrame:backgroundFrame];

  NSSize windowDelta = NSMakeSize(0.0, legalShift);
  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:[self window]
                                        delta:windowDelta];

  windowHeight_ = [[self window] frame].size.height;

  [self adjustUpdateUIVisibility];

  // Don't do anything update-related if adjustUpdateUIVisibility decided that
  // updates aren't possible.
  if (![updateBlock_ isHidden]) {
    KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
    AutoupdateStatus recentStatus = [keystoneGlue recentStatus];
    if ([keystoneGlue asyncOperationPending] ||
        recentStatus == kAutoupdateRegisterFailed ||
        ((recentStatus == kAutoupdateInstallFailed ||
          recentStatus == kAutoupdatePromoteFailed) &&
         !recentShownUserActionFailedStatus)) {
      // If an asynchronous update operation is currently pending, such as a
      // check for updates or an update installation attempt, set the status
      // up correspondingly without launching a new update check.
      //
      // If registration failed, no other operations make sense, so just go
      // straight to the error.
      //
      // If a previous update or promotion attempt was unsuccessful but no
      // About box was around to report the error, show it now, and allow
      // another chance to perform the action.
      [self updateStatus:[keystoneGlue recentNotification]];
    } else {
      // Launch a new update check, even if one was already completed, because
      // a new update may be available or a new update may have been installed
      // in the background since the last time an About box was displayed.
      [self checkForUpdate];
    }
  }

  [[self window] center];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

- (void)adjustUpdateUIVisibility {
  bool allowUpdate;
  bool allowPromotion;

  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  if (keystoneGlue && ![keystoneGlue isOnReadOnlyFilesystem]) {
    AutoupdateStatus recentStatus = [keystoneGlue recentStatus];
    if (recentStatus == kAutoupdateRegistering ||
        recentStatus == kAutoupdateRegisterFailed ||
        recentStatus == kAutoupdatePromoted) {
      // Show the update block while registering so that there's a progress
      // spinner, and if registration failed so that there's an error message.
      // Show it following a promotion because updates should be possible
      // after promotion successfully completes.
      allowUpdate = true;

      // Promotion isn't possible at this point.
      allowPromotion = false;
    } else if (recentStatus == kAutoupdatePromoteFailed) {
      // TODO(mark): Add kAutoupdatePromoting to this block.  KSRegistration
      // currently handles the promotion synchronously, meaning that the main
      // thread's loop doesn't spin, meaning that animations and other updates
      // to the window won't occur until KSRegistration is done with
      // promotion.  This looks laggy and bad and probably qualifies as
      // "jank."  For now, there just won't be any visual feedback while
      // promotion is in progress, but it should complete (or fail) very
      // quickly.  http://b/2290009.
      //
      // Also see the TODO for kAutoupdatePromoting in -updateStatus:version:.
      //
      // Show the update block so that there's some visual feedback that
      // promotion is under way or that it's failed.  Show the promotion block
      // because the user either just clicked that button or because the user
      // should be able to click it again.
      allowUpdate = true;
      allowPromotion = true;
    } else {
      // Show the update block only if a promotion is not absolutely required.
      allowUpdate = ![keystoneGlue needsPromotion];

      // Show the promotion block if promotion is a possibility.
      allowPromotion = [keystoneGlue wantsPromotion];
    }
  } else {
    // There is no glue, or the application is on a read-only filesystem.
    // Updates and promotions are impossible.
    allowUpdate = false;
    allowPromotion = false;
  }

  [self setAllowsUpdate:allowUpdate allowsPromotion:allowPromotion];
}

- (void)setAllowsUpdate:(bool)update allowsPromotion:(bool)promotion {
  bool oldUpdate = ![updateBlock_ isHidden];
  bool oldPromotion = ![promoteButton_ isHidden];

  if (promotion == oldPromotion && update == oldUpdate) {
    return;
  }

  NSRect updateFrame = [updateBlock_ frame];
  CGFloat delta = 0.0;

  if (update != oldUpdate) {
    [updateBlock_ setHidden:!update];
    delta += (update ? 1.0 : -1.0) * NSHeight(updateFrame);
  }

  if (promotion != oldPromotion) {
    [promoteButton_ setHidden:!promotion];
  }

  NSRect legalFrame = [legalBlock_ frame];

  if (delta) {
    updateFrame.origin.y += delta;
    [updateBlock_ setFrame:updateFrame];

    legalFrame.origin.y += delta;
    [legalBlock_ setFrame:legalFrame];

    NSRect backgroundFrame = [backgroundView_ frame];
    backgroundFrame.origin.y += delta;
    [backgroundView_ setFrame:backgroundFrame];

    // GTMUILocalizerAndLayoutTweaker resizes the window without any
    // opportunity for animation.  In order to animate, disable window
    // updates, save the current frame, let GTMUILocalizerAndLayoutTweaker do
    // its thing, save the desired frame, restore the original frame, and then
    // animate.
    NSWindow* window = [self window];
    [window disableScreenUpdatesUntilFlush];

    NSRect oldFrame = [window frame];

    // GTMUILocalizerAndLayoutTweaker applies its delta to the window's
    // current size (like oldFrame.size), but oldFrame isn't trustworthy if
    // an animation is in progress.  Set the window's frame to
    // intermediateFrame, which is a frame of the size that an existing
    // animation is animating to, so that GTM can apply the delta to the right
    // size.
    NSRect intermediateFrame = oldFrame;
    intermediateFrame.origin.y -= intermediateFrame.size.height - windowHeight_;
    intermediateFrame.size.height = windowHeight_;
    [window setFrame:intermediateFrame display:NO];

    NSSize windowDelta = NSMakeSize(0.0, delta);
    [GTMUILocalizerAndLayoutTweaker
        resizeWindowWithoutAutoResizingSubViews:window
                                          delta:windowDelta];
    [window setFrameTopLeftPoint:NSMakePoint(NSMinX(intermediateFrame),
                                             NSMaxY(intermediateFrame))];
    NSRect newFrame = [window frame];

    windowHeight_ += delta;

    if (![[self window] isVisible]) {
      // Don't animate if the window isn't on screen yet.
      [window setFrame:newFrame display:NO];
    } else {
      [window setFrame:oldFrame display:NO];
      [window setFrame:newFrame display:YES animate:YES];
    }
  }
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
  NSImage* statusImage = rb.GetNativeImageNamed(imageID);
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
  [[KeystoneGlue defaultKeystoneGlue] installUpdate];

  // Immediately, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with status kAutoupdateInstalling.
  //
  // Upon completion, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with a status indicating the result of the
  // installation attempt.
}

- (IBAction)promoteUpdater:(id)sender {
  [[KeystoneGlue defaultKeystoneGlue] promoteTicket];

  // Immediately, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with status kAutoupdatePromoting.
  //
  // Upon completion, kAutoupdateStatusNotification will be posted, and
  // -updateStatus: will be called with a status indicating a result of the
  // installation attempt.
  //
  // If the promotion was successful, KeystoneGlue will re-register the ticket
  // and -updateStatus: will be called again indicating first that
  // registration is in progress and subsequently that it has completed.
}

- (void)updateStatus:(NSNotification*)notification {
  recentShownUserActionFailedStatus = NO;

  NSDictionary* dictionary = [notification userInfo];
  AutoupdateStatus status = static_cast<AutoupdateStatus>(
      [[dictionary objectForKey:kAutoupdateStatusStatus] intValue]);

  // Don't assume |version| is a real string.  It may be nil.
  NSString* version = [dictionary objectForKey:kAutoupdateStatusVersion];

  bool updateMessage = true;
  bool throbber = false;
  int imageID = 0;
  NSString* message;
  bool enableUpdateButton = false;
  bool enablePromoteButton = true;

  switch (status) {
    case kAutoupdateRegistering:
      // When registering, use the "checking" message.  The check will be
      // launched if appropriate immediately after registration.
      throbber = true;
      message = l10n_util::GetNSStringWithFixup(IDS_UPGRADE_CHECK_STARTED);
      enablePromoteButton = false;

      break;

    case kAutoupdateRegistered:
      // Once registered, the ability to update and promote is known.
      [self adjustUpdateUIVisibility];

      if (![updateBlock_ isHidden]) {
        // If registration completes while the window is visible, go straight
        // into an update check.  Return immediately, this routine will be
        // re-entered shortly with kAutoupdateChecking.
        [self checkForUpdate];
        return;
      }

      // Nothing actually failed, but updates aren't possible.  The throbber
      // and message are hidden, but they'll be reset to these dummy values
      // just to get the throbber to stop spinning if it's running.
      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 base::IntToString16(status));

      break;

    case kAutoupdateChecking:
      throbber = true;
      message = l10n_util::GetNSStringWithFixup(IDS_UPGRADE_CHECK_STARTED);
      enablePromoteButton = false;

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
      enableUpdateButton = true;

      break;

    case kAutoupdateInstalling:
      throbber = true;
      message = l10n_util::GetNSStringWithFixup(IDS_UPGRADE_STARTED);
      enablePromoteButton = false;

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

    case kAutoupdatePromoting:
#if 1
      // TODO(mark): See the TODO in -adjustUpdateUIVisibility for an
      // explanation of why nothing can be done here at the moment.  When
      // KSRegistration handles promotion asynchronously, this dummy block can
      // be replaced with the #else block.  For now, just leave the messaging
      // alone.  http://b/2290009.
      updateMessage = false;
#else
      // The visibility may be changing.
      [self adjustUpdateUIVisibility];

      // This is not a terminal state, and kAutoupdatePromoted or
      // kAutoupdatePromoteFailed will follow.  Use the throbber and
      // "checking" message so that it looks like something's happening.
      throbber = true;
      message = l10n_util::GetNSStringWithFixup(IDS_UPGRADE_CHECK_STARTED);
#endif

      enablePromoteButton = false;

      break;

    case kAutoupdatePromoted:
      // The visibility may be changing.
      [self adjustUpdateUIVisibility];

      if (![updateBlock_ isHidden]) {
        // If promotion completes while the window is visible, go straight
        // into an update check.  Return immediately, this routine will be
        // re-entered shortly with kAutoupdateChecking.
        [self checkForUpdate];
        return;
      }

      // Nothing actually failed, but updates aren't possible.  The throbber
      // and message are hidden, but they'll be reset to these dummy values
      // just to get the throbber to stop spinning if it's running.
      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 base::IntToString16(status));

      break;

    case kAutoupdateRegisterFailed:
      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 base::IntToString16(status));
      enablePromoteButton = false;

      break;

    case kAutoupdateCheckFailed:
      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 base::IntToString16(status));

      break;

    case kAutoupdateInstallFailed:
      recentShownUserActionFailedStatus = YES;

      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 base::IntToString16(status));

      // Allow another chance.
      enableUpdateButton = true;

      break;

    case kAutoupdatePromoteFailed:
      recentShownUserActionFailedStatus = YES;

      imageID = IDR_UPDATE_FAIL;
      message = l10n_util::GetNSStringFWithFixup(IDS_UPGRADE_ERROR,
                                                 base::IntToString16(status));

      break;

    default:
      NOTREACHED();

      return;
  }

  if (updateMessage) {
    if (throbber) {
      [self setUpdateThrobberMessage:message];
    } else {
      DCHECK_NE(imageID, 0);
      [self setUpdateImage:imageID message:message];
    }
  }

  // Note that these buttons may be hidden depending on what
  // -adjustUpdateUIVisibility did.  Their enabled/disabled status doesn't
  // necessarily have anything to do with their visibility.
  [updateNowButton_ setEnabled:enableUpdateButton];
  [promoteButton_ setEnabled:enablePromoteButton];
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  // We always create a new window, so there's no need to try to re-use
  // an existing one just to pass in the NEW_WINDOW disposition.
  Browser* browser = Browser::Create(profile_);
  browser->OpenURL(GURL([link UTF8String]), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
  browser->window()->Show();
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
  // src/chrome/browser/ui/views/about_chrome_view.cc AboutChromeView::Init()

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
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kChromiumProjectURL));
  NSString* kChromiumProject = base::SysUTF8ToNSString(url.spec());
  // The OSS link should go to here
  NSString* kAcknowledgements =
      [NSString stringWithUTF8String:chrome::kAboutCreditsURL];

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
  NSString* kTOS = [NSString stringWithUTF8String:chrome::kAboutTermsURL];
  // Following Windows. There is one marker in the string for where the terms
  // link goes, but the text of the link comes from a second string resources.
  std::vector<size_t> url_offsets;
  NSString* about_terms = l10n_util::GetNSStringF(IDS_ABOUT_TERMS_OF_SERVICE,
                                                  string16(),
                                                  string16(),
                                                  &url_offsets);
  DCHECK_EQ(url_offsets.size(), 1U);
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
