// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"

#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/plugins/plugin_data_remover_helper.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"

using content::BrowserThread;
using browsing_data::ClearBrowsingDataTab;
using browsing_data::BrowsingDataType;

namespace extension_browsing_data_api_constants {
// Parameter name keys.
const char kDataRemovalPermittedKey[] = "dataRemovalPermitted";
const char kDataToRemoveKey[] = "dataToRemove";
const char kOptionsKey[] = "options";

// Type keys.
const char kAppCacheKey[] = "appcache";
const char kCacheKey[] = "cache";
const char kChannelIDsKey[] = "serverBoundCertificates";
const char kCookiesKey[] = "cookies";
const char kDownloadsKey[] = "downloads";
const char kFileSystemsKey[] = "fileSystems";
const char kFormDataKey[] = "formData";
const char kHistoryKey[] = "history";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kPasswordsKey[] = "passwords";
const char kPluginDataKey[] = "pluginData";
const char kServiceWorkersKey[] = "serviceWorkers";
const char kCacheStorageKey[] = "cacheStorage";
const char kWebSQLKey[] = "webSQL";

// Option keys.
const char kExtensionsKey[] = "extension";
const char kOriginTypesKey[] = "originTypes";
const char kProtectedWebKey[] = "protectedWeb";
const char kSinceKey[] = "since";
const char kUnprotectedWebKey[] = "unprotectedWeb";

// Errors!
// The placeholder will be filled by the name of the affected data type (e.g.,
// "history").
const char kBadDataTypeDetails[] = "Invalid value for data type '%s'.";
const char kDeleteProhibitedError[] =
    "Browsing history and downloads are not "
    "permitted to be removed.";

}  // namespace extension_browsing_data_api_constants

