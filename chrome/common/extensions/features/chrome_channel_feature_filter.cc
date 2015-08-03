// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

namespace {

static const char kFeatureChannelKey[] = "channel";

struct Mappings {
  Mappings() {
    channels["trunk"] = version_info::Channel::UNKNOWN;
    channels["canary"] = version_info::Channel::CANARY;
    channels["dev"] = version_info::Channel::DEV;
    channels["beta"] = version_info::Channel::BETA;
    channels["stable"] = version_info::Channel::STABLE;
  }

  std::map<std::string, version_info::Channel> channels;
};

base::LazyInstance<Mappings> g_mappings = LAZY_INSTANCE_INITIALIZER;

std::string GetChannelName(version_info::Channel channel) {
  typedef std::map<std::string, version_info::Channel> ChannelsMap;
  ChannelsMap channels = g_mappings.Get().channels;
  for (ChannelsMap::iterator i = channels.begin(); i != channels.end(); ++i) {
    if (i->second == channel)
      return i->first;
  }
  NOTREACHED();
  return "unknown";
}

version_info::Channel GetChannelValue(const std::string& name) {
  typedef std::map<std::string, version_info::Channel> ChannelsMap;
  ChannelsMap channels = g_mappings.Get().channels;
  ChannelsMap::const_iterator iter = channels.find(name);
  CHECK(iter != channels.end());
  return iter->second;
}

}  // namespace

ChromeChannelFeatureFilter::ChromeChannelFeatureFilter(SimpleFeature* feature)
    : SimpleFeatureFilter(feature),
      channel_has_been_set_(false),
      channel_(version_info::Channel::UNKNOWN) {}

ChromeChannelFeatureFilter::~ChromeChannelFeatureFilter() {}

std::string ChromeChannelFeatureFilter::Parse(
    const base::DictionaryValue* value) {
  std::string channel_name;
  if (value->GetString(kFeatureChannelKey, &channel_name)) {
    channel_ = GetChannelValue(channel_name);
  }

  // The "trunk" channel uses version_info::Channel::UNKNOWN, so we need to keep
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
    return Feature::Availability(
        Feature::UNSUPPORTED_CHANNEL,
        base::StringPrintf(
            "'%s' requires Google Chrome %s channel or newer, but this is the "
            "%s channel.",
            feature()->name().c_str(), GetChannelName(channel_).c_str(),
            GetChannelName(GetCurrentChannel()).c_str()));
  }
  return Feature::Availability(Feature::IS_AVAILABLE, std::string());
}

}  // namespace extensions
