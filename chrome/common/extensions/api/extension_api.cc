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
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "chrome/common/extensions/permissions/permission_set.h"
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

const char kUnavailableMessage[] = "You do not have permission to access this "
                                   "API. Ensure that the required permission "
                                   "or manifest property is included in your "
                                   "manifest.json.";
const char* kChildKinds[] = {
  "functions",
  "events"
};

// Returns true if |dict| has an unprivileged "true" property.
bool IsUnprivileged(const DictionaryValue* dict) {
  bool unprivileged = false;
  return dict->GetBoolean("unprivileged", &unprivileged) && unprivileged;
}

// Returns whether the list at |name_space_node|.|child_kind| contains any
// children with an { "unprivileged": true } property.
bool HasUnprivilegedChild(const DictionaryValue* name_space_node,
                          const std::string& child_kind) {
  const ListValue* child_list = NULL;
  const DictionaryValue* child_dict = NULL;

  if (name_space_node->GetList(child_kind, &child_list)) {
    for (size_t i = 0; i < child_list->GetSize(); ++i) {
      const DictionaryValue* item = NULL;
      CHECK(child_list->GetDictionary(i, &item));
      if (IsUnprivileged(item))
        return true;
    }
  } else if (name_space_node->GetDictionary(child_kind, &child_dict)) {
    for (DictionaryValue::Iterator it(*child_dict); it.HasNext();
         it.Advance()) {
      const DictionaryValue* item = NULL;
      CHECK(it.value().GetAsDictionary(&item));
      if (IsUnprivileged(item))
        return true;
    }
  }

  return false;
}

base::StringPiece ReadFromResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id);
}

