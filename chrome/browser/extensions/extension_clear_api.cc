// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Clear API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in
// chrome/common/extensions/api/extension_api.json.

#include "chrome/browser/extensions/extension_clear_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/extensions/extension_clear_api_constants.h"
#include "chrome/browser/plugin_data_remover_helper.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace keys = extension_clear_api_constants;

namespace {

// Converts the JavaScript API's string input ("last_week") into the
// appropriate BrowsingDataRemover::TimePeriod (in this case,
// BrowsingDataRemover::LAST_WEEK).
bool ParseTimePeriod(const std::string& parse,
                     BrowsingDataRemover::TimePeriod* period) {
  if (parse == keys::kHourEnum)
    *period = BrowsingDataRemover::LAST_HOUR;
  else if (parse == keys::kDayEnum)
    *period = BrowsingDataRemover::LAST_DAY;
  else if (parse == keys::kWeekEnum)
    *period = BrowsingDataRemover::LAST_WEEK;
  else if (parse == keys::kMonthEnum)
    *period = BrowsingDataRemover::FOUR_WEEKS;
  else if (parse == keys::kEverythingEnum)
    *period = BrowsingDataRemover::EVERYTHING;
  else
    return false;

  return true;
}

// Given a DictionaryValue |dict|, returns either the value stored as |key|, or
// false, if the given key doesn't exist in the dictionary.
bool DataRemovalRequested(base::DictionaryValue* dict, std::string key) {
  bool value = false;
  if (!dict->GetBoolean(key, &value))
    return false;
  else
    return value;
}

// Convert the JavaScript API's object input ({ cookies: true }) into the
// appropriate removal mask for the BrowsingDataRemover object.
int ParseRemovalMask(base::DictionaryValue* value) {
  int GetRemovalMask = 0;
  if (DataRemovalRequested(value, keys::kCacheKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_CACHE;
  if (DataRemovalRequested(value, keys::kDownloadsKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (DataRemovalRequested(value, keys::kFormDataKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (DataRemovalRequested(value, keys::kHistoryKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (DataRemovalRequested(value, keys::kPasswordsKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_PASSWORDS;

  // When we talk users about "cookies", we mean not just cookies, but pretty
  // much everything associated with an origin.
  if (DataRemovalRequested(value, keys::kCookiesKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_SITE_DATA;

  return GetRemovalMask;
}

}  // Namespace.

void BrowsingDataExtensionFunction::OnBrowsingDataRemoverDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  this->SendResponse(true);

  Release();  // Balanced in RunImpl.
}

bool BrowsingDataExtensionFunction::RunImpl() {
  if (BrowsingDataRemover::is_removing()) {
    error_ = keys::kOneAtATimeError;
    return false;
  }

  // Parse the |timeframe| argument to generate the TimePeriod.
  std::string timeframe;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &timeframe));
  EXTENSION_FUNCTION_VALIDATE(ParseTimePeriod(timeframe, &period_));

  removal_mask_ = GetRemovalMask();

  if (removal_mask_ & BrowsingDataRemover::REMOVE_LSO_DATA) {
    // If we're being asked to remove LSO data, check whether it's actually
    // supported.
    Profile* profile = GetCurrentBrowser()->profile();
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &BrowsingDataExtensionFunction::CheckRemovingLSODataSupported,
            this,
            make_scoped_refptr(PluginPrefs::GetForProfile(profile))));
  } else {
    StartRemoving();
  }

  // Will finish asynchronously.
  return true;
}

void BrowsingDataExtensionFunction::CheckRemovingLSODataSupported(
    scoped_refptr<PluginPrefs> plugin_prefs) {
  if (!PluginDataRemoverHelper::IsSupported(plugin_prefs))
    removal_mask_ &= ~BrowsingDataRemover::REMOVE_LSO_DATA;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataExtensionFunction::StartRemoving, this));
}

void BrowsingDataExtensionFunction::StartRemoving() {
  // If we're good to go, add a ref (Balanced in OnBrowsingDataRemoverDone)
  AddRef();

  // Create a BrowsingDataRemover, set the current object as an observer (so
  // that we're notified after removal) and call remove() with the arguments
  // we've generated above. We can use a raw pointer here, as the browsing data
  // remover is responsible for deleting itself once data removal is complete.
  BrowsingDataRemover* remover = new BrowsingDataRemover(
      GetCurrentBrowser()->profile(), period_, base::Time::Now());
  remover->AddObserver(this);
  remover->Remove(removal_mask_);
}

int ClearBrowsingDataFunction::GetRemovalMask() const {
  // Parse the |dataToRemove| argument to generate the removal mask.
  base::DictionaryValue* data_to_remove;
  if (args_->GetDictionary(1, &data_to_remove))
    return ParseRemovalMask(data_to_remove);
  else
    return 0;
}

int ClearCacheFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_CACHE;
}

int ClearCookiesFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_SITE_DATA;
}

int ClearDownloadsFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_DOWNLOADS;
}

int ClearFormDataFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_FORM_DATA;
}

int ClearHistoryFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_HISTORY;
}

int ClearPasswordsFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_CACHE;
}
