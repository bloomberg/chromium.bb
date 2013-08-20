// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_configurator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/omaha_query_params/omaha_query_params.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "chrome/browser/component_updater/component_patcher_win.h"
#endif

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater=value1,value2.
// Speed up component checking.
const char kSwitchFastUpdate[] = "fast-update";
// Force out-of-process-xml parsing.
const char kSwitchOutOfProcess[] = "out-of-process";
// Add "testrequest=1" parameter to the update check query.
const char kSwitchRequestParam[] = "test-request";
// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";
// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
extern const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

// The default url from which an update manifest can be fetched. Can be
// overridden with --component-updater=url-source=someurl.
const char kDefaultUrlSource[] =
    "http://clients2.google.com/service/update2/crx";

// The url to send the pings to.
const char kPingUrl[] = "http://tools.google.com/service/update2";

// Returns true if and only if |test| is contained in |vec|.
bool HasSwitchValue(const std::vector<std::string>& vec, const char* test) {
  if (vec.empty())
    return 0;
  return (std::find(vec.begin(), vec.end(), test) != vec.end());
}

// If there is an element of |vec| of the form |test|=.*, returns the right-
// hand side of that assignment. Otherwise, returns an empty string.
// The right-hand side may contain additional '=' characters, allowing for
// further nesting of switch arguments.
std::string GetSwitchArgument(const std::vector<std::string>& vec,
                              const char* test) {
  if (vec.empty())
    return std::string();
  for (std::vector<std::string>::const_iterator it = vec.begin();
      it != vec.end();
      ++it) {
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

class ChromeConfigurator : public ComponentUpdateService::Configurator {
 public:
  ChromeConfigurator(const CommandLine* cmdline,
                     net::URLRequestContextGetter* url_request_getter);

  virtual ~ChromeConfigurator() {}

  virtual int InitialDelay() OVERRIDE;
  virtual int NextCheckDelay() OVERRIDE;
  virtual int StepDelay() OVERRIDE;
  virtual int StepDelayMedium() OVERRIDE;
  virtual int MinimumReCheckWait() OVERRIDE;
  virtual int OnDemandDelay() OVERRIDE;
  virtual GURL UpdateUrl() OVERRIDE;
  virtual GURL PingUrl() OVERRIDE;
  virtual const char* ExtraRequestParams() OVERRIDE;
  virtual size_t UrlSizeLimit() OVERRIDE;
  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE;
  virtual bool InProcess() OVERRIDE;
  virtual ComponentPatcher* CreateComponentPatcher() OVERRIDE;
  virtual bool DeltasEnabled() const OVERRIDE;

 private:
  net::URLRequestContextGetter* url_request_getter_;
  std::string extra_info_;
  std::string url_source_;
  bool fast_update_;
  bool out_of_process_;
  bool pings_enabled_;
  bool deltas_enabled_;
};

ChromeConfigurator::ChromeConfigurator(const CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
      : url_request_getter_(url_request_getter),
        extra_info_(chrome::OmahaQueryParams::Get(
            chrome::OmahaQueryParams::CHROME)),
        fast_update_(false),
        out_of_process_(false),
        pings_enabled_(false),
        deltas_enabled_(false) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values;
  Tokenize(cmdline->GetSwitchValueASCII(switches::kComponentUpdater),
      ",", &switch_values);
  fast_update_ = HasSwitchValue(switch_values, kSwitchFastUpdate);
  out_of_process_ = HasSwitchValue(switch_values, kSwitchOutOfProcess);
  pings_enabled_ = !HasSwitchValue(switch_values, kSwitchDisablePings);
#if defined(OS_WIN)
  deltas_enabled_ = !HasSwitchValue(switch_values, kSwitchDisableDeltaUpdates);
#else
  deltas_enabled_ = false;
#endif

  url_source_ = GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (url_source_.empty()) {
    url_source_ = kDefaultUrlSource;
  }

  // Make the extra request params, they are necessary so omaha does
  // not deliver components that are going to be rejected at install time.
#if defined(OS_WIN)
  if (base::win::OSInfo::GetInstance()->wow64_status() ==
      base::win::OSInfo::WOW64_ENABLED)
    extra_info_ += "&wow64=1";
#endif
  if (HasSwitchValue(switch_values, kSwitchRequestParam))
    extra_info_ += "&testrequest=1";
}

int ChromeConfigurator::InitialDelay() {
  return fast_update_ ? 1 : (6 * kDelayOneMinute);
}

int ChromeConfigurator::NextCheckDelay() {
  return fast_update_ ? 3 : (2 * kDelayOneHour);
}

int ChromeConfigurator::StepDelayMedium() {
  return fast_update_ ? 3 : (15 * kDelayOneMinute);
}

int ChromeConfigurator::StepDelay() {
  return fast_update_ ? 1 : 4;
}

int ChromeConfigurator::MinimumReCheckWait() {
  return fast_update_ ? 30 : (6 * kDelayOneHour);
}

int ChromeConfigurator::OnDemandDelay() {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

GURL ChromeConfigurator::UpdateUrl() {
  return GURL(url_source_);
}

GURL ChromeConfigurator::PingUrl() {
  return pings_enabled_ ? GURL(kPingUrl) : GURL();
}

const char* ChromeConfigurator::ExtraRequestParams() {
  return extra_info_.c_str();
}

size_t ChromeConfigurator::UrlSizeLimit() {
  return 1024ul;
}

net::URLRequestContextGetter* ChromeConfigurator::RequestContext() {
  return url_request_getter_;
}

bool ChromeConfigurator::InProcess() {
  return !out_of_process_;
}

ComponentPatcher* ChromeConfigurator::CreateComponentPatcher() {
#if defined(OS_WIN)
  return new ComponentPatcherWin();
#else
  return new ComponentPatcherCrossPlatform();
#endif
}

bool ChromeConfigurator::DeltasEnabled() const {
  return deltas_enabled_;
}

ComponentUpdateService::Configurator* MakeChromeComponentUpdaterConfigurator(
    const CommandLine* cmdline, net::URLRequestContextGetter* context_getter) {
  return new ChromeConfigurator(cmdline, context_getter);
}
