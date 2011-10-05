// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater-debug=<value>.
const char kDebugFastUpdate[] = "fast-update";
const char kDebugOutOfProcess[] = "out-of-process";

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
    "os=mac&arch=x64";
  #elif defined(__i386__)
     "os=mac&arch=x86";
  #else
     #error "unknown mac architecture"
  #endif
#elif defined(OS_WIN)
  #if defined(_WIN64)
    "os=win&arch=x64";
  #elif defined(_WIN32)
    "os=win&arch=x86";
  #else
    #error "unknown windows architecture"
  #endif
#elif defined(OS_LINUX)
  #if defined(__amd64__)
    "os=linux&arch=x64";
  #elif defined(__i386__)
    "os=linux&arch=x86";
  #else
    #error "unknown linux architecture"
  #endif
#elif defined(OS_CHROMEOS)
  #if defined(__i386__)
    "os=cros&arch=x86";
  #else
    #error "unknown chromeOs architecture"
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
  virtual GURL UpdateUrl() OVERRIDE;
  virtual const char* ExtraRequestParams() OVERRIDE;
  virtual size_t UrlSizeLimit() OVERRIDE;
  virtual net::URLRequestContextGetter* RequestContext() OVERRIDE;
  virtual bool InProcess() OVERRIDE;
  virtual void OnEvent(Events event, int val) OVERRIDE;

 private:
  net::URLRequestContextGetter* url_request_getter_;
  bool fast_update_;
  bool out_of_process_;
  std::string extra_info_;
};

ChromeConfigurator::ChromeConfigurator(const CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
      : url_request_getter_(url_request_getter) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> debug_values;
  Tokenize(cmdline->GetSwitchValueASCII(switches::kComponentUpdaterDebug),
      ",", &debug_values);
  fast_update_ = HasDebugValue(debug_values, kDebugFastUpdate);
  out_of_process_ = HasDebugValue(debug_values, kDebugOutOfProcess);
}

int ChromeConfigurator::InitialDelay() {
  return  fast_update_ ? 1 : (6 * kDelayOneMinute);
}

int ChromeConfigurator::NextCheckDelay() {
  return fast_update_ ? 3 : (4 * kDelayOneHour);
}

int ChromeConfigurator::StepDelay() {
  return fast_update_ ? 1 : 4;
}

int ChromeConfigurator::MinimumReCheckWait() {
  return fast_update_ ? 30 : (6 * kDelayOneHour);
}

GURL ChromeConfigurator::UpdateUrl() {
  return GURL("http://clients2.google.com/service/update2/crx");
}

const char* ChromeConfigurator::ExtraRequestParams() {
  return kExtraInfo;
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

