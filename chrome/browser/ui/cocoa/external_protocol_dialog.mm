// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/external_protocol_dialog.h"

#include "app/l10n_util_mac.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/external_protocol_handler.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/text/text_elider.h"

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  [[ExternalProtocolDialogController alloc] initWithGURL:&url];
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialogController

@interface ExternalProtocolDialogController(Private)
- (void)alertEnded:(NSAlert *)alert
        returnCode:(int)returnCode
       contextInfo:(void*)contextInfo;
- (string16)appNameForProtocol;
@end

@implementation ExternalProtocolDialogController
- (id)initWithGURL:(const GURL*)url {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

  url_ = *url;
  creation_time_ = base::Time::Now();

  string16 appName = [self appNameForProtocol];
  if (appName.length() == 0) {
    // No registered apps for this protocol; give up and go home.
    [self autorelease];
    return nil;
  }

  alert_ = [[NSAlert alloc] init];

  [alert_ setMessageText:
      l10n_util::GetNSStringWithFixup(IDS_EXTERNAL_PROTOCOL_TITLE)];

  NSButton* allowButton = [alert_ addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT)];
  [allowButton setKeyEquivalent:@""];  // disallow as default
  [alert_ addButtonWithTitle:
      l10n_util::GetNSStringWithFixup(
        IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT)];

  const int kMaxUrlWithoutSchemeSize = 256;
  std::wstring elided_url_without_scheme;
  ui::ElideString(ASCIIToWide(url_.possibly_invalid_spec()),
                  kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);

  NSString* urlString = l10n_util::GetNSStringFWithFixup(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToUTF16(url_.scheme() + ":"),
      WideToUTF16(elided_url_without_scheme));
  NSString* appString = l10n_util::GetNSStringFWithFixup(
      IDS_EXTERNAL_PROTOCOL_APPLICATION_TO_LAUNCH,
      appName);
  NSString* warningString =
      l10n_util::GetNSStringWithFixup(IDS_EXTERNAL_PROTOCOL_WARNING);
  NSString* informativeText =
      [NSString stringWithFormat:@"%@\n\n%@\n\n%@",
                                 urlString,
                                 appString,
                                 warningString];

  [alert_ setInformativeText:informativeText];

  [alert_ setShowsSuppressionButton:YES];
  [[alert_ suppressionButton] setTitle:
      l10n_util::GetNSStringWithFixup(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT)];

  [alert_ beginSheetModalForWindow:nil  // nil here makes it app-modal
                     modalDelegate:self
                    didEndSelector:@selector(alertEnded:returnCode:contextInfo:)
                       contextInfo:nil];

  return self;
}

- (void)dealloc {
  [alert_ release];

  [super dealloc];
}

- (void)alertEnded:(NSAlert *)alert
        returnCode:(int)returnCode
       contextInfo:(void*)contextInfo {
  ExternalProtocolHandler::BlockState blockState =
      ExternalProtocolHandler::UNKNOWN;
  switch (returnCode) {
    case NSAlertFirstButtonReturn:
      blockState = ExternalProtocolHandler::DONT_BLOCK;
      break;
    case NSAlertSecondButtonReturn:
      blockState = ExternalProtocolHandler::BLOCK;
      break;
    default:
      NOTREACHED();
  }

  // Set the "don't warn me again" info.
  if ([[alert_ suppressionButton] state] == NSOnState)
    ExternalProtocolHandler::SetBlockState(url_.scheme(), blockState);

  if (blockState == ExternalProtocolHandler::DONT_BLOCK) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                             base::Time::Now() - creation_time_);

    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url_);
  }

  [self autorelease];
}

- (string16)appNameForProtocol {
  NSURL* url = [NSURL URLWithString:
      base::SysUTF8ToNSString(url_.possibly_invalid_spec())];
  CFURLRef openingApp = NULL;
  OSStatus status = LSGetApplicationForURL((CFURLRef)url,
                                           kLSRolesAll,
                                           NULL,
                                           &openingApp);
  if (status != noErr) {
    // likely kLSApplicationNotFoundErr
    return string16();
  }
  NSString* appPath = [(NSURL*)openingApp path];
  CFRelease(openingApp);  // NOT A BUG; LSGetApplicationForURL retains for us
  NSString* appDisplayName =
      [[NSFileManager defaultManager] displayNameAtPath:appPath];

  return base::SysNSStringToUTF16(appDisplayName);
}

@end
