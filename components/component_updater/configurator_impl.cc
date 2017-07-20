// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/configurator_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/update_client/utils.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // OS_WIN

namespace component_updater {

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater=value1,value2. Do not
// use these values in production code.

// Speed up the initial component checking.
const char kSwitchFastUpdate[] = "fast-update";

// Add "testrequest=1" attribute to the update check request.
const char kSwitchRequestParam[] = "test-request";

// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
extern const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";

#if defined(OS_WIN)
// Disables background downloads.
const char kSwitchDisableBackgroundDownloads[] = "disable-background-downloads";
#endif  // defined(OS_WIN)

const base::Feature kAlternateComponentUrls{"AlternateComponentUrls",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// If there is an element of |vec| of the form |test|=.*, returns the right-
// hand side of that assignment. Otherwise, returns an empty string.
// The right-hand side may contain additional '=' characters, allowing for
// further nesting of switch arguments.
std::string GetSwitchArgument(const std::vector<std::string>& vec,
                              const char* test) {
  if (vec.empty())
    return std::string();
  for (std::vector<std::string>::const_iterator it = vec.begin();
       it != vec.end(); ++it) {
    const std::size_t found = it->find("=");
    if (found != std::string::npos) {
      if (it->substr(0, found) == test) {
        return it->substr(found + 1);
      }
    }
  }
  return std::string();
}

}  // namespace

ConfiguratorImpl::ConfiguratorImpl(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter,
    bool require_encryption)
    : url_request_getter_(url_request_getter),
      fast_update_(false),
      pings_enabled_(false),
      deltas_enabled_(false),
      background_downloads_enabled_(false),
      require_encryption_(require_encryption) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values = base::SplitString(
      cmdline->GetSwitchValueASCII(switches::kComponentUpdater), ",",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  fast_update_ = base::ContainsValue(switch_values, kSwitchFastUpdate);
  pings_enabled_ = !base::ContainsValue(switch_values, kSwitchDisablePings);
  deltas_enabled_ =
      !base::ContainsValue(switch_values, kSwitchDisableDeltaUpdates);

#if defined(OS_WIN)
  background_downloads_enabled_ =
      !base::ContainsValue(switch_values, kSwitchDisableBackgroundDownloads);
#else
  background_downloads_enabled_ = false;
#endif

  const std::string switch_url_source =
      GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (!switch_url_source.empty()) {
    url_source_override_ = GURL(switch_url_source);
    DCHECK(url_source_override_.is_valid());
  }

  if (base::ContainsValue(switch_values, kSwitchRequestParam))
    extra_info_ += "testrequest=\"1\"";
}

ConfiguratorImpl::~ConfiguratorImpl() {}

int ConfiguratorImpl::InitialDelay() const {
  return fast_update_ ? 10 : (6 * kDelayOneMinute);
}

int ConfiguratorImpl::NextCheckDelay() const {
  return 5 * kDelayOneHour;
}

int ConfiguratorImpl::OnDemandDelay() const {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

int ConfiguratorImpl::UpdateDelay() const {
  return fast_update_ ? 10 : (15 * kDelayOneMinute);
}

std::vector<GURL> ConfiguratorImpl::UpdateUrl() const {
  std::vector<GURL> urls;
  if (url_source_override_.is_valid()) {
    urls.push_back(GURL(url_source_override_));
    return urls;
  }

  if (base::FeatureList::IsEnabled(kAlternateComponentUrls)) {
    urls.push_back(GURL(kUpdaterDefaultUrlAlt));
    urls.push_back(GURL(kUpdaterFallbackUrlAlt));
  } else {
    urls.push_back(GURL(kUpdaterDefaultUrl));
    urls.push_back(GURL(kUpdaterFallbackUrl));
  }

  if (require_encryption_)
    update_client::RemoveUnsecureUrls(&urls);

  return urls;
}

std::vector<GURL> ConfiguratorImpl::PingUrl() const {
  return pings_enabled_ ? UpdateUrl() : std::vector<GURL>();
}

base::Version ConfiguratorImpl::GetBrowserVersion() const {
  return base::Version(version_info::GetVersionNumber());
}

std::string ConfiguratorImpl::GetOSLongName() const {
  return version_info::GetOSType();
}

std::string ConfiguratorImpl::ExtraRequestParams() const {
  return extra_info_;
}

std::string ConfiguratorImpl::GetDownloadPreference() const {
  return std::string();
}

net::URLRequestContextGetter* ConfiguratorImpl::RequestContext() const {
  return url_request_getter_;
}

bool ConfiguratorImpl::EnabledDeltas() const {
  return deltas_enabled_;
}

bool ConfiguratorImpl::EnabledComponentUpdates() const {
  return true;
}

bool ConfiguratorImpl::EnabledBackgroundDownloader() const {
  return background_downloads_enabled_;
}

bool ConfiguratorImpl::EnabledCupSigning() const {
  return true;
}

std::vector<uint8_t> ConfiguratorImpl::GetRunActionKeyHash() const {
  return std::vector<uint8_t>{0x5f, 0x94, 0xe0, 0x3c, 0x64, 0x30, 0x9f, 0xbc,
                              0xfe, 0x00, 0x9a, 0x27, 0x3e, 0x52, 0xbf, 0xa5,
                              0x84, 0xb9, 0xb3, 0x75, 0x07, 0x29, 0xde, 0xfa,
                              0x32, 0x76, 0xd9, 0x93, 0xb5, 0xa3, 0xce, 0x02};
}

}  // namespace component_updater
