// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_EXPORTER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_EXPORTER_H_

#import <Foundation/Foundation.h>

@protocol ReauthenticationProtocol;

@protocol PasswordExporterDelegate<NSObject>

// Displays a dialog informing the user that they must set up a passcode
// in order to export passwords.
- (void)showSetPasscodeDialog;

@end

/** Class handling all the operations necessary to export passwords.*/
@interface PasswordExporter : NSObject

// The designated initializer. |reauthenticationModule| and |delegate| must
// not be nil.
- (instancetype)initWithReauthenticationModule:
                    (id<ReauthenticationProtocol>)reuthenticationModule
                                      delegate:
                                          (id<PasswordExporterDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Method to be called in order to start the export flow. This initiates
// the re-authentication procedure and asks for password serialization.
- (void)startExportFlow;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_EXPORTER_H_
