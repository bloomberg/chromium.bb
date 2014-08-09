// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_IOS)
#include "ios/public/test/fake_profile_oauth2_token_service_ios_provider.h"
#endif

namespace {

// Helper for testing.
const int kInvalidProcessId = -1;
}

TestSigninClient::TestSigninClient()
    : request_context_(new net::TestURLRequestContextGetter(
          base::MessageLoopProxy::current())),
      pref_service_(NULL) {
  LoadDatabase();
}

TestSigninClient::TestSigninClient(PrefService* pref_service)
    : pref_service_(pref_service) {}

TestSigninClient::~TestSigninClient() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
}

scoped_refptr<TokenWebData> TestSigninClient::GetDatabase() {
  return database_;
}

bool TestSigninClient::CanRevokeCredentials() { return true; }

std::string TestSigninClient::GetSigninScopedDeviceId() {
  return std::string();
}

void TestSigninClient::ClearSigninScopedDeviceId() {
}

net::URLRequestContextGetter* TestSigninClient::GetURLRequestContext() {
  return request_context_;
}

std::string TestSigninClient::GetProductVersion() { return ""; }

void TestSigninClient::LoadDatabase() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.path().AppendASCII("TestWebDB");
  scoped_refptr<WebDatabaseService> web_database =
      new WebDatabaseService(path,
                             base::MessageLoopProxy::current(),
                             base::MessageLoopProxy::current());
  web_database->AddTable(scoped_ptr<WebDatabaseTable>(new TokenServiceTable()));
  web_database->LoadDatabase();
  database_ = new TokenWebData(web_database,
                               base::MessageLoopProxy::current(),
                               base::MessageLoopProxy::current(),
                               WebDataServiceBase::ProfileErrorCallback());
  database_->Init();
}

bool TestSigninClient::ShouldMergeSigninCredentialsIntoCookieJar() {
  return true;
}

scoped_ptr<SigninClient::CookieChangedCallbackList::Subscription>
TestSigninClient::AddCookieChangedCallback(
    const SigninClient::CookieChangedCallback& callback) {
  return cookie_callbacks_.Add(callback);
}

#if defined(OS_IOS)
ios::ProfileOAuth2TokenServiceIOSProvider* TestSigninClient::GetIOSProvider() {
  return GetIOSProviderAsFake();
}

ios::FakeProfileOAuth2TokenServiceIOSProvider*
TestSigninClient::GetIOSProviderAsFake() {
  if (!iosProvider_) {
    iosProvider_.reset(new ios::FakeProfileOAuth2TokenServiceIOSProvider());
  }
  return iosProvider_.get();
}
#endif

void TestSigninClient::SetSigninProcess(int process_id) {
  if (process_id == signin_host_id_)
    return;
  DLOG_IF(WARNING, signin_host_id_ != kInvalidProcessId)
      << "Replacing in-use signin process.";
  signin_host_id_ = process_id;
}

void TestSigninClient::ClearSigninProcess() {
  signin_host_id_ = kInvalidProcessId;
}

bool TestSigninClient::IsSigninProcess(int process_id) const {
  return process_id == signin_host_id_;
}

bool TestSigninClient::HasSigninProcess() const {
  return signin_host_id_ != kInvalidProcessId;
}
