// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/extensions/simple_feature_provider.h"
#include "googleurl/src/gurl.h"
#include "grit/common_resources.h"
#include "grit/extensions_api_resources.h"
#include "ui/base/resource/resource_bundle.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace extensions {

using api::GeneratedSchemas;

namespace {

const char* kChildKinds[] = {
  "functions",
  "events"
};

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

scoped_ptr<ListValue> LoadSchemaList(const std::string& name,
                                     const base::StringPiece& schema) {
  std::string error_message;
  scoped_ptr<Value> result(
      base::JSONReader::ReadAndReturnError(
          schema.as_string(),
          base::JSON_PARSE_RFC,  // options
          NULL,  // error code
          &error_message));

  // Tracking down http://crbug.com/121424
  char buf[128];
  base::snprintf(buf, arraysize(buf), "%s: (%d) '%s'",
      name.c_str(),
      result.get() ? result->GetType() : -1,
      error_message.c_str());

  CHECK(result.get()) << error_message << " for schema " << schema;
  CHECK(result->IsType(Value::TYPE_LIST)) << " for schema " << schema;
  return scoped_ptr<ListValue>(static_cast<ListValue*>(result.release()));
}

DictionaryValue* FindListItem(const ListValue* list,
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

const DictionaryValue* GetSchemaChild(const DictionaryValue* schema_node,
                                      const std::string& child_name) {
  DictionaryValue* child_node = NULL;
  for (size_t i = 0; i < arraysize(kChildKinds); ++i) {
    ListValue* list_node = NULL;
    if (!schema_node->GetList(kChildKinds[i], &list_node))
      continue;
    child_node = FindListItem(list_node, "name", child_name);
    if (child_node)
      return child_node;
  }

  return NULL;
}

struct Static {
  Static()
      : api(ExtensionAPI::CreateWithDefaultConfiguration()) {
  }
  scoped_ptr<ExtensionAPI> api;
};

base::LazyInstance<Static> g_lazy_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ExtensionAPI* ExtensionAPI::GetSharedInstance() {
  return g_lazy_instance.Get().api.get();
}

// static
ExtensionAPI* ExtensionAPI::CreateWithDefaultConfiguration() {
  ExtensionAPI* api = new ExtensionAPI();
  api->InitDefaultConfiguration();
  return api;
}

// static
void ExtensionAPI::SplitDependencyName(const std::string& full_name,
                                       std::string* feature_type,
                                       std::string* feature_name) {
  size_t colon_index = full_name.find(':');
  if (colon_index == std::string::npos) {
    // TODO(aa): Remove this code when all API descriptions have been updated.
    *feature_type = "api";
    *feature_name = full_name;
    return;
  }

  *feature_type = full_name.substr(0, colon_index);
  *feature_name = full_name.substr(colon_index + 1);
}

void ExtensionAPI::LoadSchema(const std::string& name,
                              const base::StringPiece& schema) {
  scoped_ptr<ListValue> schema_list(LoadSchemaList(name, schema));
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
    CHECK_EQ(1u, unloaded_schemas_.erase(schema_namespace));

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

    // Populate feature maps.
    // TODO(aa): Consider not storing features that can never run on the current
    // machine (e.g., because of platform restrictions).
    bool uses_feature_system = false;
    schema->GetBoolean("uses_feature_system", &uses_feature_system);
    if (!uses_feature_system)
      continue;

    Feature* feature = new Feature();
    feature->set_name(schema_namespace);
    feature->Parse(schema);

    FeatureMap* schema_features = new FeatureMap();
    CHECK(features_.insert(
        std::make_pair(schema_namespace,
                       make_linked_ptr(schema_features))).second);
    CHECK(schema_features->insert(
        std::make_pair("", make_linked_ptr(feature))).second);

    for (size_t i = 0; i < arraysize(kChildKinds); ++i) {
      ListValue* child_list = NULL;
      schema->GetList(kChildKinds[i], &child_list);
      if (!child_list)
        continue;

      for (size_t j = 0; j < child_list->GetSize(); ++j) {
        DictionaryValue* child = NULL;
        CHECK(child_list->GetDictionary(j, &child));

        scoped_ptr<Feature> child_feature(new Feature(*feature));
        child_feature->Parse(child);
        if (child_feature->Equals(*feature))
          continue;  // no need to store no-op features

        std::string child_name;
        CHECK(child->GetString("name", &child_name));
        child_feature->set_name(schema_namespace + "." + child_name);
        CHECK(schema_features->insert(
            std::make_pair(child_name,
                           make_linked_ptr(child_feature.release()))).second);
      }
    }
  }
}

ExtensionAPI::ExtensionAPI() {
  RegisterDependencyProvider("api", this);

  // TODO(aa): Can remove this when all JSON files are converted.
  RegisterDependencyProvider("", this);
}

ExtensionAPI::~ExtensionAPI() {
}

void ExtensionAPI::InitDefaultConfiguration() {
  RegisterDependencyProvider(
      "manifest", SimpleFeatureProvider::GetManifestFeatures());
  RegisterDependencyProvider(
      "permission", SimpleFeatureProvider::GetPermissionFeatures());

  // Schemas to be loaded from resources.
  CHECK(unloaded_schemas_.empty());
  RegisterSchema("app", ReadFromResource(
      IDR_EXTENSION_API_JSON_APP));
  RegisterSchema("bookmarks", ReadFromResource(
      IDR_EXTENSION_API_JSON_BOOKMARKS));
  RegisterSchema("browserAction", ReadFromResource(
      IDR_EXTENSION_API_JSON_BROWSERACTION));
  RegisterSchema("browsingData", ReadFromResource(
      IDR_EXTENSION_API_JSON_BROWSINGDATA));
  RegisterSchema("chromeAuthPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_CHROMEAUTHPRIVATE));
  RegisterSchema("chromeosInfoPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_CHROMEOSINFOPRIVATE));
  RegisterSchema("contentSettings", ReadFromResource(
      IDR_EXTENSION_API_JSON_CONTENTSETTINGS));
  RegisterSchema("contextMenus", ReadFromResource(
      IDR_EXTENSION_API_JSON_CONTEXTMENUS));
  RegisterSchema("cookies", ReadFromResource(
      IDR_EXTENSION_API_JSON_COOKIES));
  RegisterSchema("debugger", ReadFromResource(
      IDR_EXTENSION_API_JSON_DEBUGGER));
  RegisterSchema("devtools", ReadFromResource(
      IDR_EXTENSION_API_JSON_DEVTOOLS));
  RegisterSchema("experimental.accessibility", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_ACCESSIBILITY));
  RegisterSchema("experimental.app", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_APP));
  RegisterSchema("experimental.bookmarkManager", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_BOOKMARKMANAGER));
  RegisterSchema("experimental.declarative", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_DECLARATIVE));
  RegisterSchema("experimental.downloads", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_DOWNLOADS));
  RegisterSchema("experimental.fontSettings", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_FONTSSETTINGS));
  RegisterSchema("experimental.identity", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_IDENTITY));
  RegisterSchema("experimental.infobars", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INFOBARS));
  RegisterSchema("experimental.input.ui", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_UI));
  RegisterSchema("experimental.input.virtualKeyboard", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_VIRTUALKEYBOARD));
  RegisterSchema("experimental.keybinding", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_KEYBINDING));
  RegisterSchema("experimental.offscreenTabs", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_OFFSCREENTABS));
  RegisterSchema("experimental.processes", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_PROCESSES));
  RegisterSchema("experimental.record", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_RECORD));
  RegisterSchema("experimental.rlz", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_RLZ));
  RegisterSchema("experimental.runtime", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_RUNTIME));
  RegisterSchema("experimental.speechInput", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_SPEECHINPUT));
  RegisterSchema("experimental.webRequest", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_WEBREQUEST));
  RegisterSchema("extension", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXTENSION));
  RegisterSchema("fileBrowserHandler", ReadFromResource(
      IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER));
  RegisterSchema("fileBrowserPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_FILEBROWSERPRIVATE));
  RegisterSchema("history", ReadFromResource(
      IDR_EXTENSION_API_JSON_HISTORY));
  RegisterSchema("i18n", ReadFromResource(
      IDR_EXTENSION_API_JSON_I18N));
  RegisterSchema("idle", ReadFromResource(
      IDR_EXTENSION_API_JSON_IDLE));
  RegisterSchema("experimental.input.ime", ReadFromResource(
      IDR_EXTENSION_API_JSON_INPUT_IME));
  RegisterSchema("inputMethodPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE));
  RegisterSchema("managedModePrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_MANAGEDMODEPRIVATE));
  RegisterSchema("management", ReadFromResource(
      IDR_EXTENSION_API_JSON_MANAGEMENT));
  RegisterSchema("mediaPlayerPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_MEDIAPLAYERPRIVATE));
  RegisterSchema("metricsPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_METRICSPRIVATE));
  RegisterSchema("echoPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_ECHOPRIVATE));
  RegisterSchema("omnibox", ReadFromResource(
      IDR_EXTENSION_API_JSON_OMNIBOX));
  RegisterSchema("pageAction", ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGEACTION));
  RegisterSchema("pageActions", ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGEACTIONS));
  RegisterSchema("pageCapture", ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGECAPTURE));
  RegisterSchema("permissions", ReadFromResource(
      IDR_EXTENSION_API_JSON_PERMISSIONS));
  RegisterSchema("privacy", ReadFromResource(
      IDR_EXTENSION_API_JSON_PRIVACY));
  RegisterSchema("proxy", ReadFromResource(
      IDR_EXTENSION_API_JSON_PROXY));
  RegisterSchema("storage", ReadFromResource(
      IDR_EXTENSION_API_JSON_STORAGE));
  RegisterSchema("systemPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_SYSTEMPRIVATE));
  RegisterSchema("tabs", ReadFromResource(
      IDR_EXTENSION_API_JSON_TABS));
  RegisterSchema("terminalPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_TERMINALPRIVATE));
  RegisterSchema("test", ReadFromResource(
      IDR_EXTENSION_API_JSON_TEST));
  RegisterSchema("topSites", ReadFromResource(
      IDR_EXTENSION_API_JSON_TOPSITES));
  RegisterSchema("ttsEngine", ReadFromResource(
      IDR_EXTENSION_API_JSON_TTSENGINE));
  RegisterSchema("tts", ReadFromResource(
      IDR_EXTENSION_API_JSON_TTS));
  RegisterSchema("types", ReadFromResource(
      IDR_EXTENSION_API_JSON_TYPES));
  RegisterSchema("webNavigation", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBNAVIGATION));
  RegisterSchema("webRequest", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBREQUEST));
  RegisterSchema("webSocketProxyPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSOCKETPROXYPRIVATE));
  RegisterSchema("webstore", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSTORE));
  RegisterSchema("webstorePrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSTOREPRIVATE));
  RegisterSchema("windows", ReadFromResource(
      IDR_EXTENSION_API_JSON_WINDOWS));

  // Schemas to be loaded via JSON generated from IDL files.
  GeneratedSchemas::Get(&unloaded_schemas_);
}

void ExtensionAPI::RegisterSchema(const std::string& name,
                                  const base::StringPiece& source) {
  unloaded_schemas_[name] = source;
}

void ExtensionAPI::RegisterDependencyProvider(const std::string& name,
                                              FeatureProvider* provider) {
  dependency_providers_[name] = provider;
}

bool ExtensionAPI::IsAvailable(const std::string& full_name,
                               const Extension* extension,
                               Feature::Context context) {
  std::set<std::string> dependency_names;
  dependency_names.insert(full_name);
  ResolveDependencies(&dependency_names);

  for (std::set<std::string>::iterator iter = dependency_names.begin();
       iter != dependency_names.end(); ++iter) {
    Feature* feature = GetFeatureDependency(full_name);
    CHECK(feature) << *iter;

    Feature::Availability availability =
        feature->IsAvailableToContext(extension, context);
    if (availability != Feature::IS_AVAILABLE)
      return false;
  }

  return true;
}

bool ExtensionAPI::IsPrivileged(const std::string& full_name) {
  std::string child_name;
  std::string api_name = GetAPINameFromFullName(full_name, &child_name);

  // First try to use the feature system.
  Feature* feature(GetFeature(full_name));
  if (feature) {
    // An API is 'privileged' if it or any of its dependencies can only be run
    // in a blessed context.
    std::set<std::string> resolved_dependencies;
    resolved_dependencies.insert(full_name);
    ResolveDependencies(&resolved_dependencies);
    for (std::set<std::string>::iterator iter = resolved_dependencies.begin();
         iter != resolved_dependencies.end(); ++iter) {
      Feature* dependency = GetFeatureDependency(*iter);
      for (std::set<Feature::Context>::iterator context =
               dependency->contexts()->begin();
           context != dependency->contexts()->end(); ++context) {
        if (*context != Feature::BLESSED_EXTENSION_CONTEXT)
          return false;
      }
    }
    return true;
  }

  // If this API hasn't been converted yet, fall back to the old system.
  if (completely_unprivileged_apis_.count(api_name))
    return false;

  const DictionaryValue* schema = GetSchema(api_name);
  if (partially_unprivileged_apis_.count(api_name))
    return IsChildNamePrivileged(schema, child_name);

  return true;
}

bool ExtensionAPI::IsChildNamePrivileged(const DictionaryValue* name_space_node,
                                         const std::string& child_name) {
  bool unprivileged = false;
  const DictionaryValue* child = GetSchemaChild(name_space_node, child_name);
  if (!child || !child->GetBoolean("unprivileged", &unprivileged))
    return true;

  return !unprivileged;
}

const DictionaryValue* ExtensionAPI::GetSchema(const std::string& full_name) {
  std::string child_name;
  std::string api_name = GetAPINameFromFullName(full_name, &child_name);

  const DictionaryValue* result = NULL;
  SchemaMap::iterator maybe_schema = schemas_.find(api_name);
  if (maybe_schema != schemas_.end()) {
    result = maybe_schema->second.get();
  } else {
    // Might not have loaded yet; or might just not exist.
    std::map<std::string, base::StringPiece>::iterator maybe_schema_resource =
        unloaded_schemas_.find(api_name);
    if (maybe_schema_resource == unloaded_schemas_.end())
      return NULL;

    LoadSchema(maybe_schema_resource->first, maybe_schema_resource->second);
    maybe_schema = schemas_.find(api_name);
    CHECK(schemas_.end() != maybe_schema);
    result = maybe_schema->second.get();
  }

  if (!child_name.empty())
    result = GetSchemaChild(result, child_name);

  return result;
}

scoped_ptr<std::set<std::string> > ExtensionAPI::GetAPIsForContext(
    Feature::Context context, const Extension* extension, const GURL& url) {
  // We're forced to load all schemas now because we need to know the metadata
  // about every API -- and the metadata is stored in the schemas themselves.
  // This is a shame.
  // TODO(aa/kalman): store metadata in a separate file and don't load all
  // schemas.
  LoadAllSchemas();

  std::set<std::string> temp_result;

  // First handle all the APIs that have been converted to the feature system.
  if (extension) {
    for (APIFeatureMap::iterator iter = features_.begin();
         iter != features_.end(); ++iter) {
      if (IsAvailable(iter->first, extension, context))
        temp_result.insert(iter->first);
    }
  }

  // Second, fall back to the old way.
  // TODO(aa): Remove this when all APIs have been converted.
  switch (context) {
    case Feature::UNSPECIFIED_CONTEXT:
      break;

    case Feature::BLESSED_EXTENSION_CONTEXT:
      if (extension) {
        // Availability is determined by the permissions of the extension.
        GetAllowedAPIs(extension, &temp_result);
        ResolveDependencies(&temp_result);
      }
      break;

    case Feature::UNBLESSED_EXTENSION_CONTEXT:
    case Feature::CONTENT_SCRIPT_CONTEXT:
      if (extension) {
        // Same as BLESSED_EXTENSION_CONTEXT, but only those APIs that are
        // unprivileged.
        GetAllowedAPIs(extension, &temp_result);
        // Resolving dependencies before removing unprivileged APIs means that
        // some unprivileged APIs may have unrealised dependencies. Too bad!
        ResolveDependencies(&temp_result);
        RemovePrivilegedAPIs(&temp_result);
      }
      break;

    case Feature::WEB_PAGE_CONTEXT:
      if (url.is_valid()) {
        // Availablility is determined by the url.
        GetAPIsMatchingURL(url, &temp_result);
      }
      break;
  }

  // Filter out all non-API features and remove the feature type part of the
  // name.
  scoped_ptr<std::set<std::string> > result(new std::set<std::string>());
  for (std::set<std::string>::iterator iter = temp_result.begin();
       iter != temp_result.end(); ++iter) {
    std::string feature_type;
    std::string feature_name;
    SplitDependencyName(*iter, &feature_type, &feature_name);
    if (feature_type == "api")
      result->insert(feature_name);
  }

  return result.Pass();
}

Feature* ExtensionAPI::GetFeature(const std::string& full_name) {
  // Ensure it's loaded.
  GetSchema(full_name);

  std::string child_name;
  std::string api_namespace = GetAPINameFromFullName(full_name, &child_name);

  APIFeatureMap::iterator feature_map = features_.find(api_namespace);
  if (feature_map == features_.end())
    return NULL;

  Feature* result = NULL;
  FeatureMap::iterator child_feature = feature_map->second->find(child_name);
  if (child_feature != feature_map->second->end()) {
    result = child_feature->second.get();
  } else {
    FeatureMap::iterator parent_feature = feature_map->second->find("");
    CHECK(parent_feature != feature_map->second->end());
    result = parent_feature->second.get();
  }

  if (result->contexts()->empty()) {
    LOG(ERROR) << "API feature '" << full_name
               << "' must specify at least one context.";
    return NULL;
  }

  return result;
}

Feature* ExtensionAPI::GetFeatureDependency(const std::string& full_name) {
  std::string feature_type;
  std::string feature_name;
  SplitDependencyName(full_name, &feature_type, &feature_name);

  FeatureProviderMap::iterator provider =
      dependency_providers_.find(feature_type);
  CHECK(provider != dependency_providers_.end()) << full_name;

  Feature* feature = provider->second->GetFeature(feature_name);
  CHECK(feature) << full_name;

  return feature;
}

std::string ExtensionAPI::GetAPINameFromFullName(const std::string& full_name,
                                                 std::string* child_name) {
  std::string api_name_candidate = full_name;
  while (true) {
    if (features_.find(api_name_candidate) != features_.end() ||
        schemas_.find(api_name_candidate) != schemas_.end() ||
        unloaded_schemas_.find(api_name_candidate) != unloaded_schemas_.end()) {
      std::string result = api_name_candidate;

      if (child_name) {
        if (result.length() < full_name.length())
          *child_name = full_name.substr(result.length() + 1);
        else
          *child_name = "";
      }

      return result;
    }

    size_t last_dot_index = api_name_candidate.rfind('.');
    if (last_dot_index == std::string::npos)
      break;

    api_name_candidate = api_name_candidate.substr(0, last_dot_index);
  }

  *child_name = "";
  return "";
}

void ExtensionAPI::GetAllowedAPIs(
    const Extension* extension, std::set<std::string>* out) {
  for (SchemaMap::const_iterator i = schemas_.begin(); i != schemas_.end();
      ++i) {
    if (features_.find(i->first) != features_.end()) {
      // This API is controlled by the feature system. Nothing to do here.
      continue;
    }

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
  std::string feature_type;
  std::string feature_name;
  SplitDependencyName(api_name, &feature_type, &feature_name);

  // Only API features can have dependencies for now.
  if (feature_type != "api")
    return;

  const DictionaryValue* schema = GetSchema(feature_name);
  CHECK(schema) << "Schema for " << feature_name << " not found";

  ListValue* dependencies = NULL;
  if (!schema->GetList("dependencies", &dependencies))
    return;

  for (size_t i = 0; i < dependencies->GetSize(); ++i) {
    std::string dependency_name;
    if (dependencies->GetString(i, &dependency_name) &&
        !excluding.count(dependency_name)) {
      out->insert(dependency_name);
    }
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
    if (features_.find(i->first) != features_.end()) {
      // This API is controlled by the feature system. Nothing to do.
      continue;
    }

    if (i->second.MatchesURL(url))
      out->insert(i->first);
  }
}

void ExtensionAPI::LoadAllSchemas() {
  while (unloaded_schemas_.size()) {
    std::map<std::string, base::StringPiece>::iterator it =
        unloaded_schemas_.begin();
    LoadSchema(it->first, it->second);
  }
}

}  // namespace extensions
