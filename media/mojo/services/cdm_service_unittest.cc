// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/media_buildflags.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/services/cdm_service.h"
#include "media/mojo/services/media_interface_provider.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace media {

namespace {

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kInvalidKeySystem[] = "invalid.key.system";
const char kSecurityOrigin[] = "https://foo.com";

class MockCdmServiceClient : public media::CdmService::Client {
 public:
  MockCdmServiceClient() = default;
  ~MockCdmServiceClient() override = default;

  // media::CdmService::Client implementation.
  MOCK_METHOD0(EnsureSandboxed, void());

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return std::make_unique<media::DefaultCdmFactory>();
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void OnMetadata(std::vector<media::CdmHostFilePath>*) override {}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
};

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  explicit ServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::BindRepeating(&ServiceTestClient::Create,
                            base::Unretained(this)));
  }
  ~ServiceTestClient() override {}

  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // service_manager::mojom::ServiceFactory implementation.
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    if (name != mojom::kCdmServiceName)
      return;

    auto mock_cdm_service_client = std::make_unique<MockCdmServiceClient>();
    mock_cdm_service_client_ = mock_cdm_service_client.get();
    service_context_ = std::make_unique<service_manager::ServiceContext>(
        std::make_unique<CdmService>(std::move(mock_cdm_service_client)),
        std::move(request));
  }

  void DestroyService() { service_context_.reset(); }

  MockCdmServiceClient* mock_cdm_service_client() {
    return mock_cdm_service_client_;
  }

 private:
  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> service_context_;
  MockCdmServiceClient* mock_cdm_service_client_;
};

}  // namespace

class CdmServiceTest : public service_manager::test::ServiceTest {
 public:
  CdmServiceTest() : ServiceTest("cdm_service_unittest") {}
  ~CdmServiceTest() override {}

  MOCK_METHOD0(CdmFactoryConnectionClosed, void());
  MOCK_METHOD0(CdmConnectionClosed, void());

  // service_manager::test::ServiceTest:
  void SetUp() override {
    ServiceTest::SetUp();

    connector()->BindInterface(media::mojom::kCdmServiceName, &cdm_service_);

    service_manager::mojom::InterfaceProviderPtr interfaces;
    auto provider = std::make_unique<MediaInterfaceProvider>(
        mojo::MakeRequest(&interfaces));

    ASSERT_FALSE(cdm_factory_);
    cdm_service_->CreateCdmFactory(mojo::MakeRequest(&cdm_factory_),
                                   std::move(interfaces));
    cdm_service_.FlushForTesting();
    ASSERT_TRUE(cdm_factory_);
    cdm_factory_.set_connection_error_handler(base::BindRepeating(
        &CdmServiceTest::CdmFactoryConnectionClosed, base::Unretained(this)));
  }

  // MOCK_METHOD* doesn't support move-only types. Work around this by having
  // an extra method.
  MOCK_METHOD1(OnCdmInitializedInternal, void(bool result));
  void OnCdmInitialized(mojom::CdmPromiseResultPtr result,
                        int cdm_id,
                        mojom::DecryptorPtr decryptor) {
    OnCdmInitializedInternal(result->success);
  }

  void InitializeCdm(const std::string& key_system, bool expected_result) {
    base::RunLoop run_loop;
    cdm_factory_->CreateCdm(key_system, mojo::MakeRequest(&cdm_));
    cdm_.set_connection_error_handler(base::BindRepeating(
        &CdmServiceTest::CdmConnectionClosed, base::Unretained(this)));
    EXPECT_CALL(*this, OnCdmInitializedInternal(expected_result))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    cdm_->Initialize(key_system, url::Origin::Create(GURL(kSecurityOrigin)),
                     CdmConfig(),
                     base::BindRepeating(&CdmServiceTest::OnCdmInitialized,
                                         base::Unretained(this)));
    run_loop.Run();
  }

  std::unique_ptr<service_manager::Service> CreateService() override {
    auto service_test_client = std::make_unique<ServiceTestClient>(this);
    service_test_client_ = service_test_client.get();
    return service_test_client;
  }

  mojom::CdmServicePtr cdm_service_;
  mojom::CdmFactoryPtr cdm_factory_;
  mojom::ContentDecryptionModulePtr cdm_;
  ServiceTestClient* service_test_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmServiceTest);
};

TEST_F(CdmServiceTest, LoadCdm) {
  base::FilePath cdm_path(FILE_PATH_LITERAL("dummy path"));

  // Even with a dummy path where the CDM cannot be loaded, EnsureSandboxed()
  // should still be called to ensure the process is sandboxed.
  EXPECT_CALL(*service_test_client_->mock_cdm_service_client(),
              EnsureSandboxed());

  cdm_service_->LoadCdm(cdm_path);
  cdm_service_.FlushForTesting();
}

TEST_F(CdmServiceTest, InitializeCdm_Success) {
  InitializeCdm(kClearKeyKeySystem, true);
}

TEST_F(CdmServiceTest, InitializeCdm_InvalidKeySystem) {
  InitializeCdm(kInvalidKeySystem, false);
}

TEST_F(CdmServiceTest, DestroyAndRecreateCdm) {
  InitializeCdm(kClearKeyKeySystem, true);
  cdm_.reset();
  InitializeCdm(kClearKeyKeySystem, true);
}

// CdmFactory connection error will destroy all CDMs.
TEST_F(CdmServiceTest, DestroyCdmFactory) {
  InitializeCdm(kClearKeyKeySystem, true);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, CdmConnectionClosed())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  cdm_factory_.reset();
  run_loop.Run();
}

// Destroy service will destroy the CdmFactory and all CDMs.
TEST_F(CdmServiceTest, DestroyCdmService) {
  InitializeCdm(kClearKeyKeySystem, true);

  base::RunLoop run_loop;
  // Ideally we should not care about order, and should only quit the loop when
  // both connections are closed.
  EXPECT_CALL(*this, CdmFactoryConnectionClosed());
  EXPECT_CALL(*this, CdmConnectionClosed())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  service_test_client_->DestroyService();
  run_loop.Run();
}

}  // namespace media
