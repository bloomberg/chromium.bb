// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_api.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/common_resources.h"
#include "grit/extensions_api_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char* kChildKinds[] = {
  "functions",
  "events"
};

base::StringPiece ReadFromResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id);
}

scoped_ptr<base::ListValue> LoadSchemaList(const std::string& name,
                                           const base::StringPiece& schema) {
  std::string error_message;
  scoped_ptr<base::Value> result(
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
  CHECK(result->IsType(base::Value::TYPE_LIST)) << " for schema " << schema;
  return scoped_ptr<base::ListValue>(static_cast<base::ListValue*>(
      result.release()));
}

const base::DictionaryValue* FindListItem(const base::ListValue* list,
                                          const std::string& property_name,
                                          const std::string& property_value) {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* item = NULL;
    CHECK(list->GetDictionary(i, &item))
        << property_value << "/" << property_name;
    std::string value;
    if (item->GetString(property_name, &value) && value == property_value)
      return item;
  }

  return NULL;
}

const base::DictionaryValue* GetSchemaChild(
    const base::DictionaryValue* schema_node,
    const std::string& child_name) {
  const base::DictionaryValue* child_node = NULL;
  for (size_t i = 0; i < arraysize(kChildKinds); ++i) {
    const base::ListValue* list_node = NULL;
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
                                   base::DictionaryValue* schema,
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
                             base::Value* value) {
  base::ListValue* list = NULL;
  base::DictionaryValue* dict = NULL;
  if (value->GetAsList(&list)) {
    for (base::ListValue::iterator i = list->begin(); i != list->end(); ++i) {
      PrefixRefsWithNamespace(schema_namespace, *i);
    }
  } else if (value->GetAsDictionary(&dict)) {
    MaybePrefixFieldWithNamespace(schema_namespace, dict, "$ref");
    for (base::DictionaryValue::Iterator i(*dict); !i.IsAtEnd(); i.Advance()) {
      base::Value* value = NULL;
      CHECK(dict->GetWithoutPathExpansion(i.key(), &value));
      PrefixRefsWithNamespace(schema_namespace, value);
    }
  }
}

// Modify all objects in the "types" section of the schema to be prefixed by
// |schema_namespace| if they do not already specify a namespace.
void PrefixTypesWithNamespace(const std::string& schema_namespace,
                              base::DictionaryValue* schema) {
  if (!schema->HasKey("types"))
    return;

  // Add the namespace to all of the types defined in this schema
  base::ListValue *types = NULL;
  CHECK(schema->GetList("types", &types));
  for (size_t i = 0; i < types->GetSize(); ++i) {
    base::DictionaryValue *type = NULL;
    CHECK(types->GetDictionary(i, &type));
    MaybePrefixFieldWithNamespace(schema_namespace, type, "id");
    MaybePrefixFieldWithNamespace(schema_namespace, type, "customBindings");
  }
}

// Modify the schema so that all types are fully qualified.
void PrefixWithNamespace(const std::string& schema_namespace,
                         base::DictionaryValue* schema) {
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
  scoped_ptr<base::ListValue> schema_list(LoadSchemaList(name, schema));
  std::string schema_namespace;
  extensions::ExtensionsClient* extensions_client =
      extensions::ExtensionsClient::Get();
  DCHECK(extensions_client);
  while (!schema_list->empty()) {
    base::DictionaryValue* schema = NULL;
    {
      scoped_ptr<base::Value> value;
      schema_list->Remove(schema_list->GetSize() - 1, &value);
      CHECK(value.release()->GetAsDictionary(&schema));
    }

    CHECK(schema->GetString("namespace", &schema_namespace));
    PrefixWithNamespace(schema_namespace, schema);
    schemas_[schema_namespace] = make_linked_ptr(schema);
    if (!extensions_client->IsAPISchemaGenerated(schema_namespace))
      CHECK_EQ(1u, unloaded_schemas_.erase(schema_namespace));
  }
}

ExtensionAPI::ExtensionAPI() : default_configuration_initialized_(false) {
}

ExtensionAPI::~ExtensionAPI() {
}

void ExtensionAPI::InitDefaultConfiguration() {
  const char* names[] = {"api", "manifest", "permission"};
  for (size_t i = 0; i < arraysize(names); ++i)
    RegisterDependencyProvider(names[i], FeatureProvider::GetByName(names[i]));

  // Schemas to be loaded from resources.
  CHECK(unloaded_schemas_.empty());
  RegisterSchemaResource("accessibilityPrivate",
                         IDR_EXTENSION_API_JSON_ACCESSIBILITYPRIVATE);
  RegisterSchemaResource("app", IDR_EXTENSION_API_JSON_APP);
  RegisterSchemaResource("browserAction", IDR_EXTENSION_API_JSON_BROWSERACTION);
  RegisterSchemaResource("commands", IDR_EXTENSION_API_JSON_COMMANDS);
  RegisterSchemaResource("declarativeContent",
      IDR_EXTENSION_API_JSON_DECLARATIVE_CONTENT);
  RegisterSchemaResource("declarativeWebRequest",
      IDR_EXTENSION_API_JSON_DECLARATIVE_WEBREQUEST);
  RegisterSchemaResource("fileBrowserHandler",
      IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER);
  RegisterSchemaResource("inputMethodPrivate",
      IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE);
  RegisterSchemaResource("pageAction", IDR_EXTENSION_API_JSON_PAGEACTION);
  RegisterSchemaResource("pageActions", IDR_EXTENSION_API_JSON_PAGEACTIONS);
  RegisterSchemaResource("privacy", IDR_EXTENSION_API_JSON_PRIVACY);
  RegisterSchemaResource("processes", IDR_EXTENSION_API_JSON_PROCESSES);
  RegisterSchemaResource("proxy", IDR_EXTENSION_API_JSON_PROXY);
  RegisterSchemaResource("scriptBadge", IDR_EXTENSION_API_JSON_SCRIPTBADGE);
  RegisterSchemaResource("ttsEngine", IDR_EXTENSION_API_JSON_TTSENGINE);
  RegisterSchemaResource("tts", IDR_EXTENSION_API_JSON_TTS);
  RegisterSchemaResource("types", IDR_EXTENSION_API_JSON_TYPES);
  RegisterSchemaResource("types.private", IDR_EXTENSION_API_JSON_TYPES_PRIVATE);
  RegisterSchemaResource("webRequestInternal",
      IDR_EXTENSION_API_JSON_WEBREQUESTINTERNAL);
  RegisterSchemaResource("webstore", IDR_EXTENSION_API_JSON_WEBSTORE);
  RegisterSchemaResource("webViewRequest",
      IDR_EXTENSION_API_JSON_WEBVIEW_REQUEST);

  default_configuration_initialized_ = true;
}

void ExtensionAPI::RegisterSchemaResource(const std::string& name,
                                          int resource_id) {
  unloaded_schemas_[name] = resource_id;
}

void ExtensionAPI::RegisterDependencyProvider(const std::string& name,
                                              FeatureProvider* provider) {
  dependency_providers_[name] = provider;
}

bool ExtensionAPI::IsAnyFeatureAvailableToContext(const Feature& api,
                                                  const Extension* extension,
                                                  Feature::Context context,
                                                  const GURL& url) {
  FeatureProviderMap::iterator provider = dependency_providers_.find("api");
  CHECK(provider != dependency_providers_.end());
  if (IsAvailable(api, extension, context, url).is_available())
    return true;

  // Check to see if there are any parts of this API that are allowed in this
  // context.
  const std::vector<Feature*> features = provider->second->GetChildren(api);
  for (std::vector<Feature*>::const_iterator feature = features.begin();
       feature != features.end();
       ++feature) {
    if (IsAvailable(**feature, extension, context, url).is_available())
      return true;
  }
  return false;
}

Feature::Availability ExtensionAPI::IsAvailable(const std::string& full_name,
                                                const Extension* extension,
                                                Feature::Context context,
                                                const GURL& url) {
  Feature* feature = GetFeatureDependency(full_name);
  CHECK(feature) << full_name;
  return IsAvailable(*feature, extension, context, url);
}

Feature::Availability ExtensionAPI::IsAvailable(const Feature& feature,
                                                const Extension* extension,
                                                Feature::Context context,
                                                const GURL& url) {
  Feature::Availability availability =
      feature.IsAvailableToContext(extension, context, url);
  if (!availability.is_available())
    return availability;

  for (std::set<std::string>::iterator iter = feature.dependencies().begin();
       iter != feature.dependencies().end(); ++iter) {
    Feature::Availability dependency_availability =
        IsAvailable(*iter, extension, context, url);
    if (!dependency_availability.is_available())
      return dependency_availability;
  }

  return Feature::CreateAvailability(Feature::IS_AVAILABLE, std::string());
}

bool ExtensionAPI::IsPrivileged(const std::string& full_name) {
  Feature* feature = GetFeatureDependency(full_name);
  CHECK(feature);
  DCHECK(!feature->GetContexts()->empty());
  // An API is 'privileged' if it can only be run in a blessed context.
  return feature->GetContexts()->size() ==
      feature->GetContexts()->count(Feature::BLESSED_EXTENSION_CONTEXT);
}

const base::DictionaryValue* ExtensionAPI::GetSchema(
    const std::string& full_name) {
  std::string child_name;
  std::string api_name = GetAPINameFromFullName(full_name, &child_name);

  const base::DictionaryValue* result = NULL;
  SchemaMap::iterator maybe_schema = schemas_.find(api_name);
  if (maybe_schema != schemas_.end()) {
    result = maybe_schema->second.get();
  } else {
    // Might not have loaded yet; or might just not exist.
    UnloadedSchemaMap::iterator maybe_schema_resource =
        unloaded_schemas_.find(api_name);
    extensions::ExtensionsClient* extensions_client =
        extensions::ExtensionsClient::Get();
    DCHECK(extensions_client);
    if (maybe_schema_resource != unloaded_schemas_.end()) {
      LoadSchema(maybe_schema_resource->first,
                 ReadFromResource(maybe_schema_resource->second));
    } else if (default_configuration_initialized_ &&
               extensions_client->IsAPISchemaGenerated(api_name)) {
      LoadSchema(api_name, extensions_client->GetAPISchema(api_name));
    } else {
      return NULL;
    }

    maybe_schema = schemas_.find(api_name);
    CHECK(schemas_.end() != maybe_schema);
    result = maybe_schema->second.get();
  }

  if (!child_name.empty())
    result = GetSchemaChild(result, child_name);

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
  extensions::ExtensionsClient* extensions_client =
      extensions::ExtensionsClient::Get();
  DCHECK(extensions_client);
  while (true) {
    if (schemas_.find(api_name_candidate) != schemas_.end() ||
        extensions_client->IsAPISchemaGenerated(api_name_candidate) ||
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
  return std::string();
}

}  // namespace extensions
