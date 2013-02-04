// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/plugins/plugin_data_remover_helper.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/error_utils.h"

using content::BrowserThread;

namespace extension_browsing_data_api_constants {

// Parameter name keys.
const char kDataToRemoveKey[] = "dataToRemove";
const char kOptionsKey[] = "options";

// Type keys.
const char kAppCacheKey[] = "appcache";
const char kCacheKey[] = "cache";
const char kCookiesKey[] = "cookies";
const char kDownloadsKey[] = "downloads";
const char kFileSystemsKey[] = "fileSystems";
const char kFormDataKey[] = "formData";
const char kHistoryKey[] = "history";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kServerBoundCertsKey[] = "serverBoundCertificates";
const char kPasswordsKey[] = "passwords";
const char kPluginDataKey[] = "pluginData";
const char kWebSQLKey[] = "webSQL";

// Option keys.
const char kExtensionsKey[] = "extension";
const char kOriginTypesKey[] = "originTypes";
const char kProtectedWebKey[] = "protectedWeb";
const char kSinceKey[] = "since";
const char kUnprotectedWebKey[] = "unprotectedWeb";

// Errors!
const char kOneAtATimeError[] = "Only one 'browsingData' API call can run at "
                                "a time.";

}  // namespace extension_browsing_data_api_constants

