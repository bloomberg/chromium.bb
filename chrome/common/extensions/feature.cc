// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/feature.h"

#include <map>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "chrome/common/chrome_switches.h"

using chrome::VersionInfo;

namespace {

struct Mappings {
  Mappings() {
    extension_types["extension"] = Extension::TYPE_EXTENSION;
    extension_types["theme"] = Extension::TYPE_THEME;
    extension_types["packaged_app"] = Extension::TYPE_PACKAGED_APP;
    extension_types["hosted_app"] = Extension::TYPE_HOSTED_APP;
    extension_types["platform_app"] = Extension::TYPE_PLATFORM_APP;

    contexts["blessed_extension"] =
        extensions::Feature::BLESSED_EXTENSION_CONTEXT;
    contexts["unblessed_extension"] =
        extensions::Feature::UNBLESSED_EXTENSION_CONTEXT;
    contexts["content_script"] = extensions::Feature::CONTENT_SCRIPT_CONTEXT;
    contexts["web_page"] = extensions::Feature::WEB_PAGE_CONTEXT;

    locations["component"] = extensions::Feature::COMPONENT_LOCATION;

    platforms["chromeos"] = extensions::Feature::CHROMEOS_PLATFORM;

    channels["trunk"] = VersionInfo::CHANNEL_UNKNOWN;
    channels["canary"] = VersionInfo::CHANNEL_CANARY;
    channels["dev"] = VersionInfo::CHANNEL_DEV;
    channels["beta"] = VersionInfo::CHANNEL_BETA;
    channels["stable"] = VersionInfo::CHANNEL_STABLE;
  }

  std::map<std::string, Extension::Type> extension_types;
  std::map<std::string, extensions::Feature::Context> contexts;
  std::map<std::string, extensions::Feature::Location> locations;
  std::map<std::string, extensions::Feature::Platform> platforms;
  std::map<std::string, VersionInfo::Channel> channels;
};

static base::LazyInstance<Mappings> g_mappings = LAZY_INSTANCE_INITIALIZER;

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

static bool g_channel_checking_enabled = false;

class Channel {
 public:
  VersionInfo::Channel GetChannel() {
    CHECK(g_channel_checking_enabled);
    if (channel_for_testing_.get())
      return *channel_for_testing_;
    return VersionInfo::GetChannel();
  }

  void SetChannelForTesting(VersionInfo::Channel channel_for_testing) {
    channel_for_testing_.reset(new VersionInfo::Channel(channel_for_testing));
  }

  void ResetChannelForTesting() {
    channel_for_testing_.reset();
  }

