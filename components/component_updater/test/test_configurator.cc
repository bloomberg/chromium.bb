// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/test/test_configurator.h"

#include "base/run_loop.h"
#include "base/version.h"
#include "components/component_updater/component_patcher_operation.h"
#include "url/gurl.h"

namespace component_updater {

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
      times_(1),
      recheck_time_(0),
      ondemand_time_(0),
      context_(new net::TestURLRequestContextGetter(network_task_runner)) {
}

TestConfigurator::~TestConfigurator() {
}

int TestConfigurator::InitialDelay() const {
  return initial_time_;
}

int TestConfigurator::NextCheckDelay() {
  // This is called when a new full cycle of checking for updates is going
  // to happen. In test we normally only test one cycle so it is a good
  // time to break from the test messageloop Run() method so the test can
  // finish.
  if (--times_ <= 0) {
    quit_closure_.Run();
    return 0;
  }
  return 1;
}

int TestConfigurator::StepDelay() const {
  return 0;
}

int TestConfigurator::StepDelayMedium() {
  return NextCheckDelay();
}

int TestConfigurator::MinimumReCheckWait() const {
  return recheck_time_;
}

int TestConfigurator::OnDemandDelay() const {
  return ondemand_time_;
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

size_t TestConfigurator::UrlSizeLimit() const {
  return 256;
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

// Set how many update checks are called, the default value is just once.
void TestConfigurator::SetLoopCount(int times) {
  times_ = times;
}

void TestConfigurator::SetRecheckTime(int seconds) {
  recheck_time_ = seconds;
}

void TestConfigurator::SetOnDemandTime(int seconds) {
  ondemand_time_ = seconds;
}

void TestConfigurator::SetQuitClosure(const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void TestConfigurator::SetInitialDelay(int seconds) {
  initial_time_ = seconds;
}

scoped_refptr<base::SequencedTaskRunner>
TestConfigurator::GetSequencedTaskRunner() const {
  DCHECK(worker_task_runner_.get());
  return worker_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
TestConfigurator::GetSingleThreadTaskRunner() const {
  // This is NULL because tests do not use the background downloader.
  return NULL;
}

}  // namespace component_updater
