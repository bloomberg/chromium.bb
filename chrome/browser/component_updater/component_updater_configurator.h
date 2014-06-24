// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_CONFIGURATOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_CONFIGURATOR_H_

#include <string>

#include "chrome/browser/component_updater/component_updater_service.h"

namespace base {
class CommandLine;
}

namespace net {
class URLRequestContextGetter;
}

namespace component_updater {

// Controls the component updater behavior.
class Configurator {
 public:
  virtual ~Configurator() {}
  // Delay in seconds from calling Start() to the first update check.
  virtual int InitialDelay() = 0;
  // Delay in seconds to every subsequent update check. 0 means don't check.
  virtual int NextCheckDelay() = 0;
  // Delay in seconds from each task step. Used to smooth out CPU/IO usage.
  virtual int StepDelay() = 0;
  // Delay in seconds between applying updates for different components, if
  // several updates are available at a given time.
  virtual int StepDelayMedium() = 0;
  // Minimum delta time in seconds before checking again the same component.
  virtual int MinimumReCheckWait() = 0;
  // Minimum delta time in seconds before an on-demand check is allowed
  // for the same component.
  virtual int OnDemandDelay() = 0;
  // The url that is going to be used update checks over Omaha protocol.
  virtual GURL UpdateUrl() = 0;
  // The url where the completion pings are sent. Invalid if and only if
  // pings are disabled.
  virtual GURL PingUrl() = 0;
  // Parameters added to each url request. It can be null if none are needed.
  virtual std::string ExtraRequestParams() = 0;
  // How big each update request can be. Don't go above 2000.
  virtual size_t UrlSizeLimit() = 0;
  // The source of contexts for all the url requests.
  virtual net::URLRequestContextGetter* RequestContext() = 0;
  // True means that all ops are performed in this process.
  virtual bool InProcess() = 0;
  // True means that this client can handle delta updates.
  virtual bool DeltasEnabled() const = 0;
  // True means that the background downloader can be used for downloading
  // non on-demand components.
  virtual bool UseBackgroundDownloader() const = 0;
};

Configurator* MakeChromeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_CONFIGURATOR_H_
