// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // OS_WIN
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"
#include "chrome/browser/omaha_query_params/chrome_omaha_query_params_delegate.h"
#include "chrome/common/chrome_version_info.h"
#include "components/component_updater/component_updater_configurator.h"
#include "components/component_updater/component_updater_switches.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace component_updater {

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater=value1,value2.
// Speed up component checking.
const char kSwitchFastUpdate[] = "fast-update";

// Add "testrequest=1" attribute to the update check request.
const char kSwitchRequestParam[] = "test-request";

// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
extern const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

#define COMPONENT_UPDATER_SERVICE_ENDPOINT \
  "//clients2.google.com/service/update2"

// The default URL for the v3 protocol service endpoint. In some cases, the
// component updater is allowed to fall back to and alternate URL source, if
// the request to the default URL source fails.
// The value of |kDefaultUrlSource| can be overridden with
// --component-updater=url-source=someurl.
const char kDefaultUrlSource[] = "https:" COMPONENT_UPDATER_SERVICE_ENDPOINT;
const char kAltUrlSource[] = "http:" COMPONENT_UPDATER_SERVICE_ENDPOINT;

// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";

#if defined(OS_WIN)
// Disables background downloads.
const char kSwitchDisableBackgroundDownloads[] = "disable-background-downloads";
#endif  // defined(OS_WIN)

// Returns true if and only if |test| is contained in |vec|.
bool HasSwitchValue(const std::vector<std::string>& vec, const char* test) {
  if (vec.empty())
    return 0;
  return (std::find(vec.begin(), vec.end(), test) != vec.end());
}

