// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/sync/profile_sync_service.h"

class Browser;
class Profile;
class ListValue;
class DictionaryValue;

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.
namespace sync_ui_util {

enum MessageType {
  PRE_SYNCED,  // User has not set up sync.
  SYNCED,      // We are synced and authenticated to a gmail account.
  SYNC_ERROR,  // A sync error (such as invalid credentials) has occurred.
  SYNC_PROMO,  // A situation has occurred which should be brought to the user's
               // attention, but not as an error.
};

// TODO(akalin): audit the use of ProfileSyncService* service below,
// and use const ProfileSyncService& service where possible.

string16 GetLoginMessageForEncryption();

// Create status and link labels for the current status labels and link text
// by querying |service|.
MessageType GetStatusLabels(ProfileSyncService* service,
                            string16* status_label,
                            string16* link_label);

// Same as above but for use specifically on the New Tab Page.
MessageType GetStatusLabelsForNewTabPage(ProfileSyncService* service,
                                         string16* status_label,
                                         string16* link_label);

MessageType GetStatus(ProfileSyncService* service);

// Determines whether or not the sync error button should be visible.
bool ShouldShowSyncErrorButton(ProfileSyncService* service);

// Returns a string with the synchronization status.
string16 GetSyncMenuLabel(ProfileSyncService* service);

// Open the appropriate sync dialog for the given profile (which can be
// incognito). |browser| is the browser window that should be used if the UI
// is in-window (i.e., WebUI). |code| should be one of the START_FROM_* codes.
void OpenSyncMyBookmarksDialog(Profile* profile,
                               Browser* browser,
                               ProfileSyncService::SyncEventCodes code);

void AddBoolSyncDetail(ListValue* details,
                       const std::string& stat_name,
                       bool stat_value);

// |service| can be NULL.
void ConstructAboutInformation(ProfileSyncService* service,
                               DictionaryValue* strings);

void AddIntSyncDetail(ListValue* details,
                      const std::string& stat_name,
                      int64 stat_value);
}  // namespace sync_ui_util
#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