 private:
  scoped_ptr<VersionInfo::Channel> channel_for_testing_;
};

static base::LazyInstance<Channel> g_channel = LAZY_INSTANCE_INITIALIZER;

// TODO(aa): Can we replace all this manual parsing with JSON schema stuff?

void ParseSet(const DictionaryValue* value,
              const std::string& property,
              std::set<std::string>* set) {
  ListValue* list_value = NULL;
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

}  // namespace

namespace extensions {

Feature::Feature()
  : location_(UNSPECIFIED_LOCATION),
    platform_(UNSPECIFIED_PLATFORM),
    min_manifest_version_(0),
    max_manifest_version_(0),
    channel_(VersionInfo::CHANNEL_UNKNOWN) {
}

Feature::Feature(const Feature& other)
    : whitelist_(other.whitelist_),
      extension_types_(other.extension_types_),
      contexts_(other.contexts_),
      location_(other.location_),
      platform_(other.platform_),
      min_manifest_version_(other.min_manifest_version_),
      max_manifest_version_(other.max_manifest_version_),
      channel_(other.channel_) {
}

Feature::~Feature() {
}

bool Feature::Equals(const Feature& other) const {
  return whitelist_ == other.whitelist_ &&
      extension_types_ == other.extension_types_ &&
      contexts_ == other.contexts_ &&
      location_ == other.location_ &&
      platform_ == other.platform_ &&
      min_manifest_version_ == other.min_manifest_version_ &&
      max_manifest_version_ == other.max_manifest_version_ &&
      channel_ == other.channel_;
}

// static
Feature::Platform Feature::GetCurrentPlatform() {
#if defined(OS_CHROMEOS)
  return CHROMEOS_PLATFORM;
#else
  return UNSPECIFIED_PLATFORM;
#endif
}

// static
Feature::Location Feature::ConvertLocation(Extension::Location location) {
  if (location == Extension::COMPONENT)
    return COMPONENT_LOCATION;
  else
    return UNSPECIFIED_LOCATION;
}

void Feature::Parse(const DictionaryValue* value) {
  ParseSet(value, "whitelist", &whitelist_);
  ParseEnumSet<Extension::Type>(value, "extension_types", &extension_types_,
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
}

std::string Feature::GetErrorMessage(Feature::Availability result) {
  switch (result) {
    case IS_AVAILABLE:
      return "";
    case NOT_FOUND_IN_WHITELIST:
      return base::StringPrintf(
          "'%s' is not allowed for specified extension ID.",
          name().c_str());
    case INVALID_TYPE:
      return base::StringPrintf(
          "'%s' is not allowed for specified package type (theme, app, etc.).",
          name().c_str());
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
          "'%s' requires Google Chrome %s channel or newer.",
          name().c_str(),
          GetChannelName(channel_).c_str());
  }

  return "";
}

Feature::Availability Feature::IsAvailableToManifest(
    const std::string& extension_id,
    Extension::Type type,
    Location location,
    int manifest_version,
    Platform platform) const {
  // Component extensions can access any feature.
  if (location == COMPONENT_LOCATION)
    return IS_AVAILABLE;

  if (!whitelist_.empty()) {
    if (whitelist_.find(extension_id) == whitelist_.end()) {
      // TODO(aa): This is gross. There should be a better way to test the
      // whitelist.
      CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (!command_line->HasSwitch(switches::kWhitelistedExtensionID))
        return NOT_FOUND_IN_WHITELIST;

      std::string whitelist_switch_value =
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kWhitelistedExtensionID);
      if (extension_id != whitelist_switch_value)
        return NOT_FOUND_IN_WHITELIST;
    }
  }

  if (!extension_types_.empty() &&
      extension_types_.find(type) == extension_types_.end()) {
    return INVALID_TYPE;
  }

  if (location_ != UNSPECIFIED_LOCATION && location_ != location)
    return INVALID_LOCATION;

  if (platform_ != UNSPECIFIED_PLATFORM && platform_ != platform)
    return INVALID_PLATFORM;

  if (min_manifest_version_ != 0 && manifest_version < min_manifest_version_)
    return INVALID_MIN_MANIFEST_VERSION;

  if (max_manifest_version_ != 0 && manifest_version > max_manifest_version_)
    return INVALID_MAX_MANIFEST_VERSION;

  if (g_channel_checking_enabled) {
    if (channel_ < g_channel.Get().GetChannel())
      return UNSUPPORTED_CHANNEL;
  }

  return IS_AVAILABLE;
}

Feature::Availability Feature::IsAvailableToContext(
    const Extension* extension,
    Feature::Context context,
    Feature::Platform platform) const {
  Availability result = IsAvailableToManifest(
      extension->id(),
      extension->GetType(),
      ConvertLocation(extension->location()),
      extension->manifest_version(),
      platform);
  if (result != IS_AVAILABLE)
    return result;

  if (!contexts_.empty() &&
      contexts_.find(context) == contexts_.end()) {
    return INVALID_CONTEXT;
  }

  return IS_AVAILABLE;
}

// static
void Feature::SetChannelCheckingEnabled(bool enabled) {
  g_channel_checking_enabled = enabled;
}

// static
void Feature::ResetChannelCheckingEnabled() {
  g_channel_checking_enabled = false;
}

// static
void Feature::SetChannelForTesting(VersionInfo::Channel channel) {
  g_channel.Get().SetChannelForTesting(channel);
}

// static
void Feature::ResetChannelForTesting() {
  g_channel.Get().ResetChannelForTesting();
}

}  // namespace
