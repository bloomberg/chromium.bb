// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/content_settings/content_settings_base_provider.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

// True if a given content settings type requires additional resource
// identifiers.
const bool kRequiresResourceIdentifier[CONTENT_SETTINGS_NUM_TYPES] = {
  false,  // CONTENT_SETTINGS_TYPE_COOKIES
  false,  // CONTENT_SETTINGS_TYPE_IMAGES
  false,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  true,   // CONTENT_SETTINGS_TYPE_PLUGINS
  false,  // CONTENT_SETTINGS_TYPE_POPUPS
  false,  // Not used for Geolocation
  false,  // Not used for Notifications
};

}  // namespace

namespace content_settings {

ExtendedContentSettings::ExtendedContentSettings() {}

ExtendedContentSettings::ExtendedContentSettings(
    const ExtendedContentSettings& rhs)
    : content_settings(rhs.content_settings),
      content_settings_for_resources(rhs.content_settings_for_resources) {
}

ExtendedContentSettings::~ExtendedContentSettings() {}

BaseProvider::BaseProvider(bool is_otr)
    : is_off_the_record_(is_otr) {
}

BaseProvider::~BaseProvider() {}

bool BaseProvider::RequiresResourceIdentifier(
    ContentSettingsType content_type) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableResourceContentSettings)) {
    return kRequiresResourceIdentifier[content_type];
  } else {
    return false;
  }
}

bool BaseProvider::AllDefault(
    const ExtendedContentSettings& settings) const {
  for (size_t i = 0; i < arraysize(settings.content_settings.settings); ++i) {
    if (settings.content_settings.settings[i] != CONTENT_SETTING_DEFAULT)
      return false;
  }
  return settings.content_settings_for_resources.empty();
}

ContentSetting BaseProvider::GetContentSetting(
    const GURL& requesting_url,
    const GURL& embedding_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  // Support for embedding_patterns is not implemented yet.
  DCHECK(requesting_url == embedding_url);

  if (!RequiresResourceIdentifier(content_type) ||
      (RequiresResourceIdentifier(content_type) && resource_identifier.empty()))
    return GetNonDefaultContentSettings(requesting_url).settings[content_type];

  // Resolve content settings with resource identifier.
  // 1. Check for pattern that exactly match the url/host
  //     1.1 In the content-settings-map
  //     1.2 In the off_the_record content-settings-map
  // 3. Shorten the url subdomain by subdomain and try to find a pattern in
  //     3.1 OTR content-settings-map
  //     3.2 content-settings-map
  base::AutoLock auto_lock(lock_);
  const std::string host(net::GetHostOrSpecFromURL(requesting_url));
  ContentSettingsTypeResourceIdentifierPair
      requested_setting(content_type, resource_identifier);

  // Check for exact matches first.
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  if (i != host_content_settings_.end() &&
      i->second.content_settings_for_resources.find(requested_setting) !=
      i->second.content_settings_for_resources.end()) {
    return i->second.content_settings_for_resources.find(
        requested_setting)->second;
  }

  // If this map is not for an off-the-record profile, these searches will never
  // match. The additional off-the-record exceptions always overwrite the
  // regular ones.
  i = off_the_record_settings_.find(host);
  if (i != off_the_record_settings_.end() &&
      i->second.content_settings_for_resources.find(requested_setting) !=
      i->second.content_settings_for_resources.end()) {
    return i->second.content_settings_for_resources.find(
        requested_setting)->second;
  }

  // Match patterns starting with the most concrete pattern match.
  for (std::string key =
       std::string(ContentSettingsPattern::kDomainWildcard) + host; ; ) {
    HostContentSettings::const_iterator i(off_the_record_settings_.find(key));
    if (i != off_the_record_settings_.end() &&
        i->second.content_settings_for_resources.find(requested_setting) !=
        i->second.content_settings_for_resources.end()) {
      return i->second.content_settings_for_resources.find(
          requested_setting)->second;
    }

    i = host_content_settings_.find(key);
    if (i != host_content_settings_.end() &&
        i->second.content_settings_for_resources.find(requested_setting) !=
        i->second.content_settings_for_resources.end()) {
      return i->second.content_settings_for_resources.find(
          requested_setting)->second;
    }

    const size_t next_dot =
        key.find('.', ContentSettingsPattern::kDomainWildcardLength);
    if (next_dot == std::string::npos)
      break;
    key.erase(ContentSettingsPattern::kDomainWildcardLength,
              next_dot - ContentSettingsPattern::kDomainWildcardLength + 1);
  }

  return CONTENT_SETTING_DEFAULT;
}