// Returns true if falling back on an alternate, unsafe, service URL is
// allowed. In the fallback case, the security of the component update relies
// only on the integrity of the CRX payloads, which is self-validating.
// This is allowed only for some of the pre-Windows Vista versions not including
// Windows XP SP3. As a side note, pings could be sent to the alternate URL too.
bool CanUseAltUrlSource() {
#if defined(OS_WIN)
  return !base::win::MaybeHasSHA256Support();
#else
  return false;
#endif  // OS_WIN
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

class ChromeConfigurator : public Configurator {
 public:
  ChromeConfigurator(const CommandLine* cmdline,
                     net::URLRequestContextGetter* url_request_getter);

  virtual ~ChromeConfigurator() {}

  virtual int InitialDelay() const OVERRIDE;
  virtual int NextCheckDelay() OVERRIDE;
  virtual int StepDelay() const OVERRIDE;
  virtual int StepDelayMedium() OVERRIDE;
  virtual int MinimumReCheckWait() const OVERRIDE;
  virtual int OnDemandDelay() const OVERRIDE;
  virtual std::vector<GURL> UpdateUrl() const OVERRIDE;
  virtual std::vector<GURL> PingUrl() const OVERRIDE;
  virtual base::Version GetBrowserVersion() const OVERRIDE;
  virtual std::string GetChannel() const OVERRIDE;
  virtual std::string GetLang() const OVERRIDE;
  virtual std::string GetOSLongName() const OVERRIDE;
  virtual std::string ExtraRequestParams() const OVERRIDE;
  virtual size_t UrlSizeLimit() const OVERRIDE;
  virtual net::URLRequestContextGetter* RequestContext() const OVERRIDE;
  virtual scoped_refptr<OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const OVERRIDE;
  virtual bool DeltasEnabled() const OVERRIDE;
  virtual bool UseBackgroundDownloader() const OVERRIDE;
  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetSingleThreadTaskRunner() const OVERRIDE;

 private:
  net::URLRequestContextGetter* url_request_getter_;
  std::string extra_info_;
  GURL url_source_override_;
  bool fast_update_;
  bool pings_enabled_;
  bool deltas_enabled_;
  bool background_downloads_enabled_;
  bool fallback_to_alt_source_url_enabled_;
};

ChromeConfigurator::ChromeConfigurator(
    const CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
    : url_request_getter_(url_request_getter),
      fast_update_(false),
      pings_enabled_(false),
      deltas_enabled_(false),
      background_downloads_enabled_(false),
      fallback_to_alt_source_url_enabled_(false) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values;
  Tokenize(cmdline->GetSwitchValueASCII(switches::kComponentUpdater),
           ",",
           &switch_values);
  fast_update_ = HasSwitchValue(switch_values, kSwitchFastUpdate);
  pings_enabled_ = !HasSwitchValue(switch_values, kSwitchDisablePings);
  deltas_enabled_ = !HasSwitchValue(switch_values, kSwitchDisableDeltaUpdates);

#if defined(OS_WIN)
  background_downloads_enabled_ =
      !HasSwitchValue(switch_values, kSwitchDisableBackgroundDownloads);
#else
  background_downloads_enabled_ = false;
#endif

  const std::string switch_url_source =
      GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (!switch_url_source.empty()) {
    url_source_override_ = GURL(switch_url_source);
    DCHECK(url_source_override_.is_valid());
  }

  if (HasSwitchValue(switch_values, kSwitchRequestParam))
    extra_info_ += "testrequest=\"1\"";

  fallback_to_alt_source_url_enabled_ = CanUseAltUrlSource();
}

int ChromeConfigurator::InitialDelay() const {
  return fast_update_ ? 1 : (6 * kDelayOneMinute);
}

int ChromeConfigurator::NextCheckDelay() {
  return fast_update_ ? 3 : (6 * kDelayOneHour);
}

int ChromeConfigurator::StepDelayMedium() {
  return fast_update_ ? 3 : (15 * kDelayOneMinute);
}

int ChromeConfigurator::StepDelay() const {
  return fast_update_ ? 1 : 1;
}

int ChromeConfigurator::MinimumReCheckWait() const {
  return fast_update_ ? 30 : (6 * kDelayOneHour);
}

int ChromeConfigurator::OnDemandDelay() const {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

std::vector<GURL> ChromeConfigurator::UpdateUrl() const {
  std::vector<GURL> urls;
  if (url_source_override_.is_valid()) {
    urls.push_back(GURL(url_source_override_));
  } else {
    urls.push_back(GURL(kDefaultUrlSource));
    if (fallback_to_alt_source_url_enabled_) {
      urls.push_back(GURL(kAltUrlSource));
    }
  }
  return urls;
}

std::vector<GURL> ChromeConfigurator::PingUrl() const {
  return pings_enabled_ ? UpdateUrl() : std::vector<GURL>();
}

base::Version ChromeConfigurator::GetBrowserVersion() const {
  return base::Version(chrome::VersionInfo().Version());
}

std::string ChromeConfigurator::GetChannel() const {
  return ChromeOmahaQueryParamsDelegate::GetChannelString();
}

std::string ChromeConfigurator::GetLang() const {
  return ChromeOmahaQueryParamsDelegate::GetLang();
}

std::string ChromeConfigurator::GetOSLongName() const {
  return chrome::VersionInfo().OSType();
}

std::string ChromeConfigurator::ExtraRequestParams() const {
  return extra_info_;
}

size_t ChromeConfigurator::UrlSizeLimit() const {
  return 1024ul;
}

net::URLRequestContextGetter* ChromeConfigurator::RequestContext() const {
  return url_request_getter_;
}

scoped_refptr<OutOfProcessPatcher>
ChromeConfigurator::CreateOutOfProcessPatcher() const {
  return make_scoped_refptr(new ChromeOutOfProcessPatcher);
}

bool ChromeConfigurator::DeltasEnabled() const {
  return deltas_enabled_;
}

bool ChromeConfigurator::UseBackgroundDownloader() const {
  return background_downloads_enabled_;
}

scoped_refptr<base::SequencedTaskRunner>
ChromeConfigurator::GetSequencedTaskRunner() const {
  return content::BrowserThread::GetBlockingPool()
      ->GetSequencedTaskRunnerWithShutdownBehavior(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeConfigurator::GetSingleThreadTaskRunner() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::FILE);
}

}  // namespace

Configurator* MakeChromeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter) {
  return new ChromeConfigurator(cmdline, context_getter);
}

}  // namespace component_updater