namespace {
int MaskForKey(const char* key) {
  if (strcmp(key, extension_browsing_data_api_constants::kAppCacheKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_APP_CACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCacheKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_CACHE;
  if (strcmp(key, extension_browsing_data_api_constants::kCookiesKey) == 0) {
    return content::BrowsingDataRemover::DATA_TYPE_COOKIES;
  }
  if (strcmp(key, extension_browsing_data_api_constants::kDownloadsKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
  if (strcmp(key, extension_browsing_data_api_constants::kFileSystemsKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS;
  if (strcmp(key, extension_browsing_data_api_constants::kFormDataKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
  if (strcmp(key, extension_browsing_data_api_constants::kHistoryKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
  if (strcmp(key, extension_browsing_data_api_constants::kIndexedDBKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB;
  if (strcmp(key, extension_browsing_data_api_constants::kLocalStorageKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE;
  if (strcmp(key,
             extension_browsing_data_api_constants::kChannelIDsKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS;
  if (strcmp(key, extension_browsing_data_api_constants::kPasswordsKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
  if (strcmp(key, extension_browsing_data_api_constants::kPluginDataKey) == 0)
    return ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;
  if (strcmp(key, extension_browsing_data_api_constants::kServiceWorkersKey) ==
      0)
    return content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS;
  if (strcmp(key, extension_browsing_data_api_constants::kCacheStorageKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE;
  if (strcmp(key, extension_browsing_data_api_constants::kWebSQLKey) == 0)
    return content::BrowsingDataRemover::DATA_TYPE_WEB_SQL;

  return 0;
}

// Returns false if any of the selected data types are not allowed to be
// deleted.
bool IsRemovalPermitted(int removal_mask, PrefService* prefs) {
  // Enterprise policy or user preference might prohibit deleting browser or
  // download history.
  if ((removal_mask & ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY) ||
      (removal_mask & content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS)) {
    return prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);
  }
  return true;
}

}  // namespace

bool BrowsingDataSettingsFunction::isDataTypeSelected(
    BrowsingDataType data_type,
    ClearBrowsingDataTab tab) {
  std::string pref_name;
  bool success = GetDeletionPreferenceFromDataType(data_type, tab, &pref_name);
  return success && prefs_->GetBoolean(pref_name);
}

ExtensionFunction::ResponseAction BrowsingDataSettingsFunction::Run() {
  prefs_ = Profile::FromBrowserContext(browser_context())->GetPrefs();

  ClearBrowsingDataTab tab = static_cast<ClearBrowsingDataTab>(
      prefs_->GetInteger(browsing_data::prefs::kLastClearBrowsingDataTab));

  // Fill origin types.
  // The "cookies" and "hosted apps" UI checkboxes both map to
  // REMOVE_SITE_DATA in browsing_data_remover.h, the former for the unprotected
  // web, the latter for  protected web data. There is no UI control for
  // extension data.
  std::unique_ptr<base::DictionaryValue> origin_types(
      new base::DictionaryValue);
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kUnprotectedWebKey,
      isDataTypeSelected(BrowsingDataType::COOKIES, tab));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kProtectedWebKey,
      isDataTypeSelected(BrowsingDataType::HOSTED_APPS_DATA, tab));
  origin_types->SetBoolean(
      extension_browsing_data_api_constants::kExtensionsKey, false);

  // Fill deletion time period.
  int period_pref =
      prefs_->GetInteger(browsing_data::GetTimePeriodPreferenceName(tab));

  browsing_data::TimePeriod period =
      static_cast<browsing_data::TimePeriod>(period_pref);
  double since = 0;
  if (period != browsing_data::TimePeriod::ALL_TIME) {
    base::Time time = browsing_data::CalculateBeginDeleteTime(period);
    since = time.ToJsTime();
  }

  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue);
  options->Set(extension_browsing_data_api_constants::kOriginTypesKey,
               std::move(origin_types));
  options->SetDouble(extension_browsing_data_api_constants::kSinceKey, since);

  // Fill dataToRemove and dataRemovalPermitted.
  std::unique_ptr<base::DictionaryValue> selected(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> permitted(new base::DictionaryValue);

  bool delete_site_data =
      isDataTypeSelected(BrowsingDataType::COOKIES, tab) ||
      isDataTypeSelected(BrowsingDataType::HOSTED_APPS_DATA, tab);

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kAppCacheKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCookiesKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kFileSystemsKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kIndexedDBKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
      extension_browsing_data_api_constants::kLocalStorageKey,
      delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kWebSQLKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
      extension_browsing_data_api_constants::kChannelIDsKey,
      delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kServiceWorkersKey,
             delete_site_data);
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCacheStorageKey,
             delete_site_data);

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kPluginDataKey,
             delete_site_data &&
                 prefs_->GetBoolean(prefs::kClearPluginLSODataEnabled));

  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kHistoryKey,
             isDataTypeSelected(BrowsingDataType::HISTORY, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kDownloadsKey,
             isDataTypeSelected(BrowsingDataType::DOWNLOADS, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kCacheKey,
             isDataTypeSelected(BrowsingDataType::CACHE, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kFormDataKey,
             isDataTypeSelected(BrowsingDataType::FORM_DATA, tab));
  SetDetails(selected.get(), permitted.get(),
             extension_browsing_data_api_constants::kPasswordsKey,
             isDataTypeSelected(BrowsingDataType::PASSWORDS, tab));

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->Set(extension_browsing_data_api_constants::kOptionsKey,
              std::move(options));
  result->Set(extension_browsing_data_api_constants::kDataToRemoveKey,
              std::move(selected));
  result->Set(extension_browsing_data_api_constants::kDataRemovalPermittedKey,
              std::move(permitted));
  return RespondNow(OneArgument(std::move(result)));
}

void BrowsingDataSettingsFunction::SetDetails(
    base::DictionaryValue* selected_dict,
    base::DictionaryValue* permitted_dict,
    const char* data_type,
    bool is_selected) {
  bool is_permitted = IsRemovalPermitted(MaskForKey(data_type), prefs_);
  selected_dict->SetBoolean(data_type, is_selected && is_permitted);
  permitted_dict->SetBoolean(data_type, is_permitted);
}

BrowsingDataRemoverFunction::BrowsingDataRemoverFunction() : observer_(this) {}

void BrowsingDataRemoverFunction::OnBrowsingDataRemoverDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observer_.RemoveAll();

  this->SendResponse(true);

  Release();  // Balanced in RunAsync.
}

bool BrowsingDataRemoverFunction::RunAsync() {
  // If we don't have a profile, something's pretty wrong.
  DCHECK(GetProfile());

  // Grab the initial |options| parameter, and parse out the arguments.
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &options));
  DCHECK(options);

  EXTENSION_FUNCTION_VALIDATE(
      ParseOriginTypeMask(*options, &origin_type_mask_));

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

  EXTENSION_FUNCTION_VALIDATE(GetRemovalMask(&removal_mask_));

  // Check for prohibited data types.
  if (!IsRemovalPermitted(removal_mask_, GetProfile()->GetPrefs())) {
    error_ = extension_browsing_data_api_constants::kDeleteProhibitedError;
    return false;
  }

  if (removal_mask_ &
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA) {
    // If we're being asked to remove plugin data, check whether it's actually
    // supported.
    PostTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            &BrowsingDataRemoverFunction::CheckRemovingPluginDataSupported,
            this, PluginPrefs::GetForProfile(GetProfile())));
  } else {
    StartRemoving();
  }

  // Will finish asynchronously.
  return true;
}

BrowsingDataRemoverFunction::~BrowsingDataRemoverFunction() {}

void BrowsingDataRemoverFunction::CheckRemovingPluginDataSupported(
    scoped_refptr<PluginPrefs> plugin_prefs) {
  if (!PluginDataRemoverHelper::IsSupported(plugin_prefs.get()))
    removal_mask_ &= ~ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BrowsingDataRemoverFunction::StartRemoving, this));
}

void BrowsingDataRemoverFunction::StartRemoving() {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(GetProfile());
  // Add a ref (Balanced in OnBrowsingDataRemoverDone)
  AddRef();

  // Create a BrowsingDataRemover, set the current object as an observer (so
  // that we're notified after removal) and call remove() with the arguments
  // we've generated above. We can use a raw pointer here, as the browsing data
  // remover is responsible for deleting itself once data removal is complete.
  observer_.Add(remover);
  remover->RemoveAndReply(
      remove_since_, base::Time::Max(),
      removal_mask_, origin_type_mask_, this);
}

bool BrowsingDataRemoverFunction::ParseOriginTypeMask(
    const base::DictionaryValue& options,
    int* origin_type_mask) {
  // Parse the |options| dictionary to generate the origin set mask. Default to
  // UNPROTECTED_WEB if the developer doesn't specify anything.
  *origin_type_mask = content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

  const base::DictionaryValue* d = NULL;
  if (options.HasKey(extension_browsing_data_api_constants::kOriginTypesKey)) {
    if (!options.GetDictionary(
            extension_browsing_data_api_constants::kOriginTypesKey, &d)) {
      return false;
    }
    bool value;

    // The developer specified something! Reset to 0 and parse the dictionary.
    *origin_type_mask = 0;

    // Unprotected web.
    if (d->HasKey(extension_browsing_data_api_constants::kUnprotectedWebKey)) {
      if (!d->GetBoolean(
              extension_browsing_data_api_constants::kUnprotectedWebKey,
              &value)) {
        return false;
      }
      *origin_type_mask |=
          value ? content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB : 0;
    }

    // Protected web.
    if (d->HasKey(extension_browsing_data_api_constants::kProtectedWebKey)) {
      if (!d->GetBoolean(
              extension_browsing_data_api_constants::kProtectedWebKey,
              &value)) {
        return false;
      }
      *origin_type_mask |=
          value ? content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB : 0;
    }

    // Extensions.
    if (d->HasKey(extension_browsing_data_api_constants::kExtensionsKey)) {
      if (!d->GetBoolean(extension_browsing_data_api_constants::kExtensionsKey,
                         &value)) {
        return false;
      }
      *origin_type_mask |=
          value ? ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION : 0;
    }
  }

  return true;
}

// Parses the |dataToRemove| argument to generate the removal mask.
// Returns false if parse was not successful, i.e. if 'dataToRemove' is not
// present or any data-type keys don't have supported (boolean) values.
bool BrowsingDataRemoveFunction::GetRemovalMask(int* removal_mask) {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(1, &data_to_remove))
    return false;

  *removal_mask = 0;
  for (base::DictionaryValue::Iterator i(*data_to_remove);
       !i.IsAtEnd();
       i.Advance()) {
    bool selected = false;
    if (!i.value().GetAsBoolean(&selected))
      return false;
    if (selected)
      *removal_mask |= MaskForKey(i.key().c_str());
  }

  return true;
}

bool BrowsingDataRemoveAppcacheFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_APP_CACHE;
  return true;
}

bool BrowsingDataRemoveCacheFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_CACHE;
  return true;
}

bool BrowsingDataRemoveCookiesFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_COOKIES |
                  content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS;
  return true;
}

bool BrowsingDataRemoveDownloadsFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
  return true;
}

bool BrowsingDataRemoveFileSystemsFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS;
  return true;
}

bool BrowsingDataRemoveFormDataFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
  return true;
}

bool BrowsingDataRemoveHistoryFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
  return true;
}

bool BrowsingDataRemoveIndexedDBFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB;
  return true;
}

bool BrowsingDataRemoveLocalStorageFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE;
  return true;
}

bool BrowsingDataRemovePluginDataFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;
  return true;
}

bool BrowsingDataRemovePasswordsFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
  return true;
}

bool BrowsingDataRemoveServiceWorkersFunction::GetRemovalMask(
    int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS;
  return true;
}

bool BrowsingDataRemoveCacheStorageFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE;
  return true;
}

bool BrowsingDataRemoveWebSQLFunction::GetRemovalMask(int* removal_mask) {
  *removal_mask = content::BrowsingDataRemover::DATA_TYPE_WEB_SQL;
  return true;
}
