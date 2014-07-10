// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

Feature::Availability IsAvailableToManifestForBind(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Feature::Platform platform,
    const Feature* feature) {
  return feature->IsAvailableToManifest(
      extension_id, type, location, manifest_version, platform);
}

Feature::Availability IsAvailableToContextForBind(const Extension* extension,
                                                  Feature::Context context,
                                                  const GURL& url,
                                                  Feature::Platform platform,
                                                  const Feature* feature) {
  return feature->IsAvailableToContext(extension, context, url, platform);
}

struct Mappings {
  Mappings() {
    extension_types["extension"] = Manifest::TYPE_EXTENSION;
    extension_types["theme"] = Manifest::TYPE_THEME;
    extension_types["legacy_packaged_app"] = Manifest::TYPE_LEGACY_PACKAGED_APP;
    extension_types["hosted_app"] = Manifest::TYPE_HOSTED_APP;
    extension_types["platform_app"] = Manifest::TYPE_PLATFORM_APP;
    extension_types["shared_module"] = Manifest::TYPE_SHARED_MODULE;

    contexts["blessed_extension"] = Feature::BLESSED_EXTENSION_CONTEXT;
    contexts["unblessed_extension"] = Feature::UNBLESSED_EXTENSION_CONTEXT;
    contexts["content_script"] = Feature::CONTENT_SCRIPT_CONTEXT;
    contexts["web_page"] = Feature::WEB_PAGE_CONTEXT;
    contexts["blessed_web_page"] = Feature::BLESSED_WEB_PAGE_CONTEXT;

    locations["component"] = SimpleFeature::COMPONENT_LOCATION;
    locations["policy"] = SimpleFeature::POLICY_LOCATION;

    platforms["chromeos"] = Feature::CHROMEOS_PLATFORM;
    platforms["linux"] = Feature::LINUX_PLATFORM;
    platforms["mac"] = Feature::MACOSX_PLATFORM;
    platforms["win"] = Feature::WIN_PLATFORM;
  }

  std::map<std::string, Manifest::Type> extension_types;
  std::map<std::string, Feature::Context> contexts;
  std::map<std::string, SimpleFeature::Location> locations;
  std::map<std::string, Feature::Platform> platforms;
};

base::LazyInstance<Mappings> g_mappings = LAZY_INSTANCE_INITIALIZER;

// TODO(aa): Can we replace all this manual parsing with JSON schema stuff?

void ParseSet(const base::DictionaryValue* value,
              const std::string& property,
              std::set<std::string>* set) {
  const base::ListValue* list_value = NULL;
  if (!value->GetList(property, &list_value))
    return;

  set->clear();
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    std::string str_val;
    CHECK(list_value->GetString(i, &str_val)) << property << " " << i;
    set->insert(str_val);
  }
}

template<typename T>
void ParseEnum(const std::string& string_value,
               T* enum_value,
               const std::map<std::string, T>& mapping) {
  typename std::map<std::string, T>::const_iterator iter =
      mapping.find(string_value);
  if (iter == mapping.end()) {
    // For http://crbug.com/365192.
    char minidump[256];
    base::debug::Alias(&minidump);
    base::snprintf(minidump, arraysize(minidump),
        "e::simple_feature.cc:%d:\"%s\"", __LINE__, string_value.c_str());
    CHECK(false) << string_value;
  }
  *enum_value = iter->second;
}

template<typename T>
void ParseEnum(const base::DictionaryValue* value,
               const std::string& property,
               T* enum_value,
               const std::map<std::string, T>& mapping) {
  std::string string_value;
  if (!value->GetString(property, &string_value))
    return;

  ParseEnum(string_value, enum_value, mapping);
}

template<typename T>
void ParseEnumSet(const base::DictionaryValue* value,
                  const std::string& property,
                  std::set<T>* enum_set,
                  const std::map<std::string, T>& mapping) {
  if (!value->HasKey(property))
    return;

  enum_set->clear();

  std::string property_string;
  if (value->GetString(property, &property_string)) {
    if (property_string == "all") {
      for (typename std::map<std::string, T>::const_iterator j =
               mapping.begin(); j != mapping.end(); ++j) {
        enum_set->insert(j->second);
      }
    }
    return;
  }

  std::set<std::string> string_set;
  ParseSet(value, property, &string_set);
  for (std::set<std::string>::iterator iter = string_set.begin();
       iter != string_set.end(); ++iter) {
    T enum_value = static_cast<T>(0);
    ParseEnum(*iter, &enum_value, mapping);
    enum_set->insert(enum_value);
  }
}

