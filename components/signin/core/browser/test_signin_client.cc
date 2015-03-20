// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_signin_client.h"

#include "base/logging.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_IOS)
#include "ios/public/test/fake_profile_oauth2_token_service_ios_provider.h"
#endif

TestSigninClient::TestSigninClient()
    : request_context_(new net::TestURLRequestContextGetter(
          base::MessageLoopProxy::current())),
      pref_service_(NULL),
      are_signin_cookies_allowed_(true) {
  LoadDatabase();
}

TestSigninClient::TestSigninClient(PrefService* pref_service)
    : pref_service_(pref_service),
      are_signin_cookies_allowed_(true) {}

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

void TestSigninClient::OnSignedOut() {}

void TestSigninClient::PostSignedIn(const std::string& account_id,
                  const std::string& username,
                  const std::string& password) {
  signed_in_password_ = password;
}

net::URLRequestContextGetter* TestSigninClient::GetURLRequestContext() {
  return request_context_.get();
}

void TestSigninClient::SetURLRequestContext(
    net::URLRequestContextGetter* request_context) {
  request_context_ = request_context;
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

scoped_ptr<SigninClient::CookieChangedSubscription>
TestSigninClient::AddCookieChangedCallback(
    const GURL& url,
    const std::string& name,
    const net::CookieStore::CookieChangedCallback& callback) {
  return scoped_ptr<SigninClient::CookieChangedSubscription>(
      new SigninClient::CookieChangedSubscription);
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

bool TestSigninClient::IsFirstRun() const {
  return false;
}

base::Time TestSigninClient::GetInstallDate() {
  return base::Time::Now();
}

bool TestSigninClient::AreSigninCookiesAllowed() {
  return are_signin_cookies_allowed_;
}

void TestSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
}
