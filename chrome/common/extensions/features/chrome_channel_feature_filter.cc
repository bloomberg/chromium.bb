// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

namespace {

static const char kFeatureChannelKey[] = "channel";

struct Mappings {
  Mappings() {
    channels["trunk"] = chrome::VersionInfo::CHANNEL_UNKNOWN;
    channels["canary"] = chrome::VersionInfo::CHANNEL_CANARY;
    channels["dev"] = chrome::VersionInfo::CHANNEL_DEV;
    channels["beta"] = chrome::VersionInfo::CHANNEL_BETA;
    channels["stable"] = chrome::VersionInfo::CHANNEL_STABLE;
  }

  std::map<std::string, chrome::VersionInfo::Channel> channels;
};

base::LazyInstance<Mappings> g_mappings = LAZY_INSTANCE_INITIALIZER;

std::string GetChannelName(chrome::VersionInfo::Channel channel) {
  typedef std::map<std::string, chrome::VersionInfo::Channel> ChannelsMap;
  ChannelsMap channels = g_mappings.Get().channels;
  for (ChannelsMap::iterator i = channels.begin(); i != channels.end(); ++i) {
    if (i->second == channel)
      return i->first;
  }
  NOTREACHED();
  return "unknown";
}

chrome::VersionInfo::Channel GetChannelValue(const std::string& name) {
  typedef std::map<std::string, chrome::VersionInfo::Channel> ChannelsMap;
  ChannelsMap channels = g_mappings.Get().channels;
  ChannelsMap::const_iterator iter = channels.find(name);
  CHECK(iter != channels.end());
  return iter->second;
}

}  // namespace

ChromeChannelFeatureFilter::ChromeChannelFeatureFilter(SimpleFeature* feature)
    : SimpleFeatureFilter(feature),
      channel_has_been_set_(false),
      channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
}

ChromeChannelFeatureFilter::~ChromeChannelFeatureFilter() {}

std::string ChromeChannelFeatureFilter::Parse(
    const base::DictionaryValue* value) {
  std::string channel_name;
  if (value->GetString(kFeatureChannelKey, &channel_name)) {
    channel_ = GetChannelValue(channel_name);
  }

  // The "trunk" channel uses VersionInfo::CHANNEL_UNKNOWN, so we need to keep
  // track of whether the channel has been set or not separately.
  channel_has_been_set_ |= value->HasKey(kFeatureChannelKey);

  if (!channel_has_been_set_ && !feature()->HasDependencies()) {
    return feature()->name() +
           ": Must supply a value for channel or dependencies.";
  }

  return std::string();
}

Feature::Availability ChromeChannelFeatureFilter::IsAvailableToManifest(
    const std::string& extension_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Feature::Platform platfortm) const {
  if (channel_has_been_set_ && channel_ < GetCurrentChannel()) {
    return Feature::CreateAvailability(
        Feature::UNSUPPORTED_CHANNEL,
        base::StringPrintf(
            "'%s' requires Google Chrome %s channel or newer, but this is the "
            "%s channel.",
            feature()->name().c_str(),
            GetChannelName(channel_).c_str(),
            GetChannelName(GetCurrentChannel()).c_str()));
  }
  return Feature::CreateAvailability(Feature::IS_AVAILABLE, std::string());
}

}  // namespace extensions
