// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/testing_application_context.h"

#include "base/logging.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

TestingApplicationContext::TestingApplicationContext()
    : application_locale_("en"), local_state_(nullptr) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);
}

TestingApplicationContext::~TestingApplicationContext() {
  DCHECK_EQ(this, GetApplicationContext());
  SetApplicationContext(nullptr);
}

void TestingApplicationContext::SetLocalState(PrefService* local_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  local_state_ = local_state;
}

// static
TestingApplicationContext* TestingApplicationContext::GetGlobal() {
  return static_cast<TestingApplicationContext*>(GetApplicationContext());
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
