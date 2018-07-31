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
  GoogleServicesSettingsCommandIDToggleBookmarkSync,
  // Enable/disabble history sync.
  GoogleServicesSettingsCommandIDToggleHistorySync,
  // Enable/disabble passwords sync.
  GoogleServicesSettingsCommandIDTogglePasswordsSync,
  // Enable/disabble open tabs sync.
  GoogleServicesSettingsCommandIDToggleOpenTabsSync,
  // Enable/disabble autofill sync.
  GoogleServicesSettingsCommandIDToggleAutofillSync,
  // Enable/disabble settings sync.
  GoogleServicesSettingsCommandIDToggleSettingsSync,
  // Enable/disabble reading list sync.
  GoogleServicesSettingsCommandIDToggleReadingListSync,
  // Enable/disable activity and interactions service.
  GoogleServicesSettingsCommandIDToggleActivityAndInteractionsService,
  // Enable/disabble autocomplete searches service.
  GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService,
  // Enable/disabble preload pages service.
  GoogleServicesSettingsCommandIDTogglePreloadPagesService,
  // Enable/disabble improve chrome service.
  GoogleServicesSettingsCommandIDToggleImproveChromeService,
  // Enable/disabble better search and browsing service.
  GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService,
};

// Protocol to handle Google services settings commands.
@protocol GoogleServicesSettingsCommandHandler<NSObject>

// Called when GoogleServicesSettingsCommandIDToggleSyncEverything is triggered.
- (void)toggleSyncEverythingWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleBookmarkSync is triggered.
- (void)toggleBookmarksSyncWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleHistorySync is triggered.
- (void)toggleHistorySyncWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDTogglePasswordsSync is triggered.
- (void)togglePasswordsSyncWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleOpenTabsSync is triggered.
- (void)toggleOpenTabSyncWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleAutofillSync is triggered.
- (void)toggleAutofillWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleSettingsSync is triggered.
- (void)toggleSettingsWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleReadingListSync is
// triggered.
- (void)toggleReadingListWithValue:(BOOL)on;
// Called when
// GoogleServicesSettingsCommandIDToggleActivityAndInteractionsService is
// triggered.
- (void)toggleActivityAndInteractionsServiceWithValue:(BOOL)on;
// Called when GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService
// is triggered.
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

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
