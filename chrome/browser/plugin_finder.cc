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
#include "ui/base/resource/resource_bundle.h"

// static
PluginFinder* PluginFinder::GetInstance() {
  return Singleton<PluginFinder>::get();
}

PluginFinder::PluginFinder() : plugin_list_(LoadPluginList()) {
  if (!plugin_list_.get()) {
    NOTREACHED();
    plugin_list_.reset(new base::ListValue());
  }
}

// static
scoped_ptr<base::ListValue> PluginFinder::LoadPluginList() {
  return scoped_ptr<base::ListValue>(LoadPluginListInternal()).Pass();
}

base::ListValue* PluginFinder::LoadPluginListInternal() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  base::StringPiece json_resource(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PLUGIN_DB_JSON));
  bool allow_trailing_comma = false;
  std::string error_str;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json_resource.as_string(),
      allow_trailing_comma,
      NULL,
      &error_str));
  if (!value.get()) {
    DLOG(ERROR) << error_str;
    return NULL;
  }
  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict))
    return NULL;
  base::ListValue* list = NULL;
  if (!dict->GetList("plugins", &list))
    return NULL;
  return list->DeepCopy();
#else
  return new base::ListValue();
#endif
}

PluginFinder::~PluginFinder() {
  STLDeleteValues(&installers_);
}

void PluginFinder::FindPlugin(
    const std::string& mime_type,
    const std::string& language,
    const FindPluginCallback& found_callback,
    const base::Closure& not_found_callback) {
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kDisablePluginFinder)) {
    MessageLoop::current()->PostTask(FROM_HERE, not_found_callback);
    return;
  }
  for (ListValue::const_iterator plugin_it = plugin_list_->begin();
       plugin_it != plugin_list_->end(); ++plugin_it) {
    const base::DictionaryValue* plugin = NULL;
    if (!(*plugin_it)->GetAsDictionary(&plugin)) {
      NOTREACHED();
      continue;
    }
    std::string language_str;
    bool success = plugin->GetString("lang", &language_str);
    DCHECK(success);
    if (language_str != language)
      continue;
    ListValue* mime_types = NULL;
    success = plugin->GetList("mime_types", &mime_types);
    DCHECK(success);
    for (ListValue::const_iterator mime_type_it = mime_types->begin();
         mime_type_it != mime_types->end(); ++mime_type_it) {
      std::string mime_type_str;
      success = (*mime_type_it)->GetAsString(&mime_type_str);
      DCHECK(success);
      if (mime_type_str == mime_type) {
        std::string identifier;
        success = plugin->GetString("identifier", &identifier);
        DCHECK(success);
        PluginInstaller* installer = installers_[identifier];
        if (!installer) {
          std::string url;
          success = plugin->GetString("url", &url);
          DCHECK(success);
          std::string help_url;
          plugin->GetString("help_url", &help_url);
          string16 name;
          success = plugin->GetString("name", &name);
          DCHECK(success);
          bool display_url = false;
          plugin->GetBoolean("displayurl", &display_url);
          installer = new PluginInstaller(identifier,
                                          GURL(url), GURL(help_url), name,
                                          display_url);
          installers_[identifier] = installer;
        }
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(found_callback, installer));
        return;
      }
    }
  }
  MessageLoop::current()->PostTask(FROM_HERE, not_found_callback);
}
