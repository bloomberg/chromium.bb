// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TEST_TEST_CONFIGURATOR_H_
#define COMPONENTS_UPDATE_CLIENT_TEST_TEST_CONFIGURATOR_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"
#include "net/url_request/url_request_test_util.h"

class GURL;

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace update_client {

#define POST_INTERCEPT_SCHEME "https"
#define POST_INTERCEPT_HOSTNAME "localhost2"
#define POST_INTERCEPT_PATH "/update2"

struct CrxComponent;

// component 1 has extension id "jebgalgnebhfojomionfpkfelancnnkf", and
// the RSA public key the following hash:
const uint8_t jebg_hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                             0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                             0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                             0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};
// component 2 has extension id "abagagagagagagagagagagagagagagag", and
// the RSA public key the following hash:
const uint8_t abag_hash[] = {0x01, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x01};
// component 3 has extension id "ihfokbkgjpifnbbojhneepfflplebdkc", and
// the RSA public key the following hash:
const uint8_t ihfo_hash[] = {0x87, 0x5e, 0xa1, 0xa6, 0x9f, 0x85, 0xd1, 0x1e,
                             0x97, 0xd4, 0x4f, 0x55, 0xbf, 0xb4, 0x13, 0xa2,
                             0xe7, 0xc5, 0xc8, 0xf5, 0x60, 0x19, 0x78, 0x1b,
                             0x6d, 0xe9, 0x4c, 0xeb, 0x96, 0x05, 0x42, 0x17};

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

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TEST_TEST_CONFIGURATOR_H_