void ParseURLPatterns(const base::DictionaryValue* value,
                      const std::string& key,
                      URLPatternSet* set) {
  const base::ListValue* matches = NULL;
  if (value->GetList(key, &matches)) {
    set->ClearPatterns();
    for (size_t i = 0; i < matches->GetSize(); ++i) {
      std::string pattern;
      CHECK(matches->GetString(i, &pattern));
      set->AddPattern(URLPattern(URLPattern::SCHEME_ALL, pattern));
    }
  }
}

// Gets a human-readable name for the given extension type, suitable for giving
// to developers in an error message.
std::string GetDisplayName(Manifest::Type type) {
  switch (type) {
    case Manifest::TYPE_UNKNOWN:
      return "unknown";
    case Manifest::TYPE_EXTENSION:
      return "extension";
    case Manifest::TYPE_HOSTED_APP:
      return "hosted app";
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      return "legacy packaged app";
    case Manifest::TYPE_PLATFORM_APP:
      return "packaged app";
    case Manifest::TYPE_THEME:
      return "theme";
    case Manifest::TYPE_USER_SCRIPT:
      return "user script";
    case Manifest::TYPE_SHARED_MODULE:
      return "shared module";
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

// Gets a human-readable name for the given context type, suitable for giving
// to developers in an error message.
std::string GetDisplayName(Feature::Context context) {
  switch (context) {
    case Feature::UNSPECIFIED_CONTEXT:
      return "unknown";
    case Feature::BLESSED_EXTENSION_CONTEXT:
      // "privileged" is vague but hopefully the developer will understand that
      // means background or app window.
      return "privileged page";
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      // "iframe" is a bit of a lie/oversimplification, but that's the most
      // common unblessed context.
      return "extension iframe";
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return "content script";
    case Feature::WEB_PAGE_CONTEXT:
      return "web page";
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return "hosted app";
  }
  NOTREACHED();
  return "";
}

// Gets a human-readable list of the display names (pluralized, comma separated
// with the "and" in the correct place) for each of |enum_types|.
template <typename EnumType>
std::string ListDisplayNames(const std::vector<EnumType> enum_types) {
  std::string display_name_list;
  for (size_t i = 0; i < enum_types.size(); ++i) {
    // Pluralize type name.
    display_name_list += GetDisplayName(enum_types[i]) + "s";
    // Comma-separate entries, with an Oxford comma if there is more than 2
    // total entries.
    if (enum_types.size() > 2) {
      if (i < enum_types.size() - 2)
        display_name_list += ", ";
      else if (i == enum_types.size() - 2)
        display_name_list += ", and ";
    } else if (enum_types.size() == 2 && i == 0) {
      display_name_list += " and ";
    }
  }
  return display_name_list;
}

std::string HashExtensionId(const std::string& extension_id) {
  const std::string id_hash = base::SHA1HashString(extension_id);
  DCHECK(id_hash.length() == base::kSHA1Length);
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

}  // namespace

SimpleFeature::SimpleFeature()
    : location_(UNSPECIFIED_LOCATION),
      min_manifest_version_(0),
      max_manifest_version_(0),
      has_parent_(false),
      component_extensions_auto_granted_(true) {}

SimpleFeature::~SimpleFeature() {}

bool SimpleFeature::HasDependencies() {
  return !dependencies_.empty();
}

void SimpleFeature::AddFilter(scoped_ptr<SimpleFeatureFilter> filter) {
  filters_.push_back(make_linked_ptr(filter.release()));
}

std::string SimpleFeature::Parse(const base::DictionaryValue* value) {
  ParseURLPatterns(value, "matches", &matches_);
  ParseSet(value, "blacklist", &blacklist_);
  ParseSet(value, "whitelist", &whitelist_);
  ParseSet(value, "dependencies", &dependencies_);
  ParseEnumSet<Manifest::Type>(value, "extension_types", &extension_types_,
                                g_mappings.Get().extension_types);
  ParseEnumSet<Context>(value, "contexts", &contexts_,
                        g_mappings.Get().contexts);
  ParseEnum<Location>(value, "location", &location_,
                      g_mappings.Get().locations);
  ParseEnumSet<Platform>(value, "platforms", &platforms_,
                         g_mappings.Get().platforms);
  value->GetInteger("min_manifest_version", &min_manifest_version_);
  value->GetInteger("max_manifest_version", &max_manifest_version_);

  no_parent_ = false;
  value->GetBoolean("noparent", &no_parent_);

  component_extensions_auto_granted_ = true;
  value->GetBoolean("component_extensions_auto_granted",
                    &component_extensions_auto_granted_);

  // NOTE: ideally we'd sanity check that "matches" can be specified if and
  // only if there's a "web_page" context, but without (Simple)Features being
  // aware of their own heirarchy this is impossible.
  //
  // For example, we might have feature "foo" available to "web_page" context
  // and "matches" google.com/*. Then a sub-feature "foo.bar" might override
  // "matches" to be chromium.org/*. That sub-feature doesn't need to specify
  // "web_page" context because it's inherited, but we don't know that here.

  for (FilterList::iterator filter_iter = filters_.begin();
       filter_iter != filters_.end();
       ++filter_iter) {
    std::string result = (*filter_iter)->Parse(value);
    if (!result.empty()) {
      return result;
    }
  }

  return std::string();
}

Feature::Availability SimpleFeature::IsAvailableToManifest(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Platform platform) const {
  // Check extension type first to avoid granting platform app permissions
  // to component extensions.
  // HACK(kalman): user script -> extension. Solve this in a more generic way
  // when we compile feature files.
  Manifest::Type type_to_check = (type == Manifest::TYPE_USER_SCRIPT) ?
      Manifest::TYPE_EXTENSION : type;
  if (!extension_types_.empty() &&
      extension_types_.find(type_to_check) == extension_types_.end()) {
    return CreateAvailability(INVALID_TYPE, type);
  }

  if (IsIdInBlacklist(extension_id))
    return CreateAvailability(FOUND_IN_BLACKLIST, type);

  // TODO(benwells): don't grant all component extensions.
  // See http://crbug.com/370375 for more details.
  // Component extensions can access any feature.
  // NOTE: Deliberately does not match EXTERNAL_COMPONENT.
  if (component_extensions_auto_granted_ && location == Manifest::COMPONENT)
    return CreateAvailability(IS_AVAILABLE, type);

  if (!whitelist_.empty()) {
    if (!IsIdInWhitelist(extension_id)) {
      // TODO(aa): This is gross. There should be a better way to test the
      // whitelist.
      CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (!command_line->HasSwitch(switches::kWhitelistedExtensionID))
        return CreateAvailability(NOT_FOUND_IN_WHITELIST, type);

      std::string whitelist_switch_value =
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kWhitelistedExtensionID);
      if (extension_id != whitelist_switch_value)
        return CreateAvailability(NOT_FOUND_IN_WHITELIST, type);
    }
  }

  if (!MatchesManifestLocation(location))
    return CreateAvailability(INVALID_LOCATION, type);

  if (!platforms_.empty() &&
      platforms_.find(platform) == platforms_.end())
    return CreateAvailability(INVALID_PLATFORM, type);

  if (min_manifest_version_ != 0 && manifest_version < min_manifest_version_)
    return CreateAvailability(INVALID_MIN_MANIFEST_VERSION, type);

  if (max_manifest_version_ != 0 && manifest_version > max_manifest_version_)
    return CreateAvailability(INVALID_MAX_MANIFEST_VERSION, type);

  for (FilterList::const_iterator filter_iter = filters_.begin();
       filter_iter != filters_.end();
       ++filter_iter) {
    Availability availability = (*filter_iter)->IsAvailableToManifest(
        extension_id, type, location, manifest_version, platform);
    if (!availability.is_available())
      return availability;
  }

  return CheckDependencies(base::Bind(&IsAvailableToManifestForBind,
                                      extension_id,
                                      type,
                                      location,
                                      manifest_version,
                                      platform));
}

Feature::Availability SimpleFeature::IsAvailableToContext(
    const Extension* extension,
    SimpleFeature::Context context,
    const GURL& url,
    SimpleFeature::Platform platform) const {
  if (extension) {
    Availability result = IsAvailableToManifest(extension->id(),
                                                extension->GetType(),
                                                extension->location(),
                                                extension->manifest_version(),
                                                platform);
    if (!result.is_available())
      return result;
  }

  if (!contexts_.empty() && contexts_.find(context) == contexts_.end())
    return CreateAvailability(INVALID_CONTEXT, context);

  if (context == WEB_PAGE_CONTEXT && !matches_.MatchesURL(url))
    return CreateAvailability(INVALID_URL, url);

  for (FilterList::const_iterator filter_iter = filters_.begin();
       filter_iter != filters_.end();
       ++filter_iter) {
    Availability availability =
        (*filter_iter)->IsAvailableToContext(extension, context, url, platform);
    if (!availability.is_available())
      return availability;
  }

  return CheckDependencies(base::Bind(
      &IsAvailableToContextForBind, extension, context, url, platform));
}

std::string SimpleFeature::GetAvailabilityMessage(
    AvailabilityResult result,
    Manifest::Type type,
    const GURL& url,
    Context context) const {
  switch (result) {
    case IS_AVAILABLE:
      return std::string();
    case NOT_FOUND_IN_WHITELIST:
    case FOUND_IN_BLACKLIST:
      return base::StringPrintf(
          "'%s' is not allowed for specified extension ID.",
          name().c_str());
    case INVALID_URL:
      return base::StringPrintf("'%s' is not allowed on %s.",
                                name().c_str(), url.spec().c_str());
    case INVALID_TYPE:
      return base::StringPrintf(
          "'%s' is only allowed for %s, but this is a %s.",
          name().c_str(),
          ListDisplayNames(std::vector<Manifest::Type>(
              extension_types_.begin(), extension_types_.end())).c_str(),
          GetDisplayName(type).c_str());
    case INVALID_CONTEXT:
      return base::StringPrintf(
          "'%s' is only allowed to run in %s, but this is a %s",
          name().c_str(),
          ListDisplayNames(std::vector<Context>(
              contexts_.begin(), contexts_.end())).c_str(),
          GetDisplayName(context).c_str());
    case INVALID_LOCATION:
      return base::StringPrintf(
          "'%s' is not allowed for specified install location.",
          name().c_str());
    case INVALID_PLATFORM:
      return base::StringPrintf(
          "'%s' is not allowed for specified platform.",
          name().c_str());
    case INVALID_MIN_MANIFEST_VERSION:
      return base::StringPrintf(
          "'%s' requires manifest version of at least %d.",
          name().c_str(),
          min_manifest_version_);
    case INVALID_MAX_MANIFEST_VERSION:
      return base::StringPrintf(
          "'%s' requires manifest version of %d or lower.",
          name().c_str(),
          max_manifest_version_);
    case NOT_PRESENT:
      return base::StringPrintf(
          "'%s' requires a different Feature that is not present.",
          name().c_str());
    case UNSUPPORTED_CHANNEL:
      return base::StringPrintf(
          "'%s' is unsupported in this version of the platform.",
          name().c_str());
  }

  NOTREACHED();
  return std::string();
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     UNSPECIFIED_CONTEXT));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result, Manifest::Type type) const {
  return Availability(result, GetAvailabilityMessage(result, type, GURL(),
                                                     UNSPECIFIED_CONTEXT));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    const GURL& url) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, url,
                                     UNSPECIFIED_CONTEXT));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    Context context) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     context));
}

