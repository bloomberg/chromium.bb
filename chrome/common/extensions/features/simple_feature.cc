// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/simple_feature.h"

#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"

using chrome::VersionInfo;

namespace extensions {

namespace {

struct Mappings {
  Mappings() {
    extension_types["extension"] = Manifest::TYPE_EXTENSION;
    extension_types["theme"] = Manifest::TYPE_THEME;
    extension_types["packaged_app"] = Manifest::TYPE_LEGACY_PACKAGED_APP;
    extension_types["hosted_app"] = Manifest::TYPE_HOSTED_APP;
    extension_types["platform_app"] = Manifest::TYPE_PLATFORM_APP;

    contexts["blessed_extension"] = Feature::BLESSED_EXTENSION_CONTEXT;
    contexts["unblessed_extension"] = Feature::UNBLESSED_EXTENSION_CONTEXT;
    contexts["content_script"] = Feature::CONTENT_SCRIPT_CONTEXT;
    contexts["web_page"] = Feature::WEB_PAGE_CONTEXT;

    locations["component"] = Feature::COMPONENT_LOCATION;

    platforms["chromeos"] = Feature::CHROMEOS_PLATFORM;

    channels["trunk"] = VersionInfo::CHANNEL_UNKNOWN;
    channels["canary"] = VersionInfo::CHANNEL_CANARY;
    channels["dev"] = VersionInfo::CHANNEL_DEV;
    channels["beta"] = VersionInfo::CHANNEL_BETA;
    channels["stable"] = VersionInfo::CHANNEL_STABLE;
  }

