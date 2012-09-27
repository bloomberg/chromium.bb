// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_metadata.h"

#include "base/logging.h"
#include "webkit/plugins/npapi/plugin_utils.h"
#include "webkit/plugins/webplugininfo.h"

// static
const char PluginMetadata::kAdobeReaderGroupName[] = "Adobe Reader";
const char PluginMetadata::kJavaGroupName[] = "Java(TM)";
const char PluginMetadata::kQuickTimeGroupName[] = "QuickTime Player";
const char PluginMetadata::kShockwaveGroupName[] = "Adobe Shockwave Player";
const char PluginMetadata::kRealPlayerGroupName[] = "RealPlayer";
const char PluginMetadata::kSilverlightGroupName[] = "Silverlight";
const char PluginMetadata::kWindowsMediaPlayerGroupName[] =
    "Windows Media Player";

PluginMetadata::PluginMetadata(const std::string& identifier,
                               const string16& name,
                               bool url_for_display,
                               const GURL& plugin_url,
                               const GURL& help_url,
                               const string16& group_name_matcher)
    : identifier_(identifier),
      name_(name),
      group_name_matcher_(group_name_matcher),
      url_for_display_(url_for_display),
      plugin_url_(plugin_url),
      help_url_(help_url) {
}

PluginMetadata::~PluginMetadata() {
}

void PluginMetadata::AddVersion(const Version& version,
                                SecurityStatus status) {
  DCHECK(versions_.find(version) == versions_.end());
  versions_[version] = status;
}

bool PluginMetadata::MatchesPlugin(const webkit::WebPluginInfo& plugin) {
  return plugin.name.find(group_name_matcher_) != string16::npos;
}

// static
bool PluginMetadata::ParseSecurityStatus(
    const std::string& status_str,
    PluginMetadata::SecurityStatus* status) {
  if (status_str == "up_to_date")
    *status = SECURITY_STATUS_UP_TO_DATE;
  else if (status_str == "out_of_date")
    *status = SECURITY_STATUS_OUT_OF_DATE;
  else if (status_str == "requires_authorization")
    *status = SECURITY_STATUS_REQUIRES_AUTHORIZATION;
  else
    return false;

  return true;
}

PluginMetadata::SecurityStatus PluginMetadata::GetSecurityStatus(
    const webkit::WebPluginInfo& plugin) const {
  if (versions_.empty()) {
#if defined(OS_LINUX)
    // On Linux, unknown plugins require authorization.
    return SECURITY_STATUS_REQUIRES_AUTHORIZATION;
#else
    return SECURITY_STATUS_UP_TO_DATE;
#endif
  }

  Version version;
  webkit::npapi::CreateVersionFromString(plugin.version, &version);
  if (!version.IsValid())
    version = Version("0");

  // |lower_bound| returns the latest version that is not newer than |version|.
  std::map<Version, SecurityStatus, VersionComparator>::const_iterator it =
      versions_.lower_bound(version);
  // If there is at least one version defined, everything older than the oldest
  // defined version is considered out-of-date.
  if (it == versions_.end())
    return SECURITY_STATUS_OUT_OF_DATE;

  return it->second;
}

bool PluginMetadata::VersionComparator::operator() (const Version& lhs,
                                                    const Version& rhs) const {
  // Keep versions ordered by newest (biggest) first.
  return lhs.CompareTo(rhs) > 0;
}