bool SimpleFeature::IsInternal() const {
  return false;
}

bool SimpleFeature::IsBlockedInServiceWorker() const { return false; }

bool SimpleFeature::IsIdInBlacklist(const std::string& extension_id) const {
  return IsIdInList(extension_id, blacklist_);
}

bool SimpleFeature::IsIdInWhitelist(const std::string& extension_id) const {
  return IsIdInList(extension_id, whitelist_);
}

// static
bool SimpleFeature::IsIdInList(const std::string& extension_id,
                               const std::set<std::string>& list) {
  // Belt-and-suspenders philosophy here. We should be pretty confident by this
  // point that we've validated the extension ID format, but in case something
  // slips through, we avoid a class of attack where creative ID manipulation
  // leads to hash collisions.
  if (extension_id.length() != 32)  // 128 bits / 4 = 32 mpdecimal characters
    return false;

  if (list.find(extension_id) != list.end() ||
      list.find(HashExtensionId(extension_id)) != list.end()) {
    return true;
  }

  return false;
}

bool SimpleFeature::MatchesManifestLocation(
    Manifest::Location manifest_location) const {
  switch (location_) {
    case SimpleFeature::UNSPECIFIED_LOCATION:
      return true;
    case SimpleFeature::COMPONENT_LOCATION:
      // TODO(kalman/asargent): Should this include EXTERNAL_COMPONENT too?
      return manifest_location == Manifest::COMPONENT;
    case SimpleFeature::POLICY_LOCATION:
      return manifest_location == Manifest::EXTERNAL_POLICY ||
             manifest_location == Manifest::EXTERNAL_POLICY_DOWNLOAD;
  }
  NOTREACHED();
  return false;
}

Feature::Availability SimpleFeature::CheckDependencies(
    const base::Callback<Availability(const Feature*)>& checker) const {
  for (std::set<std::string>::const_iterator it = dependencies_.begin();
       it != dependencies_.end();
       ++it) {
    Feature* dependency =
        ExtensionAPI::GetSharedInstance()->GetFeatureDependency(*it);
    if (!dependency)
      return CreateAvailability(NOT_PRESENT);
    Availability dependency_availability = checker.Run(dependency);
    if (!dependency_availability.is_available())
      return dependency_availability;
  }
  return CreateAvailability(IS_AVAILABLE);
}

}  // namespace extensions