scoped_ptr<ListValue> LoadSchemaList(const std::string& name,
                                     const base::StringPiece& schema) {
  std::string error_message;
  scoped_ptr<Value> result(
      base::JSONReader::ReadAndReturnError(
          schema,
          base::JSON_PARSE_RFC | base::JSON_DETACHABLE_CHILDREN,  // options
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

const DictionaryValue* FindListItem(const ListValue* list,
                                    const std::string& property_name,
                                    const std::string& property_value) {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const DictionaryValue* item = NULL;
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
  const DictionaryValue* child_node = NULL;
  for (size_t i = 0; i < arraysize(kChildKinds); ++i) {
    const ListValue* list_node = NULL;
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

// If it exists and does not already specify a namespace, then the value stored
// with key |key| in |schema| will be updated to |schema_namespace| + "." +
// |schema[key]|.
void MaybePrefixFieldWithNamespace(const std::string& schema_namespace,
                                   DictionaryValue* schema,
                                   const std::string& key) {
  if (!schema->HasKey(key))
    return;

  std::string old_id;
  CHECK(schema->GetString(key, &old_id));
  if (old_id.find(".") == std::string::npos)
    schema->SetString(key, schema_namespace + "." + old_id);
}

// Modify all "$ref" keys anywhere in |schema| to be prefxied by
// |schema_namespace| if they do not already specify a namespace.
void PrefixRefsWithNamespace(const std::string& schema_namespace,
                             Value* value) {
  ListValue* list = NULL;
  DictionaryValue* dict = NULL;
  if (value->GetAsList(&list)) {
    for (ListValue::iterator i = list->begin(); i != list->end(); ++i) {
      PrefixRefsWithNamespace(schema_namespace, *i);
    }
  } else if (value->GetAsDictionary(&dict)) {
    MaybePrefixFieldWithNamespace(schema_namespace, dict, "$ref");
    for (DictionaryValue::Iterator i(*dict); !i.IsAtEnd(); i.Advance()) {
      Value* value = NULL;
      CHECK(dict->GetWithoutPathExpansion(i.key(), &value));
      PrefixRefsWithNamespace(schema_namespace, value);
    }
  }
}

// Modify all objects in the "types" section of the schema to be prefixed by
// |schema_namespace| if they do not already specify a namespace.
void PrefixTypesWithNamespace(const std::string& schema_namespace,
                              DictionaryValue* schema) {
  if (!schema->HasKey("types"))
    return;

  // Add the namespace to all of the types defined in this schema
  ListValue *types = NULL;
  CHECK(schema->GetList("types", &types));
  for (size_t i = 0; i < types->GetSize(); ++i) {
    DictionaryValue *type = NULL;
    CHECK(types->GetDictionary(i, &type));
    MaybePrefixFieldWithNamespace(schema_namespace, type, "id");
    MaybePrefixFieldWithNamespace(schema_namespace, type, "customBindings");
  }
}

// Modify the schema so that all types are fully qualified.
void PrefixWithNamespace(const std::string& schema_namespace,
                         DictionaryValue* schema) {
  PrefixTypesWithNamespace(schema_namespace, schema);
  PrefixRefsWithNamespace(schema_namespace, schema);
}

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
    DictionaryValue* schema = NULL;
    {
      Value* value = NULL;
      schema_list->Remove(schema_list->GetSize() - 1, &value);
      CHECK(value->IsType(Value::TYPE_DICTIONARY));
      schema = static_cast<DictionaryValue*>(value);
    }

    CHECK(schema->GetString("namespace", &schema_namespace));
    PrefixWithNamespace(schema_namespace, schema);
    schemas_[schema_namespace] = make_linked_ptr(schema);
    CHECK_EQ(1u, unloaded_schemas_.erase(schema_namespace));

    // Populate |{completely,partially}_unprivileged_apis_|.

    // For "partially", only need to look at functions/events; even though
    // there are unprivileged properties (e.g. in extensions), access to those
    // never reaches C++ land.
    bool unprivileged = false;
    if (schema->GetBoolean("unprivileged", &unprivileged) && unprivileged) {
      completely_unprivileged_apis_.insert(schema_namespace);
    } else if (HasUnprivilegedChild(schema, "functions") ||
               HasUnprivilegedChild(schema, "events") ||
               HasUnprivilegedChild(schema, "properties")) {
      partially_unprivileged_apis_.insert(schema_namespace);
    }

    bool uses_feature_system = false;
    schema->GetBoolean("uses_feature_system", &uses_feature_system);

    if (!uses_feature_system) {
      // Populate |url_matching_apis_|.
      ListValue* matches = NULL;
      if (schema->GetList("matches", &matches)) {
        URLPatternSet pattern_set;
        for (size_t i = 0; i < matches->GetSize(); ++i) {
          std::string pattern;
          CHECK(matches->GetString(i, &pattern));
          pattern_set.AddPattern(
              URLPattern(UserScript::ValidUserScriptSchemes(), pattern));
        }
        url_matching_apis_[schema_namespace] = pattern_set;
      }
      continue;
    }

    // Populate feature maps.
    // TODO(aa): Consider not storing features that can never run on the current
    // machine (e.g., because of platform restrictions).
    SimpleFeature* feature = new SimpleFeature();
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

        scoped_ptr<SimpleFeature> child_feature(new SimpleFeature(*feature));
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
}

ExtensionAPI::~ExtensionAPI() {
}

void ExtensionAPI::InitDefaultConfiguration() {
  RegisterDependencyProvider(
      "api", BaseFeatureProvider::GetApiFeatures());
  RegisterDependencyProvider(
      "manifest", BaseFeatureProvider::GetManifestFeatures());
  RegisterDependencyProvider(
      "permission", BaseFeatureProvider::GetPermissionFeatures());

  // Schemas to be loaded from resources.
  CHECK(unloaded_schemas_.empty());
  RegisterSchema("app", ReadFromResource(
      IDR_EXTENSION_API_JSON_APP));
  RegisterSchema("browserAction", ReadFromResource(
      IDR_EXTENSION_API_JSON_BROWSERACTION));
  RegisterSchema("browsingData", ReadFromResource(
      IDR_EXTENSION_API_JSON_BROWSINGDATA));
  RegisterSchema("commands", ReadFromResource(
      IDR_EXTENSION_API_JSON_COMMANDS));
  RegisterSchema("declarativeContent", ReadFromResource(
      IDR_EXTENSION_API_JSON_DECLARATIVE_CONTENT));
  RegisterSchema("declarativeWebRequest", ReadFromResource(
      IDR_EXTENSION_API_JSON_DECLARATIVE_WEBREQUEST));
  RegisterSchema("experimental.input.virtualKeyboard", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_VIRTUALKEYBOARD));
  RegisterSchema("experimental.processes", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_PROCESSES));
  RegisterSchema("experimental.rlz", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_RLZ));
  RegisterSchema("runtime", ReadFromResource(
      IDR_EXTENSION_API_JSON_RUNTIME));
  RegisterSchema("experimental.speechInput", ReadFromResource(
      IDR_EXTENSION_API_JSON_EXPERIMENTAL_SPEECHINPUT));
  RegisterSchema("fileBrowserHandler", ReadFromResource(
      IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER));
  RegisterSchema("fileBrowserPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_FILEBROWSERPRIVATE));
  RegisterSchema("input.ime", ReadFromResource(
      IDR_EXTENSION_API_JSON_INPUT_IME));
  RegisterSchema("inputMethodPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE));
  RegisterSchema("pageAction", ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGEACTION));
  RegisterSchema("pageActions", ReadFromResource(
      IDR_EXTENSION_API_JSON_PAGEACTIONS));
  RegisterSchema("privacy", ReadFromResource(
      IDR_EXTENSION_API_JSON_PRIVACY));
  RegisterSchema("proxy", ReadFromResource(
      IDR_EXTENSION_API_JSON_PROXY));
  RegisterSchema("scriptBadge", ReadFromResource(
      IDR_EXTENSION_API_JSON_SCRIPTBADGE));
  RegisterSchema("streamsPrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_STREAMSPRIVATE));
  RegisterSchema("ttsEngine", ReadFromResource(
      IDR_EXTENSION_API_JSON_TTSENGINE));
  RegisterSchema("tts", ReadFromResource(
      IDR_EXTENSION_API_JSON_TTS));
  RegisterSchema("types", ReadFromResource(
      IDR_EXTENSION_API_JSON_TYPES));
  RegisterSchema("webRequestInternal", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBREQUESTINTERNAL));
  RegisterSchema("webstore", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSTORE));
  RegisterSchema("webstorePrivate", ReadFromResource(
      IDR_EXTENSION_API_JSON_WEBSTOREPRIVATE));

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

Feature::Availability ExtensionAPI::IsAvailable(const std::string& full_name,
                                                const Extension* extension,
                                                Feature::Context context,
                                                const GURL& url) {
  std::string feature_type;
  std::string feature_name;
  SplitDependencyName(full_name, &feature_type, &feature_name);

  std::string child_name;
  std::string api_name = GetAPINameFromFullName(feature_name, &child_name);

  Feature* feature = GetFeatureDependency(full_name);

  // Check APIs not using the feature system first.
  if (!feature) {
    return IsNonFeatureAPIAvailable(api_name, context, extension, url) ?
        Feature::CreateAvailability(Feature::IS_AVAILABLE, "") :
        Feature::CreateAvailability(Feature::INVALID_CONTEXT,
                                    kUnavailableMessage);
  }

  Feature::Availability availability =
      feature->IsAvailableToContext(extension, context, url);
  if (!availability.is_available())
    return availability;

  for (std::set<std::string>::iterator iter = feature->dependencies().begin();
       iter != feature->dependencies().end(); ++iter) {
    Feature::Availability dependency_availability =
        IsAvailable(*iter, extension, context, url);
    if (!dependency_availability.is_available())
      return dependency_availability;
  }

  return Feature::CreateAvailability(Feature::IS_AVAILABLE, "");
}

bool ExtensionAPI::IsPrivileged(const std::string& full_name) {
  std::string child_name;
  std::string api_name = GetAPINameFromFullName(full_name, &child_name);
  Feature* feature = GetFeatureDependency(full_name);

  // First try to use the feature system.
  if (feature) {
    // An API is 'privileged' if it can only be run in a blessed context.
    return feature->GetContexts()->size() ==
        feature->GetContexts()->count(Feature::BLESSED_EXTENSION_CONTEXT);
  }

  // Get the schema now to populate |completely_unprivileged_apis_|.
  const DictionaryValue* schema = GetSchema(api_name);
  // If this API hasn't been converted yet, fall back to the old system.
  if (completely_unprivileged_apis_.count(api_name))
    return false;

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

namespace {

const char* kDisallowedPlatformAppFeatures[] = {
  // "app" refers to the the legacy app namespace for hosted apps.
  "app",
  "extension",
  "tabs",
  "windows"
};

bool IsFeatureAllowedForExtension(const std::string& feature,
                                  const extensions::Extension& extension) {
  if (extension.is_platform_app()) {
    for (size_t i = 0; i < arraysize(kDisallowedPlatformAppFeatures); ++i) {
      if (feature == kDisallowedPlatformAppFeatures[i])
        return false;
    }
  }

  return true;
}

}  // namespace

bool ExtensionAPI::IsNonFeatureAPIAvailable(const std::string& name,
                                            Feature::Context context,
                                            const Extension* extension,
                                            const GURL& url) {
  // Make sure schema is loaded.
  GetSchema(name);
  switch (context) {
    case Feature::UNSPECIFIED_CONTEXT:
      break;

    case Feature::BLESSED_EXTENSION_CONTEXT:
      if (extension) {
        // Availability is determined by the permissions of the extension.
        if (!IsAPIAllowed(name, extension))
          return false;
        if (!IsFeatureAllowedForExtension(name, *extension))
          return false;
      }
      break;

    case Feature::UNBLESSED_EXTENSION_CONTEXT:
    case Feature::CONTENT_SCRIPT_CONTEXT:
      if (extension) {
        // Same as BLESSED_EXTENSION_CONTEXT, but only those APIs that are
        // unprivileged.
        if (!IsAPIAllowed(name, extension))
          return false;
        if (!IsPrivilegedAPI(name))
          return false;
      }
      break;

    case Feature::WEB_PAGE_CONTEXT:
      if (!url_matching_apis_.count(name))
        return false;
      CHECK(url.is_valid());
      // Availablility is determined by the url.
      return url_matching_apis_[name].MatchesURL(url);
  }

  return true;
}

std::set<std::string> ExtensionAPI::GetAllAPINames() {
  std::set<std::string> result;
  for (SchemaMap::iterator i = schemas_.begin(); i != schemas_.end(); ++i)
    result.insert(i->first);
  for (UnloadedSchemaMap::iterator i = unloaded_schemas_.begin();
       i != unloaded_schemas_.end(); ++i) {
    result.insert(i->first);
  }
  return result;
}

Feature* ExtensionAPI::GetFeatureDependency(const std::string& full_name) {
  std::string feature_type;
  std::string feature_name;
  SplitDependencyName(full_name, &feature_type, &feature_name);

  FeatureProviderMap::iterator provider =
      dependency_providers_.find(feature_type);
  if (provider == dependency_providers_.end())
    return NULL;

  Feature* feature = provider->second->GetFeature(feature_name);
  // Try getting the feature for the parent API, if this was a child.
  if (!feature) {
    std::string child_name;
    feature = provider->second->GetFeature(
        GetAPINameFromFullName(feature_name, &child_name));
  }
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

bool ExtensionAPI::IsAPIAllowed(const std::string& name,
                                const Extension* extension) {
  return extension->required_permission_set()->HasAnyAccessToAPI(name) ||
      extension->optional_permission_set()->HasAnyAccessToAPI(name);
}

bool ExtensionAPI::IsPrivilegedAPI(const std::string& name) {
  return completely_unprivileged_apis_.count(name) ||
      partially_unprivileged_apis_.count(name);
}

}  // namespace extensions
