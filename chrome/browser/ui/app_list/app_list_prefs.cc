// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_prefs.h"
#include "chrome/browser/ui/app_list/app_list_prefs_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace app_list {

namespace {

// App list ordering and folder data.
const char kPrefModel[] = "app_list.model";

const char kModelItemPosition[] = "position";
const char kModelItemType[] = "item_type";
const char kModelItemParentId[] = "parent_id";
const char kModelItemName[] = "name";

}  // namespace

// AppListInfo

AppListPrefs::AppListInfo::AppListInfo() : item_type(ITEM_TYPE_INVALID) {
}

AppListPrefs::AppListInfo::~AppListInfo() {
}

scoped_ptr<base::DictionaryValue>
AppListPrefs::AppListInfo::CreateDictFromAppListInfo() const {
  scoped_ptr<base::DictionaryValue> item_dict(new base::DictionaryValue());
  item_dict->SetString(kModelItemPosition, position.ToInternalValue());
  item_dict->SetString(kModelItemParentId, parent_id);
  item_dict->SetString(kModelItemName, name);
  item_dict->SetInteger(kModelItemType, item_type);
  return item_dict;
}

// static
scoped_ptr<AppListPrefs::AppListInfo>
AppListPrefs::AppListInfo::CreateAppListInfoFromDict(
    const base::DictionaryValue* item_dict) {
  std::string item_ordinal_string;
  scoped_ptr<AppListInfo> info(new AppListPrefs::AppListInfo());
  int item_type_int = -1;
  if (!item_dict ||
      !item_dict->GetString(kModelItemPosition, &item_ordinal_string) ||
      !item_dict->GetString(kModelItemParentId, &info->parent_id) ||
      !item_dict->GetString(kModelItemName, &info->name) ||
      !item_dict->GetInteger(kModelItemType, &item_type_int) ||
      item_type_int < ITEM_TYPE_BEGIN || item_type_int > ITEM_TYPE_END) {
    return scoped_ptr<AppListInfo>();
  }

  info->position = syncer::StringOrdinal(item_ordinal_string);
  info->item_type = static_cast<ItemType>(item_type_int);
  return info;
}

// AppListPrefs

AppListPrefs::AppListPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
}

AppListPrefs::~AppListPrefs() {
}

// static
void AppListPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kPrefModel);
}

// static
AppListPrefs* AppListPrefs::Create(PrefService* pref_service) {
  return new AppListPrefs(pref_service);
}

// static
AppListPrefs* AppListPrefs::Get(content::BrowserContext* context) {
  return AppListPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

void AppListPrefs::SetAppListInfo(const std::string& id,
                                  const AppListInfo& info) {
  DictionaryPrefUpdate update(pref_service_, kPrefModel);
  update->Set(id, info.CreateDictFromAppListInfo().release());
}

scoped_ptr<AppListPrefs::AppListInfo> AppListPrefs::GetAppListInfo(
    const std::string& id) const {
  const base::DictionaryValue* model_dict =
      pref_service_->GetDictionary(kPrefModel);
  DCHECK(model_dict);
  const base::DictionaryValue* item_dict = NULL;
  if (!model_dict->GetDictionary(id, &item_dict))
    return scoped_ptr<AppListInfo>();

  return AppListInfo::CreateAppListInfoFromDict(item_dict);
}

void AppListPrefs::GetAllAppListInfos(AppListInfoMap* out) const {
  out->clear();
  const base::DictionaryValue* model_dict =
      pref_service_->GetDictionary(kPrefModel);
  DCHECK(model_dict);

  for (base::DictionaryValue::Iterator it(*model_dict); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* item_dict = NULL;
    it.value().GetAsDictionary(&item_dict);
    DCHECK(item_dict);
    (*out)[it.key()] = *AppListInfo::CreateAppListInfoFromDict(item_dict);
  }
}

void AppListPrefs::DeleteAppListInfo(const std::string& id) {
  DictionaryPrefUpdate model_dict(pref_service_, kPrefModel);
  model_dict->Remove(id, NULL);
}

}  // namespace app_list
