// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/testing_application_context.h"

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "components/network_time/network_time_tracker.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

TestingApplicationContext::TestingApplicationContext()
    : application_locale_("en"),
      local_state_(nullptr),
      chrome_browser_state_manager_(nullptr),
      was_last_shutdown_clean_(false) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);
}

TestingApplicationContext::~TestingApplicationContext() {
  DCHECK_EQ(this, GetApplicationContext());
  SetApplicationContext(nullptr);
}

// static
TestingApplicationContext* TestingApplicationContext::GetGlobal() {
  return static_cast<TestingApplicationContext*>(GetApplicationContext());
}

void TestingApplicationContext::SetLocalState(PrefService* local_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  local_state_ = local_state;
}

void TestingApplicationContext::SetLastShutdownClean(bool clean) {
  was_last_shutdown_clean_ = clean;
}

void TestingApplicationContext::SetChromeBrowserStateManager(
    ios::ChromeBrowserStateManager* manager) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chrome_browser_state_manager_ = manager;
}

void TestingApplicationContext::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestingApplicationContext::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool TestingApplicationContext::WasLastShutdownClean() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return was_last_shutdown_clean_;
}

PrefService* TestingApplicationContext::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_state_;
}

net::URLRequestContextGetter*
TestingApplicationContext::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios::GetChromeBrowserProvider()->GetSystemURLRequestContext();
}

const std::string& TestingApplicationContext::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

ios::ChromeBrowserStateManager*
TestingApplicationContext::GetChromeBrowserStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return chrome_browser_state_manager_;
}

metrics::MetricsService* TestingApplicationContext::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

variations::VariationsService*
TestingApplicationContext::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

policy::BrowserPolicyConnector*
TestingApplicationContext::GetBrowserPolicyConnector() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

policy::PolicyService* TestingApplicationContext::GetPolicyService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

rappor::RapporService* TestingApplicationContext::GetRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

net_log::ChromeNetLog* TestingApplicationContext::GetNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

network_time::NetworkTimeTracker*
TestingApplicationContext::GetNetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_time_tracker_) {
    DCHECK(local_state_);
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        make_scoped_ptr(new base::DefaultTickClock), local_state_));
  }
  return network_time_tracker_.get();
}
