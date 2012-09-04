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
#include "chrome/browser/plugin_data_remover_helper.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extension_browsing_data_api_constants {

// Type Keys.
const char kAppCacheKey[] = "appcache";
const char kCacheKey[] = "cache";
const char kCookiesKey[] = "cookies";
const char kDownloadsKey[] = "downloads";
const char kFileSystemsKey[] = "fileSystems";
const char kFormDataKey[] = "formData";
const char kHistoryKey[] = "history";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kServerBoundCertsKey[] = "serverBoundCerts";
const char kPasswordsKey[] = "passwords";
const char kPluginDataKey[] = "pluginData";
const char kWebSQLKey[] = "webSQL";

// Option Keys.
const char kExtensionsKey[] = "extension";
const char kOriginTypesKey[] = "originType";
const char kProtectedWebKey[] = "protectedWeb";
const char kSinceKey[] = "since";
const char kUnprotectedWebKey[] = "unprotectedWeb";

// Errors!
const char kOneAtATimeError[] = "Only one 'browsingData' API call can run at "
                                "a time.";

}  // namespace extension_browsing_data_api_constants

namespace {
// Converts the JavaScript API's numeric input (miliseconds since epoch) into an
// appropriate base::Time that we can pass into the BrowsingDataRemove.
bool ParseTimeFromValue(const double& ms_since_epoch, base::Time* time) {
  return true;
}

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

void BrowsingDataExtensionFunction::OnBrowsingDataRemoverDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  this->SendResponse(true);

  Release();  // Balanced in RunImpl.
}

bool BrowsingDataExtensionFunction::RunImpl() {
  // If we don't have a profile, something's pretty wrong.
  DCHECK(profile());

  if (BrowsingDataRemover::is_removing()) {
    error_ = extension_browsing_data_api_constants::kOneAtATimeError;
    return false;
  }

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
            &BrowsingDataExtensionFunction::CheckRemovingPluginDataSupported,
            this,
            PluginPrefs::GetForProfile(profile())));
  } else {
    StartRemoving();
  }

  // Will finish asynchronously.
  return true;
}

void BrowsingDataExtensionFunction::CheckRemovingPluginDataSupported(
    scoped_refptr<PluginPrefs> plugin_prefs) {
  if (!PluginDataRemoverHelper::IsSupported(plugin_prefs))
    removal_mask_ &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

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
  BrowsingDataRemover* remover = BrowsingDataRemover::CreateForRange(profile(),
      remove_since_, base::Time::Max());
  remover->AddObserver(this);
  remover->Remove(removal_mask_, origin_set_mask_);
}

int BrowsingDataExtensionFunction::ParseOriginSetMask(
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
  return BrowsingDataRemover::REMOVE_COOKIES;
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

int RemoveServerBoundCertsFunction::GetRemovalMask() const {
  return BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS;
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
