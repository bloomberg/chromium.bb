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
