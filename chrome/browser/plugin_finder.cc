// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_finder.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

PluginFinder* PluginFinder::GetInstance() {
  return Singleton<PluginFinder>::get();
}

PluginFinder::PluginFinder() {
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
  DLOG_IF(ERROR, !value.get()) << error_str;
  if (value->IsType(base::Value::TYPE_DICTIONARY)) {
    base::DictionaryValue* dict =
        static_cast<base::DictionaryValue*>(value.get());
    base::ListValue* list = NULL;
    dict->GetList("plugins", &list);
    plugin_list_.reset(list->DeepCopy());
  }
  DCHECK(plugin_list_.get());
#endif
  if (!plugin_list_.get())
    plugin_list_.reset(new base::ListValue());
}

PluginFinder::~PluginFinder() {
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
    if (!(*plugin_it)->IsType(base::Value::TYPE_DICTIONARY)) {
      NOTREACHED();
      continue;
    }
    const base::DictionaryValue* plugin =
        static_cast<const base::DictionaryValue*>(*plugin_it);
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
        std::string url;
        success = plugin->GetString("url", &url);
        DCHECK(success);
        string16 name;
        success = plugin->GetString("name", &name);
        DCHECK(success);
        bool display_url = false;
        plugin->GetBoolean("displayurl", &display_url);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(found_callback, GURL(url), name, display_url));
        return;
      }
    }
  }
  MessageLoop::current()->PostTask(FROM_HERE, not_found_callback);
}

