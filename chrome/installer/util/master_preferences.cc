// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/master_preferences.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/json_value_serializer.h"

namespace {
const wchar_t* kDistroDict = L"distribution";


}  // namespace

namespace installer_util {
namespace master_preferences {
const wchar_t kAltFirstRunBubble[] = L"oem_bubble";
const wchar_t kAltShortcutText[] = L"alternate_shortcut_text";
const wchar_t kChromeShortcutIconIndex[] = L"chrome_shortcut_icon_index";
const wchar_t kCreateAllShortcuts[] = L"create_all_shortcuts";
const wchar_t kDistroImportBookmarksPref[] = L"import_bookmarks";
const wchar_t kDistroImportHistoryPref[] = L"import_history";
const wchar_t kDistroImportHomePagePref[] = L"import_home_page";
const wchar_t kDistroImportSearchPref[] = L"import_search_engine";
const wchar_t kDistroPingDelay[] = L"ping_delay";
const wchar_t kDistroShowWelcomePage[] = L"show_welcome_page";
const wchar_t kDistroSkipFirstRunPref[] = L"skip_first_run_ui";
const wchar_t kDoNotCreateShortcuts[] = L"do_not_create_shortcuts";
const wchar_t kDoNotLaunchChrome[] = L"do_not_launch_chrome";
const wchar_t kDoNotRegisterForUpdateLaunch[] =
    L"do_not_register_for_update_launch";
const wchar_t kMakeChromeDefault[] = L"make_chrome_default";
const wchar_t kMakeChromeDefaultForUser[] = L"make_chrome_default_for_user";
const wchar_t kRequireEula[] = L"require_eula";
const wchar_t kSystemLevel[] = L"system_level";
const wchar_t kVerboseLogging[] = L"verbose_logging";
}

bool GetDistroBooleanPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                bool* value) {
  if (!prefs || !value)
    return false;

  DictionaryValue* distro = NULL;
  if (!prefs->GetDictionary(kDistroDict, &distro) || !distro)
    return false;

  if (!distro->GetBoolean(name, value))
    return false;

  return true;
}

bool GetDistroIntegerPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                int* value) {
  if (!prefs || !value)
    return false;

  DictionaryValue* distro = NULL;
  if (!prefs->GetDictionary(kDistroDict, &distro) || !distro)
    return false;

  if (!distro->GetInteger(name, value))
    return false;

  return true;
}

DictionaryValue* ParseDistributionPreferences(
    const FilePath& master_prefs_path) {
  if (!file_util::PathExists(master_prefs_path)) {
    LOG(WARNING) << "Master preferences file not found: "
                 << master_prefs_path.value();
    return NULL;
  }
  std::string json_data;
  if (!file_util::ReadFileToString(master_prefs_path, &json_data))
    return NULL;
  JSONStringValueSerializer json(json_data);
  scoped_ptr<Value> root(json.Deserialize(NULL));

  if (!root.get())
    return NULL;

  if (!root->IsType(Value::TYPE_DICTIONARY))
    return NULL;

  return static_cast<DictionaryValue*>(root.release());
}

std::vector<std::wstring> GetFirstRunTabs(const DictionaryValue* prefs) {
  std::vector<std::wstring> launch_tabs;
  if (!prefs)
    return launch_tabs;
  ListValue* tabs_list = NULL;
  if (!prefs->GetList(L"first_run_tabs", &tabs_list))
    return launch_tabs;
  for (size_t i = 0; i < tabs_list->GetSize(); ++i) {
    Value* entry;
    std::wstring tab_entry;
    if (!tabs_list->Get(i, &entry) || !entry->GetAsString(&tab_entry)) {
      NOTREACHED();
      break;
    }
    launch_tabs.push_back(tab_entry);
  }
  return launch_tabs;
}

bool SetDistroBooleanPreference(DictionaryValue* prefs,
                                const std::wstring& name,
                                bool value) {

  bool ret = false;
  if (prefs && !name.empty()) {
    std::wstring key(kDistroDict);
    key.append(L"." + name);
    if (prefs->SetBoolean(key, value))
      ret = true;
  }
  return ret;
}

}  // installer_util
