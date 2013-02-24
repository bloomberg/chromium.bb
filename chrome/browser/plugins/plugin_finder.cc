// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer.h"
#endif

using base::DictionaryValue;
using content::PluginService;

namespace {

typedef std::map<std::string, PluginMetadata*> PluginMap;

// Gets the full path of the plug-in file as the identifier.
std::string GetLongIdentifier(const webkit::WebPluginInfo& plugin) {
  return plugin.path.AsUTF8Unsafe();
}

// Gets the base name of the file path as the identifier.
std::string GetIdentifier(const webkit::WebPluginInfo& plugin) {
  return plugin.path.BaseName().AsUTF8Unsafe();
}

// Gets the plug-in group name as the plug-in name if it is not empty or
// the filename without extension if the name is empty.
static string16 GetGroupName(const webkit::WebPluginInfo& plugin) {
  if (!plugin.name.empty())
    return plugin.name;

  base::FilePath::StringType path =
      plugin.path.BaseName().RemoveExtension().value();
#if defined(OS_POSIX)
  return UTF8ToUTF16(path);
#elif defined(OS_WIN)
  return base::WideToUTF16(path);
#endif
}

void LoadMimeTypes(bool matching_mime_types,
                   const DictionaryValue* plugin_dict,
                   PluginMetadata* plugin) {
  const ListValue* mime_types = NULL;
  std::string list_key =
      matching_mime_types ? "matching_mime_types" : "mime_types";
  if (!plugin_dict->GetList(list_key, &mime_types))
    return;

  bool success = false;
  for (ListValue::const_iterator mime_type_it = mime_types->begin();
       mime_type_it != mime_types->end(); ++mime_type_it) {
    std::string mime_type_str;
    success = (*mime_type_it)->GetAsString(&mime_type_str);
    DCHECK(success);
    if (matching_mime_types) {
      plugin->AddMatchingMimeType(mime_type_str);
    } else {
      plugin->AddMimeType(mime_type_str);
    }
  }
}

PluginMetadata* CreatePluginMetadata(
    const std::string& identifier,
    const DictionaryValue* plugin_dict) {
  std::string url;
  bool success = plugin_dict->GetString("url", &url);
  std::string help_url;
  plugin_dict->GetString("help_url", &help_url);
  string16 name;
  success = plugin_dict->GetString("name", &name);
  DCHECK(success);
  bool display_url = false;
  plugin_dict->GetBoolean("displayurl", &display_url);
  string16 group_name_matcher;
  success = plugin_dict->GetString("group_name_matcher", &group_name_matcher);
  DCHECK(success);
  std::string language_str;
  plugin_dict->GetString("lang", &language_str);

  PluginMetadata* plugin = new PluginMetadata(identifier,
                                              name,
                                              display_url,
                                              GURL(url),
                                              GURL(help_url),
                                              group_name_matcher,
                                              language_str);
  const ListValue* versions = NULL;
  if (plugin_dict->GetList("versions", &versions)) {
    for (ListValue::const_iterator it = versions->begin();
         it != versions->end(); ++it) {
      DictionaryValue* version_dict = NULL;
      if (!(*it)->GetAsDictionary(&version_dict)) {
        NOTREACHED();
        continue;
      }
      std::string version;
      success = version_dict->GetString("version", &version);
      DCHECK(success);
      std::string status_str;
      success = version_dict->GetString("status", &status_str);
      DCHECK(success);
      PluginMetadata::SecurityStatus status =
          PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
      success = PluginMetadata::ParseSecurityStatus(status_str, &status);
      DCHECK(success);
      plugin->AddVersion(Version(version), status);
    }
  }

  LoadMimeTypes(false, plugin_dict, plugin);
  LoadMimeTypes(true, plugin_dict, plugin);
  return plugin;
}

}  // namespace

// static
void PluginFinder::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisablePluginFinder, false);
}

// static
PluginFinder* PluginFinder::GetInstance() {
  // PluginFinder::GetInstance() is the only method that's allowed to call
  // Singleton<PluginFinder>::get().
  return Singleton<PluginFinder>::get();
}

PluginFinder::PluginFinder() : version_(-1) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PluginFinder::Init() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Load the built-in plug-in list first. If we have a newer version stored
  // locally or download one, we will replace this one with it.
  scoped_ptr<DictionaryValue> plugin_list(LoadBuiltInPluginList());
  DCHECK(plugin_list);
  ReinitializePlugins(plugin_list.get());
}

// static
DictionaryValue* PluginFinder::LoadBuiltInPluginList() {
  base::StringPiece json_resource(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PLUGIN_DB_JSON));
  std::string error_str;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json_resource,
      base::JSON_PARSE_RFC,
      NULL,
      &error_str));
  if (!value.get()) {
    DLOG(ERROR) << error_str;
    return NULL;
  }
  if (value->GetType() != base::Value::TYPE_DICTIONARY)
    return NULL;
  return static_cast<base::DictionaryValue*>(value.release());
}

