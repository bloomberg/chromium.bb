// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/shared_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "components/printing/service/pdf_compositor_service.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// In order to test PdfCompositorService, this class overrides PrepareToStart()
// to do nothing. So the test discardable memory allocator set up by
// PdfCompositorServiceTest will be used.
class PdfCompositorTestService : public printing::PdfCompositorService {
 public:
  explicit PdfCompositorTestService(const std::string& creator)
      : PdfCompositorService(creator) {}
  ~PdfCompositorTestService() override {}

  // PdfCompositorService:
  void PrepareToStart() override {}
};

class PdfServiceTestClient : public service_manager::test::ServiceTestClient,
                             public service_manager::mojom::ServiceFactory {
 public:
  explicit PdfServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::Bind(&PdfServiceTestClient::Create, base::Unretained(this)));
  }
  ~PdfServiceTestClient() override {}

  // service_manager::Service
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // service_manager::mojom::ServiceFactory
  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (!name.compare(mojom::kServiceName)) {
      service_context_ = std::make_unique<service_manager::ServiceContext>(
          std::make_unique<PdfCompositorTestService>("pdf_compositor_unittest"),
          std::move(request));
    }
  }

  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> service_context_;
};

class PdfCompositorServiceTest : public service_manager::test::ServiceTest {
 public:
  PdfCompositorServiceTest() : ServiceTest("pdf_compositor_service_unittest") {}
  ~PdfCompositorServiceTest() override {}

  MOCK_METHOD1(CallbackOnSuccess, void(mojo::SharedBufferHandle));
  MOCK_METHOD1(CallbackOnError, void(mojom::PdfCompositor::Status));
  void OnCallback(mojom::PdfCompositor::Status status,
                  mojo::ScopedSharedBufferHandle handle) {
    if (status == mojom::PdfCompositor::Status::SUCCESS)
      CallbackOnSuccess(handle.get());
    else
      CallbackOnError(status);
    run_loop_->Quit();
  }

  MOCK_METHOD0(ConnectionClosed, void());

 protected:
  // service_manager::test::ServiceTest:
  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    ServiceTest::SetUp();

    ASSERT_FALSE(compositor_);
    connector()->BindInterface(mojom::kServiceName, &compositor_);
    ASSERT_TRUE(compositor_);

    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override {
    // Clean up
    compositor_.reset();
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  std::unique_ptr<service_manager::Service> CreateService() override {
    return std::make_unique<PdfServiceTestClient>(this);
  }

  base::SharedMemoryHandle LoadFileInSharedMemory() {
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    base::FilePath test_file =
        path.AppendASCII("components/test/data/printing/google.mskp");
    std::string content;
    base::SharedMemoryHandle invalid_handle;
    if (!base::ReadFileToString(test_file, &content))
      return invalid_handle;
    size_t len = content.size();
    base::SharedMemoryCreateOptions options;
    options.size = len;
    options.share_read_only = true;
    base::SharedMemory shared_memory;
    if (shared_memory.Create(options) && shared_memory.Map(len)) {
      memcpy(shared_memory.memory(), content.data(), len);
      return base::SharedMemory::DuplicateHandle(shared_memory.handle());
    }
    return invalid_handle;
  }

  void CallCompositorWithSuccess(mojom::PdfCompositorPtr ptr) {
    auto handle = LoadFileInSharedMemory();
    ASSERT_TRUE(handle.IsValid());
    mojo::ScopedSharedBufferHandle buffer_handle =
        mojo::WrapSharedMemoryHandle(handle, handle.GetSize(), true);
    ASSERT_TRUE(buffer_handle->is_valid());
    EXPECT_CALL(*this, CallbackOnSuccess(testing::_)).Times(1);
    ptr->CompositePdf(std::move(buffer_handle),
                      base::BindOnce(&PdfCompositorServiceTest::OnCallback,
                                     base::Unretained(this)));
    run_loop_->Run();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  mojom::PdfCompositorPtr compositor_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PdfCompositorServiceTest);
};

// Test callback is called on error conditions in service.
TEST_F(PdfCompositorServiceTest, InvokeCallbackOnContentError) {
  EXPECT_CALL(*this, CallbackOnError(
                         mojom::PdfCompositor::Status::CONTENT_FORMAT_ERROR))
      .Times(1);
  compositor_->CompositePdf(
      mojo::SharedBufferHandle::Create(10),
      base::BindOnce(&PdfCompositorServiceTest::OnCallback,
                     base::Unretained(this)));
  run_loop_->Run();
}

TEST_F(PdfCompositorServiceTest, InvokeCallbackOnSuccess) {
  CallCompositorWithSuccess(std::move(compositor_));
}

TEST_F(PdfCompositorServiceTest, ServiceInstances) {
  // One service can bind multiple interfaces.
  mojom::PdfCompositorPtr another_compositor;
  ASSERT_FALSE(another_compositor);
  connector()->BindInterface(mojom::kServiceName, &another_compositor);
  ASSERT_TRUE(another_compositor);
  ASSERT_NE(compositor_.get(), another_compositor.get());

  // Terminating one interface won't affect another.
  compositor_.reset();
  CallCompositorWithSuccess(std::move(another_compositor));
}

}  // namespace printing
