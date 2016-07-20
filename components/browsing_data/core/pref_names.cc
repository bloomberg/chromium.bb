// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/pref_names.h"

namespace browsing_data {

namespace prefs {

// Clear browsing data deletion time period.
const char kDeleteTimePeriod[] = "browser.clear_data.time_period";

// Clear Browsing Data dialog datatype preferences.
const char kDeleteBrowsingHistory[] = "browser.clear_data.browsing_history";
const char kDeleteDownloadHistory[] = "browser.clear_data.download_history";
const char kDeleteCache[] = "browser.clear_data.cache";
const char kDeleteCookies[] = "browser.clear_data.cookies";
const char kDeletePasswords[] = "browser.clear_data.passwords";
const char kDeleteFormData[] = "browser.clear_data.form_data";
const char kDeleteHostedAppsData[] = "browser.clear_data.hosted_apps_data";
const char kDeleteMediaLicenses[] = "browser.clear_data.media_licenses";

}  // namespace prefs

}  // namespace browsing_data