  std::map<std::string, Manifest::Type> extension_types;
  std::map<std::string, Feature::Context> contexts;
  std::map<std::string, Feature::Location> locations;
  std::map<std::string, Feature::Platform> platforms;
  std::map<std::string, VersionInfo::Channel> channels;
};

base::LazyInstance<Mappings> g_mappings = LAZY_INSTANCE_INITIALIZER;

std::string GetChannelName(VersionInfo::Channel channel) {
  typedef std::map<std::string, VersionInfo::Channel> ChannelsMap;
  ChannelsMap channels = g_mappings.Get().channels;
  for (ChannelsMap::iterator i = channels.begin(); i != channels.end(); ++i) {
    if (i->second == channel)
      return i->first;
  }
  NOTREACHED();
  return "unknown";
}

// TODO(aa): Can we replace all this manual parsing with JSON schema stuff?

void ParseSet(const DictionaryValue* value,
              const std::string& property,
              std::set<std::string>* set) {
  const ListValue* list_value = NULL;
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
  CHECK(iter != mapping.end()) << string_value;
  *enum_value = iter->second;
}

template<typename T>
void ParseEnum(const DictionaryValue* value,
               const std::string& property,
               T* enum_value,
               const std::map<std::string, T>& mapping) {
  std::string string_value;
  if (!value->GetString(property, &string_value))
    return;

  ParseEnum(string_value, enum_value, mapping);
}

template<typename T>
void ParseEnumSet(const DictionaryValue* value,
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

void ParseURLPatterns(const DictionaryValue* value,
                      const std::string& key,
                      URLPatternSet* set) {
  const ListValue* matches = NULL;
  if (value->GetList(key, &matches)) {
    for (size_t i = 0; i < matches->GetSize(); ++i) {
      std::string pattern;
      CHECK(matches->GetString(i, &pattern));
      set->AddPattern(URLPattern(URLPattern::SCHEME_ALL, pattern));
    }
  }
}

// Gets a human-readable name for the given extension type.
std::string GetDisplayTypeName(Manifest::Type type) {
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
  }

  NOTREACHED();
  return "";
}

}  // namespace

SimpleFeature::SimpleFeature()
  : location_(UNSPECIFIED_LOCATION),
    platform_(UNSPECIFIED_PLATFORM),
    min_manifest_version_(0),
    max_manifest_version_(0),
    channel_(VersionInfo::CHANNEL_UNKNOWN) {
}

SimpleFeature::SimpleFeature(const SimpleFeature& other)
    : whitelist_(other.whitelist_),
      extension_types_(other.extension_types_),
      contexts_(other.contexts_),
      matches_(other.matches_),
      location_(other.location_),
      platform_(other.platform_),
      min_manifest_version_(other.min_manifest_version_),
      max_manifest_version_(other.max_manifest_version_),
      channel_(other.channel_) {
}

SimpleFeature::~SimpleFeature() {
}

bool SimpleFeature::Equals(const SimpleFeature& other) const {
  return whitelist_ == other.whitelist_ &&
      extension_types_ == other.extension_types_ &&
      contexts_ == other.contexts_ &&
      matches_ == other.matches_ &&
      location_ == other.location_ &&
      platform_ == other.platform_ &&
      min_manifest_version_ == other.min_manifest_version_ &&
      max_manifest_version_ == other.max_manifest_version_ &&
      channel_ == other.channel_;
}

std::string SimpleFeature::Parse(const DictionaryValue* value) {
  ParseURLPatterns(value, "matches", &matches_);
  ParseSet(value, "whitelist", &whitelist_);
  ParseSet(value, "dependencies", &dependencies_);
  ParseEnumSet<Manifest::Type>(value, "extension_types", &extension_types_,
                                g_mappings.Get().extension_types);
  ParseEnumSet<Context>(value, "contexts", &contexts_,
                        g_mappings.Get().contexts);
  ParseEnum<Location>(value, "location", &location_,
                      g_mappings.Get().locations);
  ParseEnum<Platform>(value, "platform", &platform_,
                      g_mappings.Get().platforms);
  value->GetInteger("min_manifest_version", &min_manifest_version_);
  value->GetInteger("max_manifest_version", &max_manifest_version_);
  ParseEnum<VersionInfo::Channel>(
      value, "channel", &channel_,
      g_mappings.Get().channels);
  if (matches_.is_empty() && contexts_.count(WEB_PAGE_CONTEXT) != 0) {
    return name() + ": Allowing web_page contexts requires supplying a value " +
        "for matches.";
  }
  return "";
}

Feature::Availability SimpleFeature::IsAvailableToManifest(
    const std::string& extension_id,
    Manifest::Type type,
    Location location,
    int manifest_version,
    Platform platform) const {
  // Component extensions can access any feature.
  if (location == COMPONENT_LOCATION)
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

  if (!extension_types_.empty() &&
      extension_types_.find(type) == extension_types_.end()) {
    return CreateAvailability(INVALID_TYPE, type);
  }

  if (location_ != UNSPECIFIED_LOCATION && location_ != location)
    return CreateAvailability(INVALID_LOCATION, type);

  if (platform_ != UNSPECIFIED_PLATFORM && platform_ != platform)
    return CreateAvailability(INVALID_PLATFORM, type);

  if (min_manifest_version_ != 0 && manifest_version < min_manifest_version_)
    return CreateAvailability(INVALID_MIN_MANIFEST_VERSION, type);

  if (max_manifest_version_ != 0 && manifest_version > max_manifest_version_)
    return CreateAvailability(INVALID_MAX_MANIFEST_VERSION, type);

  if (channel_ <  Feature::GetCurrentChannel())
    return CreateAvailability(UNSUPPORTED_CHANNEL, type);

  return CreateAvailability(IS_AVAILABLE, type);
}

Feature::Availability SimpleFeature::IsAvailableToContext(
    const Extension* extension,
    SimpleFeature::Context context,
    const GURL& url,
    SimpleFeature::Platform platform) const {
  if (extension) {
    Availability result = IsAvailableToManifest(
        extension->id(),
        extension->GetType(),
        ConvertLocation(extension->location()),
        extension->manifest_version(),
        platform);
    if (!result.is_available())
      return result;
  }

  if (!contexts_.empty() && contexts_.find(context) == contexts_.end()) {
    return extension ?
        CreateAvailability(INVALID_CONTEXT, extension->GetType()) :
        CreateAvailability(INVALID_CONTEXT);
  }

  if (!matches_.is_empty() && !matches_.MatchesURL(url))
    return CreateAvailability(INVALID_URL, url);

  return CreateAvailability(IS_AVAILABLE);
}

std::string SimpleFeature::GetAvailabilityMessage(
    AvailabilityResult result, Manifest::Type type, const GURL& url) const {
  switch (result) {
    case IS_AVAILABLE:
      return "";
    case NOT_FOUND_IN_WHITELIST:
      return base::StringPrintf(
          "'%s' is not allowed for specified extension ID.",
          name().c_str());
    case INVALID_URL:
      CHECK(url.is_valid());
      return base::StringPrintf("'%s' is not allowed on %s.",
                                name().c_str(), url.spec().c_str());
    case INVALID_TYPE: {
      std::string allowed_type_names;
      // Turn the set of allowed types into a vector so that it's easier to
      // inject the appropriate separator into the display string.
      std::vector<Manifest::Type> extension_types(
          extension_types_.begin(), extension_types_.end());
      for (size_t i = 0; i < extension_types.size(); i++) {
        // Pluralize type name.
        allowed_type_names += GetDisplayTypeName(extension_types[i]) + "s";
        if (i == extension_types_.size() - 2) {
          allowed_type_names += " and ";
        } else if (i != extension_types_.size() - 1) {
          allowed_type_names += ", ";
        }
      }

      return base::StringPrintf(
          "'%s' is only allowed for %s, and this is a %s.",
          name().c_str(),
          allowed_type_names.c_str(),
          GetDisplayTypeName(type).c_str());
    }
    case INVALID_CONTEXT:
      return base::StringPrintf(
          "'%s' is not allowed for specified context type content script, "
          " extension page, web page, etc.).",
          name().c_str());
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
          "'%s' requires Google Chrome %s channel or newer, and this is the "
              "%s channel.",
          name().c_str(),
          GetChannelName(channel_).c_str(),
          GetChannelName(GetCurrentChannel()).c_str());
  }

