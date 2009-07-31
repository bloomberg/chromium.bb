// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/master_preferences.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/json_value_serializer.h"

namespace {
DictionaryValue* GetPrefsFromFile(const FilePath& master_prefs_path) {
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
}  // namespace

namespace installer_util {
namespace master_preferences {
// Boolean pref that triggers skipping the first run dialogs.
const wchar_t kDistroSkipFirstRunPref[] = L"skip_first_run_ui";
// Boolean pref that triggers loading the welcome page.
const wchar_t kDistroShowWelcomePage[] = L"show_welcome_page";
// Boolean pref that triggers silent import of the default search engine.
const wchar_t kDistroImportSearchPref[] = L"import_search_engine";
// Boolean pref that triggers silent import of the default browser history.
const wchar_t kDistroImportHistoryPref[] = L"import_history";
// Boolean pref that triggers silent import of the default browser bookmarks.
const wchar_t kDistroImportBookmarksPref[] = L"import_bookmarks";
// RLZ ping delay in seconds
const wchar_t kDistroPingDelay[] = L"ping_delay";
// Register Chrome as default browser for the current user.
const wchar_t kMakeChromeDefaultForUser[] = L"make_chrome_default_for_user";
// The following boolean prefs have the same semantics as the corresponding
// setup command line switches. See chrome/installer/util/util_constants.cc
// for more info.
// Create Desktop and QuickLaunch shortcuts.
const wchar_t kCreateAllShortcuts[] = L"create_all_shortcuts";
// Prevent installer from launching Chrome after a successful first install.
const wchar_t kDoNotLaunchChrome[] = L"do_not_launch_chrome";
// Register Chrome as default browser on the system.
const wchar_t kMakeChromeDefault[] = L"make_chrome_default";
// Install Chrome to system wise location.
const wchar_t kSystemLevel[] = L"system_level";
// Run installer in verbose mode.
const wchar_t kVerboseLogging[] = L"verbose_logging";
// Show EULA dialog and install only if accepted.
const wchar_t kRequireEula[] = L"require_eula";
// Use alternate shortcut text for the main shortcut.
const wchar_t kAltShortcutText[] = L"alternate_shortcut_text";
// Use alternate smaller first run info bubble.
const wchar_t kAltFirstRunBubble[] = L"oem_bubble";
// Boolean pref that triggers silent import of the default browser homepage.
const wchar_t kDistroImportHomePagePref[] = L"import_home_page";

const wchar_t kMasterPreferencesValid[] = L"master_preferencs_valid";
}

bool GetBooleanPreference(const DictionaryValue* prefs,
                          const std::wstring& name) {
  bool value = false;
  if (!prefs || !prefs->GetBoolean(name, &value))
    return false;
  return value;
}

bool GetDistributionPingDelay(const DictionaryValue* prefs,
                              int* ping_delay) {
  if (!prefs || !ping_delay)
    return false;

  // 90 seconds is the default that we want to use in case master preferences
  // is missing or corrupt.
  *ping_delay = 90;
  if (!prefs->GetInteger(master_preferences::kDistroPingDelay, ping_delay))
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

  scoped_ptr<DictionaryValue> json_root(GetPrefsFromFile(master_prefs_path));
  if (!json_root.get()) {
    LOG(WARNING) << "Failed to parse preferences file: "
                 << master_prefs_path.value();
    return NULL;
  }

  DictionaryValue* distro = NULL;
  if (!json_root->GetDictionary(L"distribution", &distro)) {
    LOG(WARNING) << "Failed to get distriubtion params: "
                 << master_prefs_path.value();
    return NULL;
  }
  return distro;
}

std::vector<std::wstring> ParseFirstRunTabs(const DictionaryValue* prefs) {
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

}  // installer_util
