// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/file_version_info.h"
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

NSString* const kUserClosedAboutNotification =
  @"kUserClosedAboutNotification";

@interface AboutWindowController (Private)
- (KeystoneGlue*)defaultKeystoneGlue;
- (void)startProgressMessageID:(uint32_t)messageID;
- (void)startProgressMessage:(NSString*)message;
- (void)stopProgressMessage:(NSString*)message imageID:(uint32_t)imageID;
@end

namespace {

// Keystone doesn't give us error numbers on some results, so we just make
// our own for reporting in the UI.
const int kUpdateInstallFailed = 128;
const int kUpdateInstallFailedToStart = 129;

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

NSAttributedString* BuildAboutWindowLegalTextBlock() {
  // Windows builds this up in a very complex way, we're just trying to model
  // it the best we can to get all the information in (they actually do it
  // but created Labels and Links that they carefully place to make it appear
  // to be a paragraph of text).
  // src/chrome/browser/views/about_chrome_view.cc AboutChromeView::Init()

  NSMutableAttributedString* legal_block =
      [[[NSMutableAttributedString alloc] init] autorelease];
  [legal_block beginEditing];

  NSString* copyright = l10n_util::GetNSString(IDS_ABOUT_VERSION_COPYRIGHT);
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

  NSString* license = l10n_util::GetNSString(IDS_ABOUT_VERSION_LICENSE);

  NSRange begin_chr = [license rangeOfString:kBeginLinkChr];
  NSRange begin_oss = [license rangeOfString:kBeginLinkOss];
  NSRange end_chr = [license rangeOfString:kEndLinkChr];
  NSRange end_oss = [license rangeOfString:kEndLinkOss];
  DCHECK(begin_chr.location != NSNotFound);
  DCHECK(begin_oss.location != NSNotFound);
  DCHECK(end_chr.location != NSNotFound);
  DCHECK(end_oss.location != NSNotFound);

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
  NSString* terms_link_text = l10n_util::GetNSString(IDS_TERMS_OF_SERVICE);

  AttributedStringAppendString(legal_block, @"\n\n");
  sub_str = [about_terms substringToIndex:url_offsets[0]];
  AttributedStringAppendString(legal_block, sub_str);
  AttributedStringAppendHyperlink(legal_block, terms_link_text, kTOS);
  sub_str = [about_terms substringFromIndex:url_offsets[0]];
  AttributedStringAppendString(legal_block, sub_str);
#endif  // defined(GOOGLE_CHROME_BUILD)

  // We need to explicitly select Lucida Grande because once we click on
  // the NSTextView, it changes to Helvetica 12 otherwise.
  NSRange string_range = NSMakeRange(0, [legal_block length]);
  [legal_block addAttribute:NSFontAttributeName
                      value:[NSFont labelFontOfSize:11]
                      range:string_range];

  [legal_block endEditing];
  return legal_block;
}

@implementation AboutWindowController

- (id)initWithProfile:(Profile*)profile {
  NSString* nibpath = [mac_util::MainAppBundle() pathForResource:@"About"
                                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self) {
    profile_ = profile;
  }
  return self;
}

- (void)awakeFromNib {
  // Set our current version.
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  std::wstring version(version_info->product_version());
#if !defined(GOOGLE_CHROME_BUILD)
  // Yes, Windows does this raw since it is only in Chromium builds
  // src/chrome/browser/views/about_chrome_view.cc AboutChromeView::Init()
  version += L" (";
  version += version_info->last_change();
  version += L")";
#endif
  NSString* nsversion = base::SysWideToNSString(version);
  [version_ setStringValue:nsversion];

  // Put the two images into the ui
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* backgroundImage = rb.GetNSImageNamed(IDR_ABOUT_BACKGROUND_COLOR);
  DCHECK(backgroundImage);
  [backgroundView_ setTileImage:backgroundImage];
  NSImage* logoImage = rb.GetNSImageNamed(IDR_ABOUT_BACKGROUND);
  DCHECK(logoImage);
  [logoView_ setImage:logoImage];

  [[legalText_ textStorage]
    setAttributedString:BuildAboutWindowLegalTextBlock()];

  // Resize our text view now so that the |updateShift| below is set
  // correctly. The about box has its controls manually positioned, so we need
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

  KeystoneGlue* keystone = [self defaultKeystoneGlue];
  CGFloat updateShift = 0.0;
  if (keystone) {
    // Initiate an update check.
    if ([keystone checkForUpdate:self]) {
      [self startProgressMessageID:IDS_UPGRADE_CHECK_STARTED];
    }
  } else {
    // Hide all the update UI
    [updateBlock_ setHidden:YES];
    // Figure out the amount we're removing by taking about the update block
    // (and it's spacing).
    updateShift = NSMinY([legalBlock_ frame]) - NSMinY([updateBlock_ frame]);
  }

  // Adjust the sizes/locations.
  NSRect rect = [legalBlock_ frame];
  rect.origin.y -= updateShift;
  [legalBlock_ setFrame:rect];

  rect = [backgroundView_ frame];
  rect.origin.y = rect.origin.y - updateShift + legalShift;
  [backgroundView_ setFrame:rect];

  NSSize windowDelta = NSMakeSize(0, (legalShift - updateShift));
  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:[self window]
                                        delta:windowDelta];
}

