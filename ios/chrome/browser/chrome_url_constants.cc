// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_url_constants.h"

#include <stddef.h>

#include "base/macros.h"

const char kChromeUIScheme[] = "chrome";
const char kDummyExtensionScheme[] = ":no-extension-scheme:";

const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUINTPTilesInternalsURL[] = "chrome://ntp-tiles-internals/";
const char kChromeUIOfflineURL[] = "chrome://offline/";
const char kChromeUIPhysicalWebURL[] = "chrome://physical-web/";
const char kChromeUIPopularSitesInternalsURL[] =
    "chrome://popular-sites-internals/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIVersionURL[] = "chrome://version/";

const char kChromeUIAppleFlagsHost[] = "ui-alternatives";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIExternalFileHost[] = "external-file";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIHistoryFrameHost[] = "history-frame";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUIOfflineHost[] = "offline";
const char kChromeUIOmahaHost[] = "omaha";
const char kChromeUIPhysicalWebHost[] = "physical-web";
const char kChromeUIPopularSitesInternalsHost[] = "popular-sites-internals";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUITermsHost[] = "terms";
const char kChromeUIVersionHost[] = "version";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
// 'histograms' is chrome WebUI on iOS, content WebUI on other platforms.
const char* const kChromeHostURLs[] = {
    kChromeUIBookmarksHost,       kChromeUIChromeURLsHost,
    kChromeUICreditsHost,         kChromeUIFlagsHost,
    kChromeUIHistogramHost,       kChromeUINetExportHost,
    kChromeUINewTabHost,          kChromeUINTPTilesInternalsHost,
    kChromeUISignInInternalsHost, kChromeUISyncInternalsHost,
    kChromeUIPhysicalWebHost,     kChromeUIPopularSitesInternalsHost,
    kChromeUITermsHost,           kChromeUIVersionHost,
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kPageInfoHelpCenterURL[] =
    "https://support.google.com/chrome/?p=ui_security_indicator";

const char kCrashReasonURL[] = "https://support.google.com/chrome/?p=e_awsnap";

const char kPrivacyLearnMoreURL[] =
    "https://support.google.com/chrome/answer/114836?p=settings_privacy";

const char kDoNotTrackLearnMoreURL[] =
    "https://support.google.com/chrome/answer/2942429?p=mobile_do_not_track";

const char kPhysicalWebLearnMoreURL[] =
    "https://support.google.com/chrome/answer/6239299?p=physical_web";

const char kSyncEncryptionHelpURL[] =
    "https://support.google.com/chrome/answer/1181035?p=settings_encryption";

const char kClearBrowsingDataLearnMoreURL[] =
    "https://support.google.com/chrome/answer/2392709";

const char kClearBrowsingDataMyActivityUrlInFooterURL[] =
    "https://history.google.com/history/?utm_source=chrome_cbd";

const char kClearBrowsingDataMyActivityUrlInDialogURL[] =
    "https://history.google.com/history/?utm_source=chrome_n";

const char kHistoryMyActivityURL[] =
    "https://history.google.com/history/?utm_source=chrome_h";

const char kGoogleHistoryURL[] = "https://history.google.com";

const char kGoogleMyAccountURL[] =
    "https://myaccount.google.com/privacy#activitycontrols";
