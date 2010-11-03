// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/master_preferences.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "googleurl/src/gurl.h"


namespace {

const char kDistroDict[] = "distribution";
const char kFirstRunTabs[] = "first_run_tabs";

bool GetGURLFromValue(const Value* in_value, GURL* out_value) {
  if (!in_value || !out_value)
    return false;
  std::string url;
  in_value->GetAsString(&url);
  GURL gurl(url);
  *out_value = gurl;
  return true;
}

std::vector<GURL> GetNamedList(const char* name,
                               const DictionaryValue* prefs) {
  std::vector<GURL> list;
  if (!prefs)
    return list;
  ListValue* value_list = NULL;
  if (!prefs->GetList(name, &value_list))
    return list;
  for (size_t i = 0; i < value_list->GetSize(); ++i) {
    Value* entry;
    GURL gurl_entry;
    if (!value_list->Get(i, &entry) || !GetGURLFromValue(entry, &gurl_entry)) {
      NOTREACHED();
      break;
    }
    list.push_back(gurl_entry);
  }
  return list;
}

DictionaryValue* ParseDistributionPreferences(
    const FilePath& master_prefs_path) {
  std::string json_data;
  if (!file_util::ReadFileToString(master_prefs_path, &json_data)) {
    LOG(WARNING) << "Failed to read master prefs file. ";
    return NULL;
  }
  JSONStringValueSerializer json(json_data);
  std::string error;
  scoped_ptr<Value> root(json.Deserialize(NULL, &error));
  if (!root.get()) {
    LOG(WARNING) << "Failed to parse master prefs file: " << error;
    return NULL;
  }
  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "Failed to parse master prefs file: "
                 << "Root item must be a dictionary.";
    return NULL;
  }
  return static_cast<DictionaryValue*>(root.release());
}

}  // namespace

namespace installer_util {

MasterPreferences::MasterPreferences(const CommandLine& cmd_line)
    : distribution_(NULL), preferences_read_from_file_(false) {
#if defined(OS_WIN)
  if (cmd_line.HasSwitch(installer_util::switches::kInstallerData)) {
    FilePath prefs_path(cmd_line.GetSwitchValuePath(
        installer_util::switches::kInstallerData));
    this->MasterPreferences::MasterPreferences(prefs_path);
  } else {
    master_dictionary_.reset(new DictionaryValue());
  }

  DCHECK(master_dictionary_.get());

  // A simple map from command line switches to equivalent switches in the
  // distribution dictionary.  Currently all switches added will be set to
  // 'true'.
  static const struct CmdLineSwitchToDistributionSwitch {
    const wchar_t* cmd_line_switch;
    const char* distribution_switch;
  } translate_switches[] = {
    { installer_util::switches::kChromeFrame,
      installer_util::master_preferences::kChromeFrame },
    { installer_util::switches::kCreateAllShortcuts,
      installer_util::master_preferences::kCreateAllShortcuts },
    { installer_util::switches::kDoNotCreateShortcuts,
      installer_util::master_preferences::kDoNotCreateShortcuts },
    { installer_util::switches::kMsi,
      installer_util::master_preferences::kMsi },
    { installer_util::switches::kDoNotRegisterForUpdateLaunch,
      installer_util::master_preferences::kDoNotRegisterForUpdateLaunch },
    { installer_util::switches::kDoNotLaunchChrome,
      installer_util::master_preferences::kDoNotLaunchChrome },
    { installer_util::switches::kMakeChromeDefault,
      installer_util::master_preferences::kMakeChromeDefault },
    { installer_util::switches::kSystemLevel,
      installer_util::master_preferences::kSystemLevel },
    { installer_util::switches::kVerboseLogging,
      installer_util::master_preferences::kVerboseLogging },
    { installer_util::switches::kAltDesktopShortcut,
      installer_util::master_preferences::kAltShortcutText },
  };

  std::string name(kDistroDict);
  for (int i = 0; i < arraysize(translate_switches); ++i) {
    if (cmd_line.HasSwitch(translate_switches[i].cmd_line_switch)) {
      name.resize(arraysize(kDistroDict) - 1);
      name.append(".").append(translate_switches[i].distribution_switch);
      master_dictionary_->SetBoolean(name, true);
    }
  }

  // Cache a pointer to the distribution dictionary. Ignore errors if any.
  master_dictionary_->GetDictionary(kDistroDict, &distribution_);
#endif
}

MasterPreferences::MasterPreferences(const FilePath& prefs_path)
    : distribution_(NULL), preferences_read_from_file_(false) {
  master_dictionary_.reset(ParseDistributionPreferences(prefs_path));
  LOG_IF(ERROR, !master_dictionary_.get()) << "Failed to parse "
      << prefs_path.value();
  if (!master_dictionary_.get()) {
    master_dictionary_.reset(new DictionaryValue());
  } else {
    preferences_read_from_file_ = true;
    // Cache a pointer to the distribution dictionary.
    master_dictionary_->GetDictionary(kDistroDict, &distribution_);
  }
}

MasterPreferences::~MasterPreferences() {
}

bool MasterPreferences::GetBool(const std::string& name, bool* value) const {
  bool ret = false;
  if (distribution_)
    ret = distribution_->GetBoolean(name, value);
  return ret;
}

bool MasterPreferences::GetInt(const std::string& name, int* value) const {
  bool ret = false;
  if (distribution_)
    ret = distribution_->GetInteger(name, value);
  return ret;
}

bool MasterPreferences::GetString(const std::string& name,
                                  std::string* value) const {
  bool ret = false;
  if (distribution_)
    ret = (distribution_->GetString(name, value) && !value->empty());
  return ret;
}

std::vector<GURL> MasterPreferences::GetFirstRunTabs() const {
  return GetNamedList(kFirstRunTabs, master_dictionary_.get());
}

bool MasterPreferences::GetExtensionsBlock(DictionaryValue** extensions) const {
  return master_dictionary_->GetDictionary(
      master_preferences::kExtensionsBlock, extensions);
}

}  // installer_util