PluginFinder::~PluginFinder() {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  STLDeleteValues(&installers_);
#endif
  STLDeleteValues(&identifier_plugin_);
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
bool PluginFinder::FindPlugin(
    const std::string& mime_type,
    const std::string& language,
    PluginInstaller** installer,
    scoped_ptr<PluginMetadata>* plugin_metadata) {
  if (g_browser_process->local_state()->GetBoolean(prefs::kDisablePluginFinder))
    return false;

  base::AutoLock lock(mutex_);
  PluginMap::const_iterator metadata_it = identifier_plugin_.begin();
  for (; metadata_it != identifier_plugin_.end(); ++metadata_it) {
    if (language == metadata_it->second->language() &&
        metadata_it->second->HasMimeType(mime_type)) {
      *plugin_metadata = metadata_it->second->Clone();

      std::map<std::string, PluginInstaller*>::const_iterator installer_it =
          installers_.find(metadata_it->second->identifier());
      DCHECK(installer_it != installers_.end());
      *installer = installer_it->second;
      return true;
    }
  }
  return false;
}

bool PluginFinder::FindPluginWithIdentifier(
    const std::string& identifier,
    PluginInstaller** installer,
    scoped_ptr<PluginMetadata>* plugin_metadata) {
  base::AutoLock lock(mutex_);
  PluginMap::const_iterator metadata_it = identifier_plugin_.find(identifier);
  if (metadata_it == identifier_plugin_.end())
    return false;
  *plugin_metadata = metadata_it->second->Clone();

  if (installer) {
    std::map<std::string, PluginInstaller*>::const_iterator installer_it =
        installers_.find(identifier);
    if (installer_it == installers_.end())
      return false;
    *installer = installer_it->second;
  }
  return true;
}
#endif

void PluginFinder::ReinitializePlugins(
    const base::DictionaryValue* plugin_list) {
  base::AutoLock lock(mutex_);
  int version = 0;  // If no version is defined, we default to 0.
  const char kVersionKey[] = "x-version";
  plugin_list->GetInteger(kVersionKey, &version);
  if (version <= version_)
    return;

  version_ = version;

  STLDeleteValues(&identifier_plugin_);
  identifier_plugin_.clear();

  for (DictionaryValue::Iterator plugin_it(*plugin_list);
      plugin_it.HasNext(); plugin_it.Advance()) {
    const DictionaryValue* plugin = NULL;
    const std::string& identifier = plugin_it.key();
    if (plugin_list->GetDictionaryWithoutPathExpansion(identifier, &plugin)) {
      DCHECK(!identifier_plugin_[identifier]);
      identifier_plugin_[identifier] = CreatePluginMetadata(identifier, plugin);

#if defined(ENABLE_PLUGIN_INSTALLATION)
      if (installers_.find(identifier) == installers_.end())
        installers_[identifier] = new PluginInstaller();
#endif
    }
  }
}

string16 PluginFinder::FindPluginNameWithIdentifier(
    const std::string& identifier) {
  base::AutoLock lock(mutex_);
  PluginMap::const_iterator it = identifier_plugin_.find(identifier);
  string16 name;
  if (it != identifier_plugin_.end())
    name = it->second->name();

  return name.empty() ? UTF8ToUTF16(identifier) : name;
}

scoped_ptr<PluginMetadata> PluginFinder::GetPluginMetadata(
    const webkit::WebPluginInfo& plugin) {
  base::AutoLock lock(mutex_);
  for (PluginMap::const_iterator it = identifier_plugin_.begin();
       it != identifier_plugin_.end(); ++it) {
    if (!it->second->MatchesPlugin(plugin))
      continue;

    return it->second->Clone();
  }

  // The plug-in metadata was not found, create a dummy one holding
  // the name, identifier and group name only.
  std::string identifier = GetIdentifier(plugin);
  PluginMetadata* metadata = new PluginMetadata(identifier,
                                                GetGroupName(plugin),
                                                false, GURL(), GURL(),
                                                plugin.name,
                                                "");
  for (size_t i = 0; i < plugin.mime_types.size(); ++i)
    metadata->AddMatchingMimeType(plugin.mime_types[i].mime_type);

  DCHECK(metadata->MatchesPlugin(plugin));
  if (identifier_plugin_.find(identifier) != identifier_plugin_.end())
    identifier = GetLongIdentifier(plugin);

  DCHECK(identifier_plugin_.find(identifier) == identifier_plugin_.end());
  identifier_plugin_[identifier] = metadata;
  return metadata->Clone();
}
