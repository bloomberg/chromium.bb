// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/external_protocol_dialog.h"

#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/text_elider.h"

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id,
    ui::PageTransition page_transition, bool has_user_gesture) {
  [[ExternalProtocolDialogController alloc] initWithGURL:&url
                                     renderProcessHostId:render_process_host_id
                                               routingId:routing_id];
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialogController

@interface ExternalProtocolDialogController(Private)
- (void)alertEnded:(NSAlert *)alert
        returnCode:(int)returnCode
       contextInfo:(void*)contextInfo;
@end

@implementation ExternalProtocolDialogController
- (id)initWithGURL:(const GURL*)url
    renderProcessHostId:(int)renderProcessHostId
    routingId:(int)routingId {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  if (!(self = [super init]))
    return nil;

  url_ = *url;
  render_process_host_id_ = renderProcessHostId;
  routing_id_ = routingId;
  creation_time_ = base::Time::Now();

  base::string16 appName =
      shell_integration::GetApplicationNameForProtocol(url_);
  if (appName.length() == 0) {
    // No registered apps for this protocol; give up and go home.
    [self autorelease];
    return nil;
  }

  alert_ = [[NSAlert alloc] init];

  [alert_ setMessageText:
      l10n_util::GetNSStringFWithFixup(IDS_EXTERNAL_PROTOCOL_TITLE, appName)];

  NSButton* allowButton = [alert_
      addButtonWithTitle:l10n_util::GetNSStringFWithFixup(
                   IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT, appName)];
  [allowButton setKeyEquivalent:@""];  // disallow as default
  [alert_ addButtonWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)];

  [alert_ setShowsSuppressionButton:YES];
  [[alert_ suppressionButton]
      setTitle:l10n_util::GetNSStringFWithFixup(
                   IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT, appName)];

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
  if ([[alert_ suppressionButton] state] == NSOnState) {
    ExternalProtocolHandler::SetBlockState(url_.scheme(), blockState);
    ExternalProtocolHandler::RecordMetrics(true);
  } else {
    ExternalProtocolHandler::RecordMetrics(false);
  }

  if (blockState == ExternalProtocolHandler::DONT_BLOCK) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                             base::Time::Now() - creation_time_);

    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
        url_, render_process_host_id_, routing_id_);
  }

  [self autorelease];
}

@end