- (KeystoneGlue*)defaultKeystoneGlue {
  return [KeystoneGlue defaultKeystoneGlue];
}

- (void)startProgressMessageID:(uint32_t)messageID {
  NSString* message = l10n_util::GetNSStringWithFixup(messageID);
  [self startProgressMessage:message];
}

- (void)startProgressMessage:(NSString*)message {
  [updateStatusIndicator_ setHidden:YES];
  [spinner_ setHidden:NO];
  [spinner_ startAnimation:self];

  [updateText_ setStringValue:message];
}

- (void)stopProgressMessage:(NSString*)message imageID:(uint32_t)imageID {
  [spinner_ stopAnimation:self];
  [spinner_ setHidden:YES];
  if (imageID) {
    [updateStatusIndicator_ setHidden:NO];
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    NSImage* statusImage = rb.GetNSImageNamed(imageID);
    DCHECK(statusImage);
    [updateStatusIndicator_ setImage:statusImage];
  }

  [updateText_ setStringValue:message];
}

// Callback from KeystoneGlue; implementation of KeystoneGlueCallbacks protocol.
// Warning: latest version may be nil if not set in server config.
- (void)upToDateCheckCompleted:(BOOL)updatesAvailable
                 latestVersion:(NSString*)latestVersion {
  uint32_t imageID;
  NSString* message;
  if (updatesAvailable) {
    newVersionAvailable_.reset([latestVersion copy]);

    // Window UI doesn't put the version number in the string.
    imageID = IDR_UPDATE_AVAILABLE;
    message =
        l10n_util::GetNSStringF(IDS_UPGRADE_AVAILABLE,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    [updateNowButton_ setEnabled:YES];
  } else {
    // NOTE: This is can be a lie, Keystone does not provide us with an error if
    // it was not able to reach the server.  So we can't completely map to the
    // Windows UI.

    // Keystone does not provide the version number when we are up to date so to
    // maintain the UI, we just go fetch our version and call it good.
    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfoForCurrentModule());
    std::wstring version(version_info->product_version());

    // TODO: We really should check to see if what is on disk is newer then what
    // is running and report it as such.  (Windows has some messages that can
    // help with this.)  http://crbug.com/13165

    imageID = IDR_UPDATE_UPTODATE;
    message =
        l10n_util::GetNSStringF(IDS_UPGRADE_ALREADY_UP_TO_DATE,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                                WideToUTF16(version));
  }
  [self stopProgressMessage:message imageID:imageID];
}

- (void)windowWillClose:(NSNotification*)notification {
  // If an update has ever been triggered, we force reuse of the same About Box.
  // This gives us 2 things:
  // 1. If an update is ongoing and the window was closed we would have
  // no way of getting status.
  // 2. If we have a "Please restart" message we want it to stay there.
  if (updateTriggered_)
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kUserClosedAboutNotification
                    object:self];
}

// Callback from KeystoneGlue; implementation of KeystoneGlueCallbacks protocol.
- (void)updateCompleted:(BOOL)successful installs:(int)installs {
  uint32_t imageID;
  NSString* message;
  if (successful && installs) {
    imageID = IDR_UPDATE_UPTODATE;
    if ([newVersionAvailable_.get() length]) {
      message =
          l10n_util::GetNSStringF(IDS_UPGRADE_SUCCESSFUL,
                                  l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                                  base::SysNSStringToUTF16(
                                                  newVersionAvailable_.get()));
    } else {
      message =
          l10n_util::GetNSStringF(IDS_UPGRADE_SUCCESSFUL_NOVERSION,
                                  l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    }

    // Tell the user to restart their browser.
    restart_browser::RequestRestart(nil);

  } else {
    imageID = IDR_UPDATE_FAIL;
    message =
        l10n_util::GetNSStringF(IDS_UPGRADE_ERROR,
                                IntToString16(kUpdateInstallFailed));

    // Allow a second chance.
    [updateNowButton_ setEnabled:YES];
  }

  [self stopProgressMessage:message imageID:imageID];
}

- (IBAction)updateNow:(id)sender {
  updateTriggered_ = YES;

  // Don't let someone click "Update Now" twice!
  [updateNowButton_ setEnabled:NO];
  if ([[self defaultKeystoneGlue] startUpdate:self]) {
    // Clear any previous error message from the throbber area.
    [self startProgressMessageID:IDS_UPGRADE_STARTED];
  } else {
    NSString* message =
        l10n_util::GetNSStringF(IDS_UPGRADE_ERROR,
                                IntToString16(kUpdateInstallFailedToStart));
    [self stopProgressMessage:message imageID:IDR_UPDATE_FAIL];
  }
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  // We always create a new window, so there's no need to try to re-use
  // an existing one just to pass in the NEW_WINDOW disposition.
  Browser* browser = Browser::Create(profile_);
  if (browser)
    browser->OpenURL(GURL([link UTF8String]), GURL(), NEW_WINDOW,
                     PageTransition::LINK);
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

@end

