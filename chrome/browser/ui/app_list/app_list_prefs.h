// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_PREFS_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_PREFS_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "sync/api/string_ordinal.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace app_list {

class AppListPrefs : public KeyedService {
 public:
  // An app list item's information. This is enough data to reconstruct the full
  // hierarchy and ordering information of the app list.
  struct AppListInfo {
    enum ItemType {
      ITEM_TYPE_INVALID = 0,
      ITEM_TYPE_BEGIN,
      APP_ITEM = ITEM_TYPE_BEGIN,
      FOLDER_ITEM,

      // Do not change the order of this enum.
      // When adding values, remember to update ITEM_TYPE_END.
      ITEM_TYPE_END = FOLDER_ITEM,
    };
    AppListInfo();
    ~AppListInfo();
    scoped_ptr<base::DictionaryValue> CreateDictFromAppListInfo() const;

    static scoped_ptr<AppListPrefs::AppListInfo> CreateAppListInfoFromDict(
        const base::DictionaryValue* item_dict);

    // The id of the folder containing this item.
    std::string parent_id;

    // The name of this item.
    std::string name;

    // The position of this item in the app list.
    syncer::StringOrdinal position;

    // The type of app list item being represented.
    ItemType item_type;
  };

  typedef std::map<std::string, AppListInfo> AppListInfoMap;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static AppListPrefs* Get(content::BrowserContext* context);

  static AppListPrefs* Create(PrefService* pref_service);

  ~AppListPrefs() override;

  // Sets the app list info for |id|.
  void SetAppListInfo(const std::string& id, const AppListInfo& info);

  // Gets the app list info for |id|.
  scoped_ptr<AppListInfo> GetAppListInfo(const std::string& id) const;

  // Gets a map of all AppListInfo objects in the prefs.
  void GetAllAppListInfos(AppListInfoMap* out) const;

  // Deletes the app list info for |id|.
  void DeleteAppListInfo(const std::string& id);

 private:
  explicit AppListPrefs(PrefService* pref_service);

  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(AppListPrefs);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_PREFS_H_
