// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_auth.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_accessor.mojom-test-utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

class AuthDelegateImpl : public DriveFsAuth::Delegate {
 public:
  AuthDelegateImpl(std::unique_ptr<service_manager::Connector> connector,
                   const AccountId& account_id)
      : connector_(std::move(connector)), account_id_(account_id) {}

  ~AuthDelegateImpl() override {}

 private:
  // AuthDelegate::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  service_manager::Connector* GetConnector() override {
    return connector_.get();
  }
  const AccountId& GetAccountId() override { return account_id_; }
  std::string GetObfuscatedAccountId() override {
    return "salt-" + account_id_.GetAccountIdKey();
  }

  const std::unique_ptr<service_manager::Connector> connector_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(AuthDelegateImpl);
};

class MockIdentityAccessor {
 public:
  MOCK_METHOD3(
      GetAccessToken,
      std::pair<base::Optional<std::string>, GoogleServiceAuthError::State>(
          const std::string& account_id,
          const ::identity::ScopeSet& scopes,
          const std::string& consumer_id));

  mojo::BindingSet<identity::mojom::IdentityAccessor>* bindings_ = nullptr;
};

class FakeIdentityService
    : public identity::mojom::IdentityAccessorInterceptorForTesting,
      public service_manager::Service {
 public:
  explicit FakeIdentityService(MockIdentityAccessor* mock,
                               const base::Clock* clock,
                               service_manager::mojom::ServiceRequest request)
      : mock_(mock), clock_(clock), binding_(this, std::move(request)) {
    binder_registry_.AddInterface(
        base::BindRepeating(&FakeIdentityService::BindIdentityAccessorRequest,
                            base::Unretained(this)));
    mock_->bindings_ = &bindings_;
  }

  ~FakeIdentityService() override { mock_->bindings_ = nullptr; }

 private:
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    binder_registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void BindIdentityAccessorRequest(
      identity::mojom::IdentityAccessorRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // identity::mojom::IdentityAccessorInterceptorForTesting overrides:
  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override {
    auto account_id = AccountId::FromUserEmailGaiaId("test@example.com", "ID");
    AccountInfo account_info;
    account_info.email = account_id.GetUserEmail();
    account_info.gaia = account_id.GetGaiaId();
    account_info.account_id = account_id.GetAccountIdKey();
    std::move(callback).Run(account_info, {});
  }

  void GetAccessToken(const std::string& account_id,
                      const ::identity::ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override {
    auto result = mock_->GetAccessToken(account_id, scopes, consumer_id);
    std::move(callback).Run(std::move(result.first),
                            clock_->Now() + base::TimeDelta::FromSeconds(1),
                            GoogleServiceAuthError(result.second));
  }

  IdentityAccessor* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MockIdentityAccessor* const mock_;
  const base::Clock* const clock_;
  service_manager::ServiceBinding binding_;
  service_manager::BinderRegistry binder_registry_;
  mojo::BindingSet<identity::mojom::IdentityAccessor> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeIdentityService);
};

class DriveFsAuthTest : public ::testing::Test {
 public:
  DriveFsAuthTest() = default;

 protected:
  void SetUp() override {
    account_id_ = AccountId::FromUserEmailGaiaId("test@example.com", "ID");
    clock_.SetNow(base::Time::Now());
    identity_service_ = std::make_unique<FakeIdentityService>(
        &mock_identity_accessor_, &clock_,
        connector_factory_.RegisterInstance(identity::mojom::kServiceName));
    delegate_ = std::make_unique<AuthDelegateImpl>(
        connector_factory_.CreateConnector(), account_id_);
    auth_ = std::make_unique<DriveFsAuth>(
        &clock_, base::FilePath("/path/to/profile"), delegate_.get());
  }

  void TearDown() override { auth_.reset(); }

  void ExpectAccessToken(bool use_cached,
                         mojom::AccessTokenStatus expected_status,
                         const std::string& expected_token) {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auth_->GetAccessToken(use_cached, base::BindLambdaForTesting(
                                          [&](mojom::AccessTokenStatus status,
                                              const std::string& token) {
                                            EXPECT_EQ(expected_status, status);
                                            EXPECT_EQ(expected_token, token);
                                            std::move(quit_closure).Run();
                                          }));
    run_loop.Run();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestConnectorFactory connector_factory_;
  MockIdentityAccessor mock_identity_accessor_;
  base::SimpleTestClock clock_;
  std::unique_ptr<FakeIdentityService> identity_service_;

  AccountId account_id_;

  std::unique_ptr<AuthDelegateImpl> delegate_;
  std::unique_ptr<DriveFsAuth> auth_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsAuthTest);
};

TEST_F(DriveFsAuthTest, GetAccessToken_Success) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));
  ExpectAccessToken(false, mojom::AccessTokenStatus::kSuccess, "auth token");
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Permanent) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(std::make_pair(
          base::nullopt, GoogleServiceAuthError::ACCOUNT_DISABLED)));
  ExpectAccessToken(false, mojom::AccessTokenStatus::kAuthError, "");
}

TEST_F(DriveFsAuthTest, GetAccessToken_GetAccessTokenFailure_Transient) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(std::make_pair(
          base::nullopt, GoogleServiceAuthError::SERVICE_UNAVAILABLE)));
  ExpectAccessToken(false, mojom::AccessTokenStatus::kTransientError, "");
}

TEST_F(DriveFsAuthTest, GetAccessToken_ParallelRequests) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auth_->GetAccessToken(
      false, base::BindOnce(
                 [](mojom::AccessTokenStatus status, const std::string& token) {
                   FAIL() << "Unexpected callback";
                 }));
  auth_->GetAccessToken(
      false, base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                            const std::string& token) {
        EXPECT_EQ(mojom::AccessTokenStatus::kTransientError, status);
        EXPECT_TRUE(token.empty());
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
}

TEST_F(DriveFsAuthTest, GetAccessToken_SequentialRequests) {
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(mock_identity_accessor_,
                GetAccessToken("test@example.com", _, "drivefs"))
        .WillOnce(testing::Return(
            std::make_pair("auth token", GoogleServiceAuthError::NONE)));
    ExpectAccessToken(false, mojom::AccessTokenStatus::kSuccess, "auth token");
  }
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(mock_identity_accessor_,
                GetAccessToken("test@example.com", _, "drivefs"))
        .WillOnce(testing::Return(std::make_pair(
            base::nullopt, GoogleServiceAuthError::ACCOUNT_DISABLED)));
    ExpectAccessToken(false, mojom::AccessTokenStatus::kAuthError, "");
  }
}

TEST_F(DriveFsAuthTest, Caching) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  // Second attempt should reuse already available token.
  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");
}

TEST_F(DriveFsAuthTest, CachedAndNotCached) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)))
      .WillOnce(testing::Return(
          std::make_pair("auth token 2", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  // Second attempt should reuse already available token.
  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  // Now ask for token explicitly bypassing the cache.
  ExpectAccessToken(false, mojom::AccessTokenStatus::kSuccess, "auth token 2");
}

TEST_F(DriveFsAuthTest, CacheExpired) {
  EXPECT_CALL(mock_identity_accessor_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)))
      .WillOnce(testing::Return(
          std::make_pair("auth token 2", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token");

  clock_.Advance(base::TimeDelta::FromHours(2));

  // As the token expired second mount attempt go to identity.
  ExpectAccessToken(true, mojom::AccessTokenStatus::kSuccess, "auth token 2");
}

}  // namespace
}  // namespace drivefs