void BaseProvider::GetAllContentSettingsRules(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Rules* content_setting_rules) const {
  DCHECK(content_setting_rules);
  content_setting_rules->clear();

  const HostContentSettings* map_to_return =
      is_off_the_record_ ? &off_the_record_settings_ : &host_content_settings_;
  ContentSettingsTypeResourceIdentifierPair requested_setting(
      content_type, resource_identifier);

  base::AutoLock auto_lock(lock_);
  for (HostContentSettings::const_iterator i(map_to_return->begin());
       i != map_to_return->end(); ++i) {
    ContentSetting setting;
    if (RequiresResourceIdentifier(content_type)) {
      if (i->second.content_settings_for_resources.find(requested_setting) !=
          i->second.content_settings_for_resources.end()) {
        setting = i->second.content_settings_for_resources.find(
            requested_setting)->second;
      } else {
        setting = CONTENT_SETTING_DEFAULT;
      }
    } else {
     setting = i->second.content_settings.settings[content_type];
    }
    if (setting != CONTENT_SETTING_DEFAULT) {
      // Use of push_back() relies on the map iterator traversing in order of
      // ascending keys.
      content_setting_rules->push_back(Rule(ContentSettingsPattern(i->first),
                                            ContentSettingsPattern(i->first),
                                            setting));
    }
  }
}

ContentSettings BaseProvider::GetNonDefaultContentSettings(
    const GURL& url) const {
  base::AutoLock auto_lock(lock_);

  const std::string host(net::GetHostOrSpecFromURL(url));
  ContentSettings output;
  for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
    output.settings[j] = CONTENT_SETTING_DEFAULT;

  // Check for exact matches first.
  HostContentSettings::const_iterator i(host_content_settings_.find(host));
  if (i != host_content_settings_.end())
    output = i->second.content_settings;

  // If this map is not for an off-the-record profile, these searches will never
  // match. The additional off-the-record exceptions always overwrite the
  // regular ones.
  i = off_the_record_settings_.find(host);
  if (i != off_the_record_settings_.end()) {
    for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j)
      if (i->second.content_settings.settings[j] != CONTENT_SETTING_DEFAULT)
        output.settings[j] = i->second.content_settings.settings[j];
  }

  // Match patterns starting with the most concrete pattern match.
  for (std::string key =
       std::string(ContentSettingsPattern::kDomainWildcard) + host; ; ) {
    HostContentSettings::const_iterator i(off_the_record_settings_.find(key));
    if (i != off_the_record_settings_.end()) {
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
        if (output.settings[j] == CONTENT_SETTING_DEFAULT)
          output.settings[j] = i->second.content_settings.settings[j];
      }
    }
    i = host_content_settings_.find(key);
    if (i != host_content_settings_.end()) {
      for (int j = 0; j < CONTENT_SETTINGS_NUM_TYPES; ++j) {
        if (output.settings[j] == CONTENT_SETTING_DEFAULT)
          output.settings[j] = i->second.content_settings.settings[j];
      }
    }
    const size_t next_dot =
        key.find('.', ContentSettingsPattern::kDomainWildcardLength);
    if (next_dot == std::string::npos)
      break;
    key.erase(ContentSettingsPattern::kDomainWildcardLength,
              next_dot - ContentSettingsPattern::kDomainWildcardLength + 1);
  }

  return output;
}

void BaseProvider::UpdateContentSettingsMap(
    const ContentSettingsPattern& requesting_pattern,
    const ContentSettingsPattern& embedding_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    ContentSetting content_setting) {
  std::string pattern_str(requesting_pattern.CanonicalizePattern());
  HostContentSettings* content_settings_map = host_content_settings();
  ExtendedContentSettings& extended_settings =
      (*content_settings_map)[pattern_str];
  extended_settings.content_settings.settings[content_type] = content_setting;
}

// static
ContentSetting BaseProvider::ClickToPlayFixup(ContentSettingsType content_type,
                                              ContentSetting setting) {
  if (setting == CONTENT_SETTING_ASK &&
      content_type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay)) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

}  // namespace content_settings
