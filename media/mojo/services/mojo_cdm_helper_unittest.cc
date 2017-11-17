// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/mojo/interfaces/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Status = cdm::FileIOClient::Status;

namespace media {

namespace {

class MockFileIOClient : public cdm::FileIOClient {
 public:
  MockFileIOClient() {}
  ~MockFileIOClient() override {}

  MOCK_METHOD1(OnOpenComplete, void(Status));
  MOCK_METHOD3(OnReadComplete, void(Status, const uint8_t*, uint32_t));
  MOCK_METHOD1(OnWriteComplete, void(Status));
};

class MockCdmStorage : public mojom::CdmStorage {
 public:
  MockCdmStorage() { CHECK(temp_directory_.CreateUniqueTempDir()); }
  ~MockCdmStorage() override {}

  // MojoCdmFileIO calls CdmStorage::Open() to actually open the file.
  // Simulate this by creating a file in the temp directory and returning it.
  void Open(const std::string& file_name, OpenCallback callback) override {
    base::FilePath temp_file = temp_directory_.GetPath().AppendASCII(file_name);
    DVLOG(1) << __func__ << " " << temp_file;
    base::File file(temp_file, base::File::FLAG_CREATE_ALWAYS |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    std::move(callback).Run(mojom::CdmStorage::Status::kSuccess,
                            std::move(file), nullptr);
  }

 private:
  base::ScopedTempDir temp_directory_;
};

void CreateCdmStorage(mojom::CdmStorageRequest request) {
  mojo::MakeStrongBinding(std::make_unique<MockCdmStorage>(),
                          std::move(request));
}

class TestInterfaceProvider : public service_manager::mojom::InterfaceProvider {
 public:
  TestInterfaceProvider() {
    registry_.AddInterface(base::Bind(&CreateCdmStorage));
  }
  ~TestInterfaceProvider() override {}

  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override {
    registry_.BindInterface(interface_name, std::move(handle));
  }

 private:
  service_manager::BinderRegistry registry_;
};

}  // namespace

class MojoCdmHelperTest : public testing::Test {
 protected:
  MojoCdmHelperTest() : helper_(&test_interface_provider_) {}
  ~MojoCdmHelperTest() override {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestInterfaceProvider test_interface_provider_;
  MockFileIOClient file_io_client_;
  MojoCdmHelper helper_;
};

TEST_F(MojoCdmHelperTest, CreateCdmFileIO_OpenClose) {
  cdm::FileIO* file_io = helper_.CreateCdmFileIO(
      &file_io_client_, CdmAuxiliaryHelper::FileReadCB());
  const std::string kFileName = "openfile";
  EXPECT_CALL(file_io_client_, OnOpenComplete(Status::kSuccess));
  file_io->Open(kFileName.data(), kFileName.length());
  base::RunLoop().RunUntilIdle();

  // Close the file as required by cdm::FileIO API.
  file_io->Close();
  base::RunLoop().RunUntilIdle();
}

// Simulate the case where the CDM didn't call Close(). In this case we still
// should not leak the cdm::FileIO object. LeakSanitizer bots should be able to
// catch such issues.
TEST_F(MojoCdmHelperTest, CreateCdmFileIO_OpenWithoutClose) {
  cdm::FileIO* file_io = helper_.CreateCdmFileIO(
      &file_io_client_, CdmAuxiliaryHelper::FileReadCB());
  const std::string kFileName = "openfile";
  EXPECT_CALL(file_io_client_, OnOpenComplete(Status::kSuccess));
  file_io->Open(kFileName.data(), kFileName.length());
  // file_io->Close() is NOT called.
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/773860): Add more test cases.

}  // namespace media