namespace {
// Given a DictionaryValue |dict|, returns either the value stored as |key|, or
// false, if the given key doesn't exist in the dictionary.
bool RemoveType(base::DictionaryValue* dict, const std::string& key) {
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
  if (RemoveType(value, extension_browsing_data_api_constants::kAppCacheKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_APPCACHE;
  if (RemoveType(value, extension_browsing_data_api_constants::kCacheKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_CACHE;
  if (RemoveType(value, extension_browsing_data_api_constants::kCookiesKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_COOKIES;
  if (RemoveType(value, extension_browsing_data_api_constants::kDownloadsKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (RemoveType(value, extension_browsing_data_api_constants::kFileSystemsKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_FILE_SYSTEMS;
  if (RemoveType(value, extension_browsing_data_api_constants::kFormDataKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_FORM_DATA;
  if (RemoveType(value, extension_browsing_data_api_constants::kHistoryKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (RemoveType(value, extension_browsing_data_api_constants::kIndexedDBKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_INDEXEDDB;
  if (RemoveType(value,
                 extension_browsing_data_api_constants::kLocalStorageKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_LOCAL_STORAGE;
  if (RemoveType(value,
                 extension_browsing_data_api_constants::kServerBoundCertsKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS;
  if (RemoveType(value, extension_browsing_data_api_constants::kPasswordsKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (RemoveType(value, extension_browsing_data_api_constants::kPluginDataKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_PLUGIN_DATA;
  if (RemoveType(value, extension_browsing_data_api_constants::kWebSQLKey))
    GetRemovalMask |= BrowsingDataRemover::REMOVE_WEBSQL;

  return GetRemovalMask;
}

}  // namespace


bool BrowsingDataSettingsFunction::RunImpl() {
  PrefService* prefs = profile()->GetPrefs();

  scoped_ptr<DictionaryValue> options(new DictionaryValue);
  scoped_ptr<DictionaryValue> data_types(new DictionaryValue);
  scoped_ptr<DictionaryValue> origin_types(new DictionaryValue);

  // The "cookies" and "hosted apps" UI checkboxes both map to
  // REMOVE_SITE_DATA in browsing_data_remover.h, the former for the unprotected
  // web, the latter for  protected web data. There is no UI control for
  // extension data.
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kUnprotectedWebKey,
      prefs->GetBoolean(prefs::kDeleteCookies));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kProtectedWebKey,
      prefs->GetBoolean(prefs::kDeleteHostedAppsData));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kExtensionsKey, false);
  options->Set(extension_browsing_data_api_constants::kOriginTypesKey,
               origin_types.release());

  int period_pref = prefs->GetInteger(prefs::kDeleteTimePeriod);
  BrowsingDataRemover::TimePeriod period =
      static_cast<BrowsingDataRemover::TimePeriod>(period_pref);
  double since = 0;
  if (period != BrowsingDataRemover::EVERYTHING) {
    base::Time time = BrowsingDataRemover::CalculateBeginDeleteTime(period);
    since = time.ToJsTime();
  }
  options->SetDouble(extension_browsing_data_api_constants::kSinceKey, since);

  bool delete_site_data = prefs->GetBoolean(prefs::kDeleteCookies) ||
                          prefs->GetBoolean(prefs::kDeleteHostedAppsData);
  data_types->SetBoolean(extension_browsing_data_api_constants::kAppCacheKey,
                         delete_site_data);
  data_types->SetBoolean(extension_browsing_data_api_constants::kCookiesKey,
                         delete_site_data);
  data_types->SetBoolean(extension_browsing_data_api_constants::kFileSystemsKey,
                         delete_site_data);
  data_types->SetBoolean(extension_browsing_data_api_constants::kIndexedDBKey,
                         delete_site_data);
  data_types->SetBoolean(
      extension_browsing_data_api_constants::kLocalStorageKey,
      delete_site_data);
  data_types->SetBoolean(extension_browsing_data_api_constants::kWebSQLKey,
                         delete_site_data);
  data_types->SetBoolean(
      extension_browsing_data_api_constants::kServerBoundCertsKey,
      delete_site_data);
  data_types->SetBoolean(extension_browsing_data_api_constants::kPluginDataKey,
      delete_site_data && prefs->GetBoolean(prefs::kClearPluginLSODataEnabled));

  data_types->SetBoolean(extension_browsing_data_api_constants::kHistoryKey,
                         prefs->GetBoolean(prefs::kDeleteBrowsingHistory));
  data_types->SetBoolean(extension_browsing_data_api_constants::kDownloadsKey,
                         prefs->GetBoolean(prefs::kDeleteDownloadHistory));
  data_types->SetBoolean(extension_browsing_data_api_constants::kCacheKey,
                         prefs->GetBoolean(prefs::kDeleteCache));
  data_types->SetBoolean(extension_browsing_data_api_constants::kFormDataKey,
                         prefs->GetBoolean(prefs::kDeleteFormData));
  data_types->SetBoolean(extension_browsing_data_api_constants::kPasswordsKey,
                         prefs->GetBoolean(prefs::kDeletePasswords));

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->Set(extension_browsing_data_api_constants::kOptionsKey,
              options.release());
  result->Set(extension_browsing_data_api_constants::kDataToRemoveKey,
              data_types.release());
  SetResult(result.release());
  return true;
}

void BrowsingDataRemoveFunction::OnBrowsingDataRemoverDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  this->SendResponse(true);

  Release();  // Balanced in RunImpl.
}

bool BrowsingDataRemoveFunction::RunImpl() {
  // If we don't have a profile, something's pretty wrong.
  DCHECK(profile());

  // Grab the initial |options| parameter, and parse out the arguments.
  DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  DCHECK(options);

  origin_set_mask_ = ParseOriginSetMask(*options);

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch;
  if (!options->GetDouble(extension_browsing_data_api_constants::kSinceKey,
                          &ms_since_epoch))
    ms_since_epoch = 0;

  // base::Time takes a double that represents seconds since epoch. JavaScript
  // gives developers milliseconds, so do a quick conversion before populating
  // the object. Also, Time::FromDoubleT converts double time 0 to empty Time
  // object. So we need to do special handling here.
  remove_since_ = (ms_since_epoch == 0) ?
      base::Time::UnixEpoch() :
      base::Time::FromDoubleT(ms_since_epoch / 1000.0);

  removal_mask_ = GetRemovalMask();

  if (removal_mask_ & BrowsingDataRemover::REMOVE_PLUGIN_DATA) {
    // If we're being asked to remove plugin data, check whether it's actually
    // supported.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &BrowsingDataRemoveFunction::CheckRemovingPluginDataSupported,
            this,
            PluginPrefs::GetForProfile(profile())));
  } else {
    StartRemoving();
  }

  // Will finish asynchronously.
  return true;
}

void BrowsingDataRemoveFunction::CheckRemovingPluginDataSupported(
    scoped_refptr<PluginPrefs> plugin_prefs) {
  if (!PluginDataRemoverHelper::IsSupported(plugin_prefs))
    removal_mask_ &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemoveFunction::StartRemoving, this));
}

void BrowsingDataRemoveFunction::StartRemoving() {
  if (BrowsingDataRemover::is_removing()) {
    error_ = extension_browsing_data_api_constants::kOneAtATimeError;
    SendResponse(false);
    return;
  }

  // If we're good to go, add a ref (Balanced in OnBrowsingDataRemoverDone)
  AddRef();

  // Create a BrowsingDataRemover, set the current object as an observer (so
  // that we're notified after removal) and call remove() with the arguments
  // we've generated above. We can use a raw pointer here, as the browsing data
  // remover is responsible for deleting itself once data removal is complete.
  BrowsingDataRemover* remover = BrowsingDataRemover::CreateForRange(profile(),
      remove_since_, base::Time::Max());
  remover->AddObserver(this);
  remover->Remove(removal_mask_, origin_set_mask_);
}

int BrowsingDataRemoveFunction::ParseOriginSetMask(
    const base::DictionaryValue& options) {
  // Parse the |options| dictionary to generate the origin set mask. Default to
  // UNPROTECTED_WEB if the developer doesn't specify anything.
  int mask = BrowsingDataHelper::UNPROTECTED_WEB;

  const DictionaryValue* d = NULL;
  if (options.HasKey(extension_browsing_data_api_constants::kOriginTypesKey)) {
    EXTENSION_FUNCTION_VALIDATE(options.GetDictionary(
        extension_browsing_data_api_constants::kOriginTypesKey, &d));
    bool value;

    // The developer specified something! Reset to 0 and parse the dictionary.
    mask = 0;

    // Unprotected web.
    if (d->HasKey(extension_browsing_data_api_constants::kUnprotectedWebKey)) {
      EXTENSION_FUNCTION_VALIDATE(d->GetBoolean(
          extension_browsing_data_api_constants::kUnprotectedWebKey, &value));
      mask |= value ? BrowsingDataHelper::UNPROTECTED_WEB : 0;
    }

    // Protected web.
    if (d->HasKey(extension_browsing_data_api_constants::kProtectedWebKey)) {
      EXTENSION_FUNCTION_VALIDATE(d->GetBoolean(
          extension_browsing_data_api_constants::kProtectedWebKey, &value));
      mask |= value ? BrowsingDataHelper::PROTECTED_WEB : 0;
    }

    // Extensions.
    if (d->HasKey(extension_browsing_data_api_constants::kExtensionsKey)) {
      EXTENSION_FUNCTION_VALIDATE(d->GetBoolean(
          extension_browsing_data_api_constants::kExtensionsKey, &value));
      mask |= value ? BrowsingDataHelper::EXTENSION : 0;
    }
  }

  return mask;
}

int RemoveBrowsingDataFunction::GetRemovalMask() const {
  // Parse the |dataToRemove| argument to generate the removal mask.
  base::DictionaryValue* data_to_remove;
  if (args_->GetDictionary(1, &data_to_remove))
    return ParseRemovalMask(data_to_remove);
  else
    return 0;
}

int RemoveAppCacheFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_APPCACHE;
}

int RemoveCacheFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_CACHE;
}

int RemoveCookiesFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_COOKIES |
         BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS;
}

int RemoveDownloadsFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_DOWNLOADS;
}

int RemoveFileSystemsFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_FILE_SYSTEMS;
}

int RemoveFormDataFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_FORM_DATA;
}

int RemoveHistoryFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_HISTORY;
}

int RemoveIndexedDBFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_INDEXEDDB;
}

int RemoveLocalStorageFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_LOCAL_STORAGE;
}

int RemovePluginDataFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_PLUGIN_DATA;
}

int RemovePasswordsFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_PASSWORDS;
}

int RemoveWebSQLFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_WEBSQL;
}
