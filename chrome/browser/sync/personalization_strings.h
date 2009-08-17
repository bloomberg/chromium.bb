// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains all string resources usec by the personalization module.

// TODO(munjal): This file should go away once the personalization module
// becomes public. At that point, we have to put all the strings below in the
// generated resources file.

#ifdef CHROME_PERSONALIZATION

// TODO(timsteele): Rename this file; 'personalization' is deprecated.
#ifndef CHROME_BROWSER_SYNC_PERSONALIZATION_STRINGS_H_
#define CHROME_BROWSER_SYNC_PERSONALIZATION_STRINGS_H_

// Options dialog strings.
static const wchar_t kSyncGroupName[] = L"Bookmark Sync:";
static const wchar_t kSyncNotSetupInfo[] =
    L"You are not set up to sync your bookmarks with your other computers.";
static const wchar_t kStartSyncButtonLabel[] = L"Synchronize my bookmarks...";
static const wchar_t kSyncAccountLabel[] = L"Synced to ";
static const wchar_t kLastSyncedLabel[] = L"Last synced: ";
static const wchar_t kSyncCredentialsNeededLabel[] =
    L"Account login details are not yet entered.";
static const wchar_t kSyncAuthenticatingLabel[] = L"Authenticating...";
static const wchar_t kSyncInvalidCredentialsError[] =
    L"Invalid user name or password.";
static const wchar_t kSyncOtherLoginErrorLabel[] =
    L"Error signing in.";
static const wchar_t kSyncExpiredCredentialsError[] =
    L"Login details are out of date.";
static const wchar_t kSyncServerNotReachableError[] =
    L"Sync server is not reachable. Retrying...";
static const wchar_t kSyncReLoginLinkLabel[] = L"Login again";
static const wchar_t kStopSyncButtonLabel[] = L"Stop syncing this account";

// Sync status messages.
static const wchar_t kLastSyncedTimeNever[] = L"Never.";
static const wchar_t kLastSyncedTimeWithinLastMinute[] = L"Just now.";

// Various strings for the new tab page personalization.
static const char kSyncSectionTitle[] = "Bookmark Sync";
static const char kSyncErrorSectionTitle[] = "Bookmark Sync Error!";
static const char kSyncPromotionMsg[] =
    "You can sync your bookmarks across computers using your Google account.";
static const wchar_t kSyncServerUnavailableMsg[] =
    L"Google Chrome could not sync your bookmarks because it could not connect "
    L"to the sync server. Retrying...";
static const char kStartNowLinkText[] = "Start now";
static const char kSettingUpText[] = "Setup in progress...";

// TODO(idana): when we get rid of this file and move all the strings into
// generated_resources.grd, we should probably move these into a pref of some
// sort to allow third party sync providers to control which URL to use for the
// sync web UI. We should also make it possible for the sync provider to hide
// the link altogether if they don't provide web UI for their users.
static const char kSyncViewOnlineLinkUrl[] = "http://docs.google.com";
static const char kSyncViewOnlineLinkLabel[] = "View in Google Docs";

// Sync menu item strings.
static const wchar_t kMenuLabelStartSync[] = L"Sync my bookmarks...";

// Login dialog strings.
static const wchar_t kLoginDialogTitle[] = L"Sync my bookmarks";

// Bookmark bar strings.
static const wchar_t kBookmarkBarSyncErrorButtonText[] = L"Sync Error";
static const wchar_t kBookmarkBarErrorTooltip[] =
    L"Your login details are out of date. Click to re-enter your password.";

#endif  // CHROME_BROWSER_SYNC_PERSONALIZATION_STRINGS_H_

#endif  // CHROME_PERSONALIZATION
