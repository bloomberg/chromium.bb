// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater-debug=value1,value2.
// Speed up component checking.
const char kDebugFastUpdate[] = "fast-update";
// Force out-of-process-xml parsing.
const char kDebugOutOfProcess[] = "out-of-process";
// Add "testrequest=1" parameter to the update check query.
const char kDebugRequestParam[] = "test-request";

// The urls from which an update manifest can be fetched.
const char* kUrlSources[] = {
  "http://clients2.google.com/service/update2/crx",       // BANDAID
  "http://omaha.google.com/service/update2/crx",          // CWS_PUBLIC
  "http://omaha.sandbox.google.com/service/update2/crx"   // CWS_SANDBOX
};

bool HasDebugValue(const std::vector<std::string>& vec, const char* test) {
  if (vec.empty())
    return 0;
  return (std::find(vec.begin(), vec.end(), test) != vec.end());
}

// The request extra information is the OS and architecture, this helps
// the server select the right package to be delivered.
const char kExtraInfo[] =
#if defined(OS_MACOSX)
  #if defined(__amd64__)
    "os=mac&arch=x64&prod=chrome&prodversion=";
  #elif defined(__i386__)
     "os=mac&arch=x86&prod=chrome&prodversion=";
  #else
     #error "unknown mac architecture"
  #endif
#elif defined(OS_WIN)
  #if defined(_WIN64)
    "os=win&arch=x64&prod=chrome&prodversion=";
  #elif defined(_WIN32)
    "os=win&arch=x86&prod=chrome&prodversion=";
  #else
    #error "unknown windows architecture"
  #endif
#elif defined(OS_ANDROID)
  #if defined(__i386__)
    "os=android&arch=x86&prod=chrome&prodversion=";
  #elif defined(__arm__)
    "os=android&arch=arm&prod=chrome&prodversion=";
  #else
    "os=android&arch=unknown&prod=chrome&prodversion=";
  #endif
#elif defined(OS_CHROMEOS)
  #if defined(__i386__)
    "os=cros&arch=x86&prod=chrome&prodversion=";
  #elif defined(__arm__)
    "os=cros&arch=arm&prod=chrome&prodversion=";
  #else
    "os=cros&arch=unknown&prod=chrome&prodversion=";
  #endif
#elif defined(OS_LINUX)
  #if defined(__amd64__)
    "os=linux&arch=x64&prod=chrome&prodversion=";
  #elif defined(__i386__)
    "os=linux&arch=x86&prod=chrome&prodversion=";
  #elif defined(__arm__)
    "os=linux&arch=arm&prod=chrome&prodversion=";
  #else
    "os=linux&arch=unknown&prod=chrome&prodversion=";
  #endif
#elif defined(OS_OPENBSD)
  #if defined(__amd64__)
    "os=openbsd&arch=x64";
  #elif defined(__i386__)
    "os=openbsd&arch=x86";
  #else
    "os=openbsd&arch=unknown";
  #endif
#else
    #error "unknown os or architecture"
#endif

}  // namespace

class ChromeConfigurator : public ComponentUpdateService::Configurator {
 public:
  ChromeConfigurator(const CommandLine* cmdline,
                     net::URLRequestContextGetter* url_request_getter);

  virtual ~ChromeConfigurator() {}

  virtual int InitialDelay() OVERRIDE;
  virtual int NextCheckDelay() OVERRIDE;
  virtual int StepDelay() OVERRIDE;
  virtual int MinimumReCheckWait() OVERRIDE;
  virtual GURL UpdateUrl(CrxComponent::UrlSource source) OVERRIDE;
  virtual const char* ExtraRequestParams() OVERRIDE;
  virtual size_t UrlSizeLimit() OVERRIDE;
  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE;
  virtual bool InProcess() OVERRIDE;
  virtual void OnEvent(Events event, int val) OVERRIDE;

 private:
  net::URLRequestContextGetter* url_request_getter_;
  std::string extra_info_;
  bool fast_update_;
  bool out_of_process_;
};

ChromeConfigurator::ChromeConfigurator(const CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
      : url_request_getter_(url_request_getter),
        extra_info_(kExtraInfo) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> debug_values;
  Tokenize(cmdline->GetSwitchValueASCII(switches::kComponentUpdaterDebug),
      ",", &debug_values);
  fast_update_ = HasDebugValue(debug_values, kDebugFastUpdate);
  out_of_process_ = HasDebugValue(debug_values, kDebugOutOfProcess);

  // Make the extra request params, they are necessary so omaha does
  // not deliver components that are going to be rejected at install time.
  extra_info_ += chrome::VersionInfo().Version();
#if defined(OS_WIN)
  if (base::win::OSInfo::GetInstance()->wow64_status() ==
      base::win::OSInfo::WOW64_ENABLED)
    extra_info_ += "&wow64=1";
#endif
  if (HasDebugValue(debug_values, kDebugRequestParam))
    extra_info_ += "&testrequest=1";
}

int ChromeConfigurator::InitialDelay() {
  return  fast_update_ ? 1 : (6 * kDelayOneMinute);
}

int ChromeConfigurator::NextCheckDelay() {
  return fast_update_ ? 3 : (2 * kDelayOneHour);
}

int ChromeConfigurator::StepDelay() {
  return fast_update_ ? 1 : 4;
}

int ChromeConfigurator::MinimumReCheckWait() {
  return fast_update_ ? 30 : (6 * kDelayOneHour);
}

GURL ChromeConfigurator::UpdateUrl(CrxComponent::UrlSource source) {
  return GURL(kUrlSources[source]);
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

void ChromeConfigurator::OnEvent(Events event, int val) {
  switch (event) {
    case kManifestCheck:
      UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.ManifestCheck", val, 100);
      break;
    case kComponentUpdated:
      UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.ComponentUpdated", val, 100);
      break;
    case kManifestError:
      UMA_HISTOGRAM_COUNTS_100("ComponentUpdater.ManifestError", val);
      break;
    case kNetworkError:
      UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.NetworkError", val, 100);
      break;
    case kUnpackError:
      UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.UnpackError", val, 100);
      break;
    case kInstallerError:
      UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.InstallError", val, 100);
      break;
    default:
      NOTREACHED();
      break;
  }
}

ComponentUpdateService::Configurator* MakeChromeComponentUpdaterConfigurator(
    const CommandLine* cmdline, net::URLRequestContextGetter* context_getter) {
  return new ChromeConfigurator(cmdline, context_getter);
}
