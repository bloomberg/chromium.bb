// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_signin_client.h"

#include <memory>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

TestSigninClient::TestSigninClient(PrefService* pref_service)
    : shared_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      pref_service_(pref_service),
      are_signin_cookies_allowed_(true),
      network_calls_delayed_(false) {}

TestSigninClient::~TestSigninClient() {}

void TestSigninClient::DoFinalInit() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
}

scoped_refptr<TokenWebData> TestSigninClient::GetDatabase() {
  return database_;
}

bool TestSigninClient::CanRevokeCredentials() { return true; }

std::string TestSigninClient::GetSigninScopedDeviceId() {
  return "DeviceID";
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

scoped_refptr<network::SharedURLLoaderFactory>
TestSigninClient::GetURLLoaderFactory() {
  return shared_factory_;
}

void TestSigninClient::SetURLRequestContext(
    net::URLRequestContextGetter* request_context) {
  request_context_ = request_context;
}

std::string TestSigninClient::GetProductVersion() { return ""; }

void TestSigninClient::LoadTokenDatabase() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.GetPath().AppendASCII("TestWebDB");
  scoped_refptr<WebDatabaseService> web_database =
      new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get());
  web_database->AddTable(std::make_unique<TokenServiceTable>());
  web_database->LoadDatabase();
  database_ =
      new TokenWebData(web_database, base::ThreadTaskRunnerHandle::Get(),
                       base::ThreadTaskRunnerHandle::Get(),
                       WebDataServiceBase::ProfileErrorCallback());
  database_->Init();
}

std::unique_ptr<SigninClient::CookieChangeSubscription>
TestSigninClient::AddCookieChangeCallback(const GURL& url,
                                          const std::string& name,
                                          net::CookieChangeCallback callback) {
  return std::make_unique<SigninClient::CookieChangeSubscription>();
}

void TestSigninClient::SetNetworkCallsDelayed(bool value) {
  network_calls_delayed_ = value;

  if (!network_calls_delayed_) {
    for (base::OnceClosure& call : delayed_network_calls_)
      std::move(call).Run();
    delayed_network_calls_.clear();
  }
}

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

void TestSigninClient::DelayNetworkCall(const base::Closure& callback) {
  if (network_calls_delayed_) {
    delayed_network_calls_.push_back(callback);
  } else {
    callback.Run();
  }
}

std::unique_ptr<GaiaAuthFetcher> TestSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           url_loader_factory);
}

void TestSigninClient::PreGaiaLogout(base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}
