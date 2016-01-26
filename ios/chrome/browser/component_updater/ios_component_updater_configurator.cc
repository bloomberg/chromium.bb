// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/component_updater/ios_component_updater_configurator.h"

#include <string>

#include "base/threading/sequenced_worker_pool.h"
#include "base/version.h"
#include "components/component_updater/configurator_impl.h"
#include "components/update_client/component_patcher_operation.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"

namespace component_updater {

namespace {

class IOSConfigurator : public update_client::Configurator {
 public:
  IOSConfigurator(const base::CommandLine* cmdline,
                  net::URLRequestContextGetter* url_request_getter);

  // update_client::Configurator overrides.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int StepDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const override;
  bool DeltasEnabled() const override;
  bool UseBackgroundDownloader() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;

 private:
  friend class base::RefCountedThreadSafe<IOSConfigurator>;

  ConfiguratorImpl configurator_impl_;

  ~IOSConfigurator() override {}
};

IOSConfigurator::IOSConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
    : configurator_impl_(cmdline, url_request_getter) {}

int IOSConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int IOSConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int IOSConfigurator::StepDelay() const {
  return configurator_impl_.StepDelay();
}

int IOSConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int IOSConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> IOSConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> IOSConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

base::Version IOSConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string IOSConfigurator::GetChannel() const {
  return GetChannelString();
}

std::string IOSConfigurator::GetLang() const {
  return GetApplicationContext()->GetApplicationLocale();
}

std::string IOSConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

std::string IOSConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string IOSConfigurator::GetDownloadPreference() const {
  return configurator_impl_.GetDownloadPreference();
}

net::URLRequestContextGetter* IOSConfigurator::RequestContext() const {
  return configurator_impl_.RequestContext();
}

scoped_refptr<update_client::OutOfProcessPatcher>
IOSConfigurator::CreateOutOfProcessPatcher() const {
  return nullptr;
}

bool IOSConfigurator::DeltasEnabled() const {
  return configurator_impl_.DeltasEnabled();
}

bool IOSConfigurator::UseBackgroundDownloader() const {
  return configurator_impl_.UseBackgroundDownloader();
}

scoped_refptr<base::SequencedTaskRunner>
IOSConfigurator::GetSequencedTaskRunner() const {
  return web::WebThread::GetBlockingPool()
      ->GetSequencedTaskRunnerWithShutdownBehavior(
          web::WebThread::GetBlockingPool()->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

}  // namespace

scoped_refptr<update_client::Configurator> MakeIOSComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter) {
  return new IOSConfigurator(cmdline, context_getter);
}

}  // namespace component_updater
