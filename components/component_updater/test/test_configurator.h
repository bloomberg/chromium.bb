// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_TEST_TEST_CONFIGURATOR_H_
#define COMPONENTS_COMPONENT_UPDATER_TEST_TEST_CONFIGURATOR_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/component_updater/component_updater_configurator.h"
#include "net/url_request/url_request_test_util.h"

class GURL;

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace component_updater {

#define POST_INTERCEPT_SCHEME "https"
#define POST_INTERCEPT_HOSTNAME "localhost2"
#define POST_INTERCEPT_PATH "/update2"

struct CrxComponent;

class TestConfigurator : public Configurator {
 public:
  TestConfigurator(
      const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);
  virtual ~TestConfigurator();

  // Overrrides for Configurator.
  virtual int InitialDelay() const override;
  virtual int NextCheckDelay() override;
  virtual int StepDelay() const override;
  virtual int StepDelayMedium() override;
  virtual int MinimumReCheckWait() const override;
  virtual int OnDemandDelay() const override;
  virtual std::vector<GURL> UpdateUrl() const override;
  virtual std::vector<GURL> PingUrl() const override;
  virtual base::Version GetBrowserVersion() const override;
  virtual std::string GetChannel() const override;
  virtual std::string GetLang() const override;
  virtual std::string GetOSLongName() const override;
  virtual std::string ExtraRequestParams() const override;
  virtual size_t UrlSizeLimit() const override;
  virtual net::URLRequestContextGetter* RequestContext() const override;
  virtual scoped_refptr<OutOfProcessPatcher> CreateOutOfProcessPatcher()
      const override;
  virtual bool DeltasEnabled() const override;
  virtual bool UseBackgroundDownloader() const override;
  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetSingleThreadTaskRunner() const override;

  void SetLoopCount(int times);
  void SetRecheckTime(int seconds);
  void SetOnDemandTime(int seconds);
  void SetQuitClosure(const base::Closure& quit_closure);
  void SetInitialDelay(int seconds);

 private:
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  int initial_time_;
  int times_;
  int recheck_time_;
  int ondemand_time_;

  scoped_refptr<net::TestURLRequestContextGetter> context_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestConfigurator);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_TEST_TEST_CONFIGURATOR_H_
