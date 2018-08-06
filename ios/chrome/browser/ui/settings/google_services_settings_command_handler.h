// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_

// List of Google Services Settings commands.
typedef NS_ENUM(NSInteger, GoogleServicesSettingsCommandID) {
  // Does nothing.
  GoogleServicesSettingsCommandIDNoOp,
  // Enabble/disable all the Google services.
  GoogleServicesSettingsCommandIDToggleSyncEverything,
  // Enable/disabble bookmark sync.
  GoogleServicesSettingsCommandIDToggleDataTypeSync,
  // Enable/disabble autocomplete searches service.
  GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService,
  // Enable/disabble preload pages service.
  GoogleServicesSettingsCommandIDTogglePreloadPagesService,
  // Enable/disabble improve chrome service.
  GoogleServicesSettingsCommandIDToggleImproveChromeService,
  // Enable/disabble better search and browsing service.
  GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService,
  // Opens the Google activity controls dialog.
  GoogleServicesSettingsCommandIDOpenGoogleActivityPage,
  // Opens the encryption dialog.
  GoogleServicesSettingsCommandIDOpenEncryptionDialog,
  // Opens manage synced data page.
  GoogleServicesSettingsCommandIDOpenManageSyncedDataPage,
};

// Protocol to handle Google services settings commands.
@protocol GoogleServicesSettingsCommandHandler<NSObject>

// Called when GoogleServicesSettingsCommandIDToggleSyncEverything is triggered.
- (void)toggleSyncEverythingWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleDataTypeSync is triggered.
- (void)toggleSyncDataSync:(NSInteger)dataType WithValue:(BOOL)on;
- (void)toggleAutocompleteSearchesServiceWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDTogglePreloadPagesService is
// triggered.
- (void)togglePreloadPagesServiceWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleImproveChromeService is
// triggered.
- (void)toggleImproveChromeServiceWithValue:(BOOL)on;
// Called when
// GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService is
// triggered.
- (void)toggleBetterSearchAndBrowsingServiceWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDOpenGoogleActivityPage is
// triggered.
- (void)openGoogleActivityPage;
// Called when GoogleServicesSettingsCommandIDOpenEncryptionDialog is
// triggered.
- (void)openEncryptionDialog;
// Called when GoogleServicesSettingsCommandIDOpenManageSyncedDataPage is
// triggered.
- (void)openManageSyncedDataPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
