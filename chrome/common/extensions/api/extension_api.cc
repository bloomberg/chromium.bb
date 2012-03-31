// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "googleurl/src/gurl.h"
#include "grit/common_resources.h"
#include "ui/base/resource/resource_bundle.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace extensions {

using api::GeneratedSchemas;

namespace {

// Returns whether the list at |name_space_node|.|child_kind| contains any
// children with an { "unprivileged": true } property.
bool HasUnprivilegedChild(const DictionaryValue* name_space_node,
                          const std::string& child_kind) {
  ListValue* child_list = NULL;
  name_space_node->GetList(child_kind, &child_list);
  if (!child_list)
    return false;

  for (size_t i = 0; i < child_list->GetSize(); ++i) {
    DictionaryValue* item = NULL;
    CHECK(child_list->GetDictionary(i, &item));
    bool unprivileged = false;
    if (item->GetBoolean("unprivileged", &unprivileged))
      return unprivileged;
  }

  return false;
}

base::StringPiece ReadFromResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
}

scoped_ptr<ListValue> LoadSchemaList(const base::StringPiece& schema) {
  std::string error_message;
  scoped_ptr<Value> result(
      base::JSONReader::ReadAndReturnError(
          schema.as_string(),
          false, // allow trailing commas
          NULL,  // error code
          &error_message));
  CHECK(result.get()) << error_message;
  CHECK(result->IsType(Value::TYPE_LIST));
  return scoped_ptr<ListValue>(static_cast<ListValue*>(result.release()));
}

}  // namespace

// static
ExtensionAPI* ExtensionAPI::GetInstance() {
  return Singleton<ExtensionAPI>::get();
}

void ExtensionAPI::LoadSchema(const base::StringPiece& schema) {
  scoped_ptr<ListValue> schema_list(LoadSchemaList(schema));
  std::string schema_namespace;

  while (!schema_list->empty()) {
    const DictionaryValue* schema = NULL;
    {
      Value* value = NULL;
      schema_list->Remove(schema_list->GetSize() - 1, &value);
      CHECK(value->IsType(Value::TYPE_DICTIONARY));
      schema = static_cast<const DictionaryValue*>(value);
    }

    CHECK(schema->GetString("namespace", &schema_namespace));
    schemas_[schema_namespace] = make_linked_ptr(schema);
    unloaded_schemas_.erase(schema_namespace);

    // Populate |{completely,partially}_unprivileged_apis_|.
    //
    // For "partially", only need to look at functions/events; even though
    // there are unprivileged properties (e.g. in extensions), access to those
    // never reaches C++ land.
    if (schema->HasKey("unprivileged")) {
      completely_unprivileged_apis_.insert(schema_namespace);
    } else if (HasUnprivilegedChild(schema, "functions") ||
               HasUnprivilegedChild(schema, "events")) {
      partially_unprivileged_apis_.insert(schema_namespace);
    }

    // Populate |url_matching_apis_|.
    ListValue* matches = NULL;
    if (schema->GetList("matches", &matches)) {
      URLPatternSet pattern_set;
      for (size_t i = 0; i < matches->GetSize(); ++i) {
        std::string pattern;
        CHECK(matches->GetString(i, &pattern));
        pattern_set.AddPattern(
            URLPattern(UserScript::kValidUserScriptSchemes, pattern));
      }
      url_matching_apis_[schema_namespace] = pattern_set;
    }
  }
}

