// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password_exporter.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordExporter () {
  // Module containing the reauthentication mechanism used for exporting
  // passwords.
  __weak id<ReauthenticationProtocol> _weakReauthenticationModule;
  // Instance of the view controller initiating the export. Used
  // for displaying alerts.
  __weak id<PasswordExporterDelegate> _weakDelegate;
}

@end

@implementation PasswordExporter

- (instancetype)initWithReauthenticationModule:
                    (id<ReauthenticationProtocol>)reauthenticationModule
                                      delegate:(id<PasswordExporterDelegate>)
                                                   delegate {
  DCHECK(delegate);
  DCHECK(reauthenticationModule);
  self = [super init];
  if (self) {
    _weakReauthenticationModule = reauthenticationModule;
    _weakDelegate = delegate;
  }
  return self;
}

- (void)startExportFlow {
  if ([_weakReauthenticationModule canAttemptReauth]) {
    // TODO(crbug.com/789122): Ask for password serialization.
    [self startReauthentication];
  } else {
    [_weakDelegate showSetPasscodeDialog];
  }
}

#pragma mark -  Private methods

- (void)startReauthentication {
  [_weakReauthenticationModule
      attemptReauthWithLocalizedReason:l10n_util::GetNSString(
                                           IDS_IOS_EXPORT_PASSWORDS)
                  canReusePreviousAuth:NO
                               handler:^(BOOL success){
                                   // TODO(crbug.com/789122): Store reauth
                                   // result and continue the export flow.
                               }];
}
@end