  NOTREACHED();
  return "";
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL()));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result, Manifest::Type type) const {
  return Availability(result, GetAvailabilityMessage(result, type, GURL()));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    const GURL& url) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, url));
}

std::set<Feature::Context>* SimpleFeature::GetContexts() {
  return &contexts_;
}

bool SimpleFeature::IsIdInWhitelist(const std::string& extension_id) const {
  // An empty whitelist means the absence of a whitelist, rather than a
  // whitelist that allows no ID through. This could be surprising behavior, so
  // we force the caller to handle it explicitly. A DCHECK should be sufficient
  // here because all whitelists today are hardcoded, meaning that errors
  // should be caught during development, but if that assumption changes in the
  // future (e.g., a whitelist that lives in the user's profile and is managed
  // by runtime behavior), better to die via CHECK than to let an edge case
  // open up access to a whitelisted API with a whitelist that has shrunk to
  // size zero.
  CHECK(!whitelist_.empty());

  // Belt-and-suspenders philosophy here. We should be pretty confident by this
  // point that we've validated the extension ID format, but in case something
  // slips through, we avoid a class of attack where creative ID manipulation
  // leads to hash collisions.
  if (extension_id.length() != 32)  // 128 bits / 4 = 32 mpdecimal characters
    return false;

  if (whitelist_.find(extension_id) != whitelist_.end())
    return true;

  const std::string id_hash = base::SHA1HashString(extension_id);
  DCHECK(id_hash.length() == base::kSHA1Length);
  const std::string hexencoded_id_hash = base::HexEncode(id_hash.c_str(),
      id_hash.length());
  if (whitelist_.find(hexencoded_id_hash) != whitelist_.end())
    return true;

  return false;
}

}  // namespace extensions
