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
  ~TestConfigurator() override;

  // Overrrides for Configurator.
  int InitialDelay() const override;
  int NextCheckDelay() override;
  int StepDelay() const override;
  int StepDelayMedium() override;
  int MinimumReCheckWait() const override;
  int OnDemandDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  size_t UrlSizeLimit() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<OutOfProcessPatcher> CreateOutOfProcessPatcher() const override;
  bool DeltasEnabled() const override;
  bool UseBackgroundDownloader() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetSingleThreadTaskRunner()
      const override;

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
