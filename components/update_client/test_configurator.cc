// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_configurator.h"

#include "base/version.h"
#include "components/update_client/component_patcher_operation.h"
#include "url/gurl.h"

namespace update_client {

namespace {

std::vector<GURL> MakeDefaultUrls() {
  std::vector<GURL> urls;
  urls.push_back(GURL(POST_INTERCEPT_SCHEME
                      "://" POST_INTERCEPT_HOSTNAME POST_INTERCEPT_PATH));
  return urls;
}

}  // namespace

TestConfigurator::TestConfigurator(
    const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : worker_task_runner_(worker_task_runner),
      initial_time_(0),
      ondemand_time_(0),
      context_(new net::TestURLRequestContextGetter(network_task_runner)) {
}

TestConfigurator::~TestConfigurator() {
}

int TestConfigurator::InitialDelay() const {
  return initial_time_;
}

int TestConfigurator::NextCheckDelay() const {
  return 1;
}

int TestConfigurator::StepDelay() const {
  return 0;
}

int TestConfigurator::OnDemandDelay() const {
  return ondemand_time_;
}

int TestConfigurator::UpdateDelay() const {
  return 1;
}

std::vector<GURL> TestConfigurator::UpdateUrl() const {
  return MakeDefaultUrls();
}

std::vector<GURL> TestConfigurator::PingUrl() const {
  return UpdateUrl();
}

base::Version TestConfigurator::GetBrowserVersion() const {
  // Needs to be larger than the required version in tested component manifests.
  return base::Version("30.0");
}

std::string TestConfigurator::GetChannel() const {
  return "fake_channel_string";
}

std::string TestConfigurator::GetLang() const {
  return "fake_lang";
}

std::string TestConfigurator::GetOSLongName() const {
  return "Fake Operating System";
}

std::string TestConfigurator::ExtraRequestParams() const {
  return "extra=\"foo\"";
}

std::string TestConfigurator::GetDownloadPreference() const {
  return download_preference_;
}

net::URLRequestContextGetter* TestConfigurator::RequestContext() const {
  return context_.get();
}

scoped_refptr<OutOfProcessPatcher> TestConfigurator::CreateOutOfProcessPatcher()
    const {
  return NULL;
}

bool TestConfigurator::DeltasEnabled() const {
  return true;
}

bool TestConfigurator::UseBackgroundDownloader() const {
  return false;
}

void TestConfigurator::SetOnDemandTime(int seconds) {
  ondemand_time_ = seconds;
}

void TestConfigurator::SetInitialDelay(int seconds) {
  initial_time_ = seconds;
}

void TestConfigurator::SetDownloadPreference(
    const std::string& download_preference) {
  download_preference_ = download_preference;
}

scoped_refptr<base::SequencedTaskRunner>
TestConfigurator::GetSequencedTaskRunner() const {
  DCHECK(worker_task_runner_.get());
  return worker_task_runner_;
}

}  // namespace update_client
