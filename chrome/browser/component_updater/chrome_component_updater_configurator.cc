// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"

#include <string>
#include <vector>

#include "base/threading/sequenced_worker_pool.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/component_updater/configurator_impl.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

class ChromeConfigurator : public update_client::Configurator {
 public:
  ChromeConfigurator(const base::CommandLine* cmdline,
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
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const override;
  bool DeltasEnabled() const override;
  bool UseBackgroundDownloader() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;

 private:
  friend class base::RefCountedThreadSafe<ChromeConfigurator>;

  ConfiguratorImpl configurator_impl_;

  ~ChromeConfigurator() override {}
};

ChromeConfigurator::ChromeConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
    : configurator_impl_(cmdline, url_request_getter) {}

int ChromeConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int ChromeConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int ChromeConfigurator::StepDelay() const {
  return configurator_impl_.StepDelay();
}

int ChromeConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int ChromeConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> ChromeConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> ChromeConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

base::Version ChromeConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string ChromeConfigurator::GetChannel() const {
  return chrome::GetChannelString();
}

std::string ChromeConfigurator::GetLang() const {
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

std::string ChromeConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

net::URLRequestContextGetter* ChromeConfigurator::RequestContext() const {
  return configurator_impl_.RequestContext();
}

scoped_refptr<update_client::OutOfProcessPatcher>
ChromeConfigurator::CreateOutOfProcessPatcher() const {
  return make_scoped_refptr(new ChromeOutOfProcessPatcher);
}

bool ChromeConfigurator::DeltasEnabled() const {
  return configurator_impl_.DeltasEnabled();
}

bool ChromeConfigurator::UseBackgroundDownloader() const {
  return configurator_impl_.UseBackgroundDownloader();
}

// Returns a task runner to run blocking tasks. The task runner continues to run
// after the browser shuts down, until the OS terminates the process. This
// imposes certain requirements for the code using the task runner, such as
// not accessing any global browser state while the code is running.
scoped_refptr<base::SequencedTaskRunner>
ChromeConfigurator::GetSequencedTaskRunner() const {
  return content::BrowserThread::GetBlockingPool()
      ->GetSequencedTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::GetSequenceToken(),
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

}  // namespace

scoped_refptr<update_client::Configurator>
MakeChromeComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter) {
  return new ChromeConfigurator(cmdline, context_getter);
}

}  // namespace component_updater
