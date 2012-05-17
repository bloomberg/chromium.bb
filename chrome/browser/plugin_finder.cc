// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_finder.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using base::DictionaryValue;

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

PluginFinder::PluginFinder() : plugin_list_(LoadPluginList()) {
  if (!plugin_list_.get())
    plugin_list_.reset(new DictionaryValue());
}

// static
DictionaryValue* PluginFinder::LoadPluginList() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  base::StringPiece json_resource(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PLUGIN_DB_JSON, ui::SCALE_FACTOR_NONE));
  std::string error_str;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json_resource.as_string(),
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
  STLDeleteValues(&installers_);
}

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
    DCHECK(success);
    if (language_str != language)
      continue;
    ListValue* mime_types = NULL;
    plugin->GetList("mime_types", &mime_types);
    DCHECK(success);
    for (ListValue::const_iterator mime_type_it = mime_types->begin();
         mime_type_it != mime_types->end(); ++mime_type_it) {
      std::string mime_type_str;
      success = (*mime_type_it)->GetAsString(&mime_type_str);
      DCHECK(success);
      if (mime_type_str == mime_type) {
        std::string identifier = plugin_it.key();
        std::map<std::string, PluginInstaller*>::const_iterator installer =
            installers_.find(identifier);
        if (installer != installers_.end())
          return installer->second;
        return CreateInstaller(identifier, plugin);
      }
    }
  }
  return NULL;
}

PluginInstaller* PluginFinder::FindPluginWithIdentifier(
    const std::string& identifier) {
  std::map<std::string, PluginInstaller*>::const_iterator it =
      installers_.find(identifier);
  if (it != installers_.end())
    return it->second;
  DictionaryValue* plugin = NULL;
  if (plugin_list_->GetDictionaryWithoutPathExpansion(identifier, &plugin))
    return CreateInstaller(identifier, plugin);
  return NULL;
}

PluginInstaller* PluginFinder::CreateInstaller(
    const std::string& identifier,
    const DictionaryValue* plugin_dict) {
  DCHECK(!installers_[identifier]);
  std::string url;
  bool success = plugin_dict->GetString("url", &url);
  DCHECK(success);
  std::string help_url;
  plugin_dict->GetString("help_url", &help_url);
  string16 name;
  success = plugin_dict->GetString("name", &name);
  DCHECK(success);
  bool display_url = false;
  plugin_dict->GetBoolean("displayurl", &display_url);
  bool requires_authorization = true;
  plugin_dict->GetBoolean("requires_authorization", &requires_authorization);
  PluginInstaller* installer = new PluginInstaller(identifier,
                                                   GURL(url),
                                                   GURL(help_url),
                                                   name,
                                                   display_url,
                                                   requires_authorization);
  installers_[identifier] = installer;
  return installer;
}