ExtensionAPI::ExtensionAPI() {
  // Schemas to be loaded from resources.
  unloaded_schemas_["app"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_APP);
  unloaded_schemas_["bookmarks"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_BOOKMARKS);
  unloaded_schemas_["browserAction"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_BROWSERACTION);
  unloaded_schemas_["browsingData"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_BROWSINGDATA);
  unloaded_schemas_["chromeAuthPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_CHROMEAUTHPRIVATE);
  unloaded_schemas_["chromeosInfoPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_CHROMEOSINFOPRIVATE);
  unloaded_schemas_["contentSettings"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_CONTENTSETTINGS);
  unloaded_schemas_["contextMenus"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_CONTEXTMENUS);
  unloaded_schemas_["cookies"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_COOKIES);
  unloaded_schemas_["debugger"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_DEBUGGER);
  unloaded_schemas_["devtools"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_DEVTOOLS);
  unloaded_schemas_["experimental.accessibility"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_ACCESSIBILITY);
  unloaded_schemas_["experimental.alarms"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_ALARMS);
  unloaded_schemas_["experimental.app"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_APP);
  unloaded_schemas_["experimental.bookmarkManager"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_BOOKMARKMANAGER);
  unloaded_schemas_["experimental.declarative"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_DECLARATIVE);
  unloaded_schemas_["experimental.downloads"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_DOWNLOADS);
  unloaded_schemas_["experimental.extension"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_EXTENSION);
  unloaded_schemas_["experimental.fontSettings"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_FONTSSETTINGS);
  unloaded_schemas_["experimental.identity"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_IDENTITY);
  unloaded_schemas_["experimental.infobars"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INFOBARS);
  unloaded_schemas_["experimental.input.ui"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_UI);
  unloaded_schemas_["experimental.input.virtualKeyboard"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_VIRTUALKEYBOARD);
  unloaded_schemas_["experimental.keybinding"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_KEYBINDING);
  unloaded_schemas_["experimental.managedMode"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_MANAGEDMODE);
  unloaded_schemas_["experimental.offscreenTabs"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_OFFSCREENTABS);
  unloaded_schemas_["experimental.processes"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_PROCESSES);
  unloaded_schemas_["experimental.rlz"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_RLZ);
  unloaded_schemas_["experimental.serial"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_SERIAL);
  unloaded_schemas_["experimental.socket"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_SOCKET);
  unloaded_schemas_["experimental.speechInput"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_SPEECHINPUT);
  unloaded_schemas_["experimental.webRequest"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_WEBREQUEST);
  unloaded_schemas_["extension"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_EXTENSION);
  unloaded_schemas_["fileBrowserHandler"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER);
  unloaded_schemas_["fileBrowserPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_FILEBROWSERPRIVATE);
  unloaded_schemas_["history"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_HISTORY);
  unloaded_schemas_["i18n"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_I18N);
  unloaded_schemas_["idle"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_IDLE);
  unloaded_schemas_["input.ime"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_INPUT_IME);
  unloaded_schemas_["inputMethodPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE);
  unloaded_schemas_["management"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_MANAGEMENT);
  unloaded_schemas_["mediaPlayerPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_MEDIAPLAYERPRIVATE);
  unloaded_schemas_["metricsPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_METRICSPRIVATE);
  unloaded_schemas_["offersPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_OFFERSPRIVATE);
  unloaded_schemas_["omnibox"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_OMNIBOX);
  unloaded_schemas_["pageAction"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGEACTION);
  unloaded_schemas_["pageActions"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGEACTIONS);
  unloaded_schemas_["pageCapture"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGECAPTURE);
  unloaded_schemas_["permissions"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_PERMISSIONS);
  unloaded_schemas_["privacy"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_PRIVACY);
  unloaded_schemas_["proxy"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_PROXY);
  unloaded_schemas_["storage"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_STORAGE);
  unloaded_schemas_["systemPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_SYSTEMPRIVATE);
  unloaded_schemas_["tabs"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TABS);
  unloaded_schemas_["terminalPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TERMINALPRIVATE);
  unloaded_schemas_["test"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TEST);
  unloaded_schemas_["topSites"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TOPSITES);
  unloaded_schemas_["ttsEngine"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TTSENGINE);
  unloaded_schemas_["tts"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TTS);
  unloaded_schemas_["types"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_TYPES);
  unloaded_schemas_["webNavigation"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBNAVIGATION);
  unloaded_schemas_["webRequest"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBREQUEST);
  unloaded_schemas_["webSocketProxyPrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSOCKETPROXYPRIVATE);
  unloaded_schemas_["webstore"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSTORE);
  unloaded_schemas_["webstorePrivate"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSTOREPRIVATE);
  unloaded_schemas_["windows"] = ReadFromResource(
      IDR_EXTENSION_API_JSON_WINDOWS);

  // Schemas to be loaded via JSON generated from IDL files.
  GeneratedSchemas::Get(&unloaded_schemas_);
}

ExtensionAPI::~ExtensionAPI() {
}

bool ExtensionAPI::IsPrivileged(const std::string& full_name) {
  std::string api_name;
  std::string child_name;

  {
    std::vector<std::string> split;
    base::SplitString(full_name, '.', &split);
    std::reverse(split.begin(), split.end());

    api_name = split.back();
    split.pop_back();
    if (api_name == "experimental") {
      api_name += "." + split.back();
      split.pop_back();
    }

    // This only really works properly if split.size() == 1, however:
    //  - if it's empty, it's ok to leave child_name empty; presumably there's
    //    no functions or events with empty names.
    //  - if it's > 1, we can just do our best.
    if (split.size() > 0)
      child_name = split[0];
  }

  // GetSchema to ensure that it gets loaded before any checks.
  const DictionaryValue* schema = GetSchema(api_name);

  if (completely_unprivileged_apis_.count(api_name))
    return false;

  if (partially_unprivileged_apis_.count(api_name)) {
    return IsChildNamePrivileged(schema, "functions", child_name) &&
           IsChildNamePrivileged(schema, "events", child_name);
  }

  return true;
}

DictionaryValue* ExtensionAPI::FindListItem(
    const ListValue* list,
    const std::string& property_name,
    const std::string& property_value) {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    DictionaryValue* item = NULL;
    CHECK(list->GetDictionary(i, &item))
        << property_value << "/" << property_name;
    std::string value;
    if (item->GetString(property_name, &value) && value == property_value)
      return item;
  }

  return NULL;
}

bool ExtensionAPI::IsChildNamePrivileged(const DictionaryValue* name_space_node,
                                         const std::string& child_kind,
                                         const std::string& child_name) {
  ListValue* child_list = NULL;
  name_space_node->GetList(child_kind, &child_list);
  if (!child_list)
    return true;

  bool unprivileged = false;
  DictionaryValue* child = FindListItem(child_list, "name", child_name);
  if (!child || !child->GetBoolean("unprivileged", &unprivileged))
    return true;

  return !unprivileged;
}

const DictionaryValue* ExtensionAPI::GetSchema(const std::string& api_name) {
  SchemaMap::const_iterator maybe_schema = schemas_.find(api_name);
  if (maybe_schema != schemas_.end())
    return maybe_schema->second.get();

  // Might not have loaded yet; or might just not exist.
  std::map<std::string, base::StringPiece>::iterator maybe_schema_resource =
      unloaded_schemas_.find(api_name);
  if (maybe_schema_resource == unloaded_schemas_.end())
    return NULL;

  LoadSchema(maybe_schema_resource->second);
  maybe_schema = schemas_.find(api_name);
  CHECK(schemas_.end() != maybe_schema);
  return maybe_schema->second.get();
}

scoped_ptr<std::set<std::string> > ExtensionAPI::GetAPIsForContext(
    Feature::Context context, const Extension* extension, const GURL& url) {
  // We're forced to load all schemas now because we need to know the metadata
  // about every API -- and the metadata is stored in the schemas themselves.
  // This is a shame.
  // TODO(aa/kalman): store metadata in a separate file and don't load all
  // schemas.
  LoadAllSchemas();

  scoped_ptr<std::set<std::string> > result(new std::set<std::string>());

  switch (context) {
    case Feature::UNSPECIFIED_CONTEXT:
      break;

    case Feature::BLESSED_EXTENSION_CONTEXT:
      // Availability is determined by the permissions of the extension.
      CHECK(extension);
      GetAllowedAPIs(extension, result.get());
      ResolveDependencies(result.get());
      break;

    case Feature::UNBLESSED_EXTENSION_CONTEXT:
    case Feature::CONTENT_SCRIPT_CONTEXT:
      // Same as BLESSED_EXTENSION_CONTEXT, but only those APIs that are
      // unprivileged.
      CHECK(extension);
      GetAllowedAPIs(extension, result.get());
      // Resolving dependencies before removing unprivileged APIs means that
      // some unprivileged APIs may have unrealised dependencies. Too bad!
      ResolveDependencies(result.get());
      RemovePrivilegedAPIs(result.get());
      break;

    case Feature::WEB_PAGE_CONTEXT:
      // Availablility is determined by the url.
      CHECK(url.is_valid());
      GetAPIsMatchingURL(url, result.get());
      break;
  }

  return result.Pass();
}

void ExtensionAPI::GetAllowedAPIs(
    const Extension* extension, std::set<std::string>* out) {
  for (SchemaMap::const_iterator i = schemas_.begin(); i != schemas_.end();
      ++i) {
    if (extension->required_permission_set()->HasAnyAccessToAPI(i->first) ||
        extension->optional_permission_set()->HasAnyAccessToAPI(i->first)) {
      out->insert(i->first);
    }
  }
}

void ExtensionAPI::ResolveDependencies(std::set<std::string>* out) {
  std::set<std::string> missing_dependencies;
  for (std::set<std::string>::iterator i = out->begin(); i != out->end(); ++i)
    GetMissingDependencies(*i, *out, &missing_dependencies);

  while (missing_dependencies.size()) {
    std::string next = *missing_dependencies.begin();
    missing_dependencies.erase(next);
    out->insert(next);
    GetMissingDependencies(next, *out, &missing_dependencies);
  }
}

void ExtensionAPI::GetMissingDependencies(
    const std::string& api_name,
    const std::set<std::string>& excluding,
    std::set<std::string>* out) {
  const DictionaryValue* schema = GetSchema(api_name);
  CHECK(schema) << "Schema for " << api_name << " not found";

  ListValue* dependencies = NULL;
  if (!schema->GetList("dependencies", &dependencies))
    return;

  for (size_t i = 0; i < dependencies->GetSize(); ++i) {
    std::string api_name;
    if (dependencies->GetString(i, &api_name) && !excluding.count(api_name))
      out->insert(api_name);
  }
}

void ExtensionAPI::RemovePrivilegedAPIs(std::set<std::string>* apis) {
  std::set<std::string> privileged_apis;
  for (std::set<std::string>::iterator i = apis->begin(); i != apis->end();
      ++i) {
    if (!completely_unprivileged_apis_.count(*i) &&
        !partially_unprivileged_apis_.count(*i)) {
      privileged_apis.insert(*i);
    }
  }
  for (std::set<std::string>::iterator i = privileged_apis.begin();
      i != privileged_apis.end(); ++i) {
    apis->erase(*i);
  }
}

void ExtensionAPI::GetAPIsMatchingURL(const GURL& url,
                                      std::set<std::string>* out) {
  for (std::map<std::string, URLPatternSet>::const_iterator i =
      url_matching_apis_.begin(); i != url_matching_apis_.end(); ++i) {
    if (i->second.MatchesURL(url))
      out->insert(i->first);
  }
}

void ExtensionAPI::LoadAllSchemas() {
  while (unloaded_schemas_.size()) {
    LoadSchema(unloaded_schemas_.begin()->second);
  }
}

}  // namespace extensions
