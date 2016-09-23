// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/ios_browsing_data_counter_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/cache_counter.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/web_data_service_factory.h"

// static
std::unique_ptr<browsing_data::BrowsingDataCounter>
IOSBrowsingDataCounterFactory::GetForBrowserStateAndPref(
    ios::ChromeBrowserState* browser_state,
    base::StringPiece pref_name) {
  if (!experimental_flags::IsNewClearBrowsingDataUIEnabled())
    return nullptr;

  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    return base::MakeUnique<browsing_data::HistoryCounter>(
        ios::HistoryServiceFactory::GetForBrowserStateIfExists(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS),
        base::Bind(&ios::WebHistoryServiceFactory::GetForBrowserState,
                   base::Unretained(browser_state)),
        IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state));
  }

  if (pref_name == browsing_data::prefs::kDeleteCache)
    return base::MakeUnique<CacheCounter>(browser_state);

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    return base::MakeUnique<browsing_data::PasswordsCounter>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS));
  }

  if (pref_name == browsing_data::prefs::kDeleteFormData) {
    return base::MakeUnique<browsing_data::AutofillCounter>(
        ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS));
  }

  return nullptr;
}
