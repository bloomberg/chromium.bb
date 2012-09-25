// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer.h"
#endif

using base::DictionaryValue;
using content::PluginService;

namespace {

// Gets the base name of the file path as the identifier.
static std::string GetIdentifier(const webkit::WebPluginInfo& plugin) {
#if defined(OS_POSIX)
  return plugin.path.BaseName().value();
#elif defined(OS_WIN)
  return base::SysWideToUTF8(plugin.path.BaseName().value());
#endif
}

// Gets the plug-in group name as the plug-in name if it is not empty or
// the filename without extension if the name is empty.
static string16 GetGroupName(const webkit::WebPluginInfo& plugin) {
  if (!plugin.name.empty())
    return plugin.name;

  FilePath::StringType path = plugin.path.BaseName().RemoveExtension().value();
#if defined(OS_POSIX)
  return UTF8ToUTF16(path);
#elif defined(OS_WIN)
  return WideToUTF16(path);
#endif
}

}  // namespace

// TODO(ibraaaa): DELETE. http://crbug.com/124396
// static
void PluginFinder::Get(const base::Callback<void(PluginFinder*)>& cb) {
  // At a later point we might want to do intialization here that needs to be
  // done asynchronously, like loading the plug-in list from disk or from a URL.
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(cb, GetInstance()));
}

// static
PluginFinder* PluginFinder::GetInstance() {
  // PluginFinder::GetInstance() is the only method that's allowed to call
  // Singleton<PluginFinder>::get().
  return Singleton<PluginFinder>::get();
}

PluginFinder::PluginFinder() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

// TODO(ibraaaa): initialize |plugin_list_| from Local State.
// http://crbug.com/124396
void PluginFinder::Init() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  plugin_list_.reset(LoadPluginList());
  if (!plugin_list_.get())
    plugin_list_.reset(new DictionaryValue());

  for (DictionaryValue::Iterator plugin_it(*plugin_list_);
      plugin_it.HasNext(); plugin_it.Advance()) {
    DictionaryValue* plugin = NULL;
    const std::string& identifier = plugin_it.key();
    if (plugin_list_->GetDictionaryWithoutPathExpansion(identifier, &plugin)) {
      PluginMetadata* metadata = CreatePluginMetadata(identifier, plugin);
      identifier_plugin_[identifier] = metadata;

#if defined(ENABLE_PLUGIN_INSTALLATION)
      installers_[identifier] = new PluginInstaller(metadata);
#endif
    }
  }
}

// static
DictionaryValue* PluginFinder::LoadPluginList() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  base::StringPiece json_resource(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PLUGIN_DB_JSON, ui::SCALE_FACTOR_NONE));
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
#else
  return new DictionaryValue();
#endif
}

PluginFinder::~PluginFinder() {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  STLDeleteValues(&installers_);
#endif
  STLDeleteValues(&identifier_plugin_);
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
PluginInstaller* PluginFinder::FindPlugin(const std::string& mime_type,
                                          const std::string& language) {
  if (g_browser_process->local_state()->GetBoolean(prefs::kDisablePluginFinder))
    return NULL;
  for (DictionaryValue::Iterator plugin_it(*plugin_list_);
       plugin_it.HasNext(); plugin_it.Advance()) {
    const DictionaryValue* plugin = NULL;
    if (!plugin_it.value().GetAsDictionary(&plugin)) {
      NOTREACHED();
      continue;
    }
    std::string language_str;
    bool success = plugin->GetString("lang", &language_str);
    if (language_str != language)
      continue;
    const ListValue* mime_types = NULL;
    plugin->GetList("mime_types", &mime_types);
    DCHECK(success);
    for (ListValue::const_iterator mime_type_it = mime_types->begin();
         mime_type_it != mime_types->end(); ++mime_type_it) {
      std::string mime_type_str;
      success = (*mime_type_it)->GetAsString(&mime_type_str);
      DCHECK(success);
      if (mime_type_str == mime_type) {
        std::string identifier = plugin_it.key();
        {
          base::AutoLock lock(mutex_);
          std::map<std::string, PluginInstaller*>::const_iterator installer =
              installers_.find(identifier);
          DCHECK(installer != installers_.end());
          return installer->second;
        }
      }
    }
  }
  return NULL;
}

PluginInstaller* PluginFinder::FindPluginWithIdentifier(
    const std::string& identifier) {
  base::AutoLock lock(mutex_);
  std::map<std::string, PluginInstaller*>::const_iterator it =
      installers_.find(identifier);
  if (it != installers_.end())
    return it->second;

  return NULL;
}
#endif

PluginMetadata* PluginFinder::FindPluginMetadataWithIdentifier(
    const std::string& identifier) {
  base::AutoLock lock(mutex_);
  std::map<std::string, PluginMetadata*>::const_iterator it =
      identifier_plugin_.find(identifier);
  if (it != identifier_plugin_.end())
    return it->second;

  return NULL;
}

PluginMetadata* PluginFinder::CreatePluginMetadata(
    const std::string& identifier,
    const DictionaryValue* plugin_dict) {
  DCHECK(!identifier_plugin_[identifier]);
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

  PluginMetadata* plugin = new PluginMetadata(identifier,
                                              name,
                                              display_url,
                                              GURL(url),
                                              GURL(help_url),
                                              group_name_matcher);
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

  return plugin;
}

PluginMetadata* PluginFinder::GetPluginMetadata(
    const webkit::WebPluginInfo& plugin) {
  base::AutoLock lock(mutex_);
  if (name_plugin_.find(plugin.name) != name_plugin_.end())
    return name_plugin_[plugin.name];

  // Use the group name matcher to find the plug-in metadata we want.
  for (std::map<std::string, PluginMetadata*>::const_iterator it =
      identifier_plugin_.begin(); it != identifier_plugin_.end(); ++it) {
    if (!it->second->MatchesPlugin(plugin))
      continue;

    name_plugin_[plugin.name] = it->second;
    return it->second;
  }

  // The plug-in metadata was not found, create a dummy one holding
  // the name, identifier and group name only.
  std::string identifier = GetIdentifier(plugin);
  PluginMetadata* metadata = new PluginMetadata(identifier,
                                                GetGroupName(plugin),
                                                false, GURL(), GURL(),
                                                GetGroupName(plugin));

  name_plugin_[plugin.name] = metadata;
  identifier_plugin_[identifier] = metadata;
  return metadata;
}
