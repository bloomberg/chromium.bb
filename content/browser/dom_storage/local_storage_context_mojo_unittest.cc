// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_context_mojo.h"

#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/mock_leveldb_database.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/file/file_service.h"
#include "services/file/public/interfaces/constants.mojom.h"
#include "services/file/user_id_map.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;

namespace content {

namespace {

void NoOpSuccess(bool success) {}

void GetCallback(const base::Closure& callback,
                 bool* success_out,
                 std::vector<uint8_t>* value_out,
                 bool success,
                 const std::vector<uint8_t>& value) {
  *success_out = success;
  *value_out = value;
  callback.Run();
}

}  // namespace

class LocalStorageContextMojoTest : public testing::Test {
 public:
  LocalStorageContextMojoTest() : db_(&mock_data_), db_binding_(&db_) {}

  LocalStorageContextMojo* context() {
    if (!context_) {
      context_ =
          base::MakeUnique<LocalStorageContextMojo>(nullptr, base::FilePath());
      context_->SetDatabaseForTesting(db_binding_.CreateInterfacePtrAndBind());
    }
    return context_.get();
  }
  const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data() {
    return mock_data_;
  }

  void set_mock_data(const std::string& key, const std::string& value) {
    mock_data_[StdStringToUint8Vector(key)] = StdStringToUint8Vector(value);
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  MockLevelDBDatabase db_;
  mojo::Binding<leveldb::mojom::LevelDBDatabase> db_binding_;

  std::unique_ptr<LocalStorageContextMojo> context_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageContextMojoTest);
};

TEST_F(LocalStorageContextMojoTest, Basic) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));
  wrapper->Put(key, value, "source", base::Bind(&NoOpSuccess));
  wrapper.reset();

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, mock_data().size());
  EXPECT_EQ(value, mock_data().rbegin()->second);
}

TEST_F(LocalStorageContextMojoTest, OriginsAreIndependent) {
  url::Origin origin1(GURL("http://foobar.com:123"));
  url::Origin origin2(GURL("http://foobar.com:1234"));
  auto key1 = StdStringToUint8Vector("4key");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(origin1, MakeRequest(&wrapper));
  wrapper->Put(key1, value, "source", base::Bind(&NoOpSuccess));
  wrapper.reset();

  context()->OpenLocalStorage(origin2, MakeRequest(&wrapper));
  wrapper->Put(key2, value, "source", base::Bind(&NoOpSuccess));
  wrapper.reset();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, mock_data().size());
  EXPECT_EQ(value, mock_data().rbegin()->second);
}

TEST_F(LocalStorageContextMojoTest, ValidVersion) {
  set_mock_data("VERSION", "1");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));

  base::RunLoop run_loop;
  bool success = false;
  std::vector<uint8_t> result;
  wrapper->Get(
      StdStringToUint8Vector("key"),
      base::Bind(&GetCallback, run_loop.QuitClosure(), &success, &result));
  run_loop.Run();
  EXPECT_TRUE(success);
  EXPECT_EQ(StdStringToUint8Vector("value"), result);
}

TEST_F(LocalStorageContextMojoTest, InvalidVersion) {
  set_mock_data("VERSION", "foobar");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));

  base::RunLoop run_loop;
  bool success = false;
  std::vector<uint8_t> result;
  wrapper->Get(
      StdStringToUint8Vector("key"),
      base::Bind(&GetCallback, run_loop.QuitClosure(), &success, &result));
  run_loop.Run();
  EXPECT_FALSE(success);
}

TEST_F(LocalStorageContextMojoTest, VersionOnlyWrittenOnCommit) {
  mojom::LevelDBWrapperPtr wrapper;
  context()->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));
  base::RunLoop run_loop;
  bool success = false;
  std::vector<uint8_t> result;
  wrapper->Get(
      StdStringToUint8Vector("key"),
      base::Bind(&GetCallback, run_loop.QuitClosure(), &success, &result));
  run_loop.Run();
  EXPECT_FALSE(success);
  wrapper.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_data().empty());
}

namespace {

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory,
                          public service_manager::InterfaceFactory<
                              service_manager::mojom::ServiceFactory> {
 public:
  explicit ServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test) {}
  ~ServiceTestClient() override {}

 protected:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::mojom::ServiceFactory>(this);
    return true;
  }

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == file::mojom::kServiceName) {
      file_service_context_.reset(new service_manager::ServiceContext(
          file::CreateFileService(
              BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
              BrowserThread::GetTaskRunnerForThread(BrowserThread::DB)),
          std::move(request)));
    }
  }

  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ServiceFactoryRequest request) override {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> file_service_context_;
};

}  // namespace

class LocalStorageContextMojoTestWithService
    : public service_manager::test::ServiceTest {
 public:
  LocalStorageContextMojoTestWithService()
      : ServiceTest("content_unittests", false) {}
  ~LocalStorageContextMojoTestWithService() override {}

 protected:
  void SetUp() override {
    ServiceTest::SetUp();
    ASSERT_TRUE(temp_path_.CreateUniqueTempDir());
    file::AssociateServiceUserIdWithUserDir(test_userid(),
                                            temp_path_.GetPath());
  }

  void TearDown() override {
    temp_path_.Take();
    ServiceTest::TearDown();
  }

  std::unique_ptr<service_manager::Service> CreateService() override {
    return base::MakeUnique<ServiceTestClient>(this);
  }

  std::unique_ptr<base::MessageLoop> CreateMessageLoop() override {
    return nullptr;
  }

  const base::FilePath& temp_path() { return temp_path_.GetPath(); }

  base::FilePath FirstEntryInDir() {
    base::FileEnumerator enumerator(
        temp_path(), false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    return enumerator.Next();
  }

  void DoTestPut(LocalStorageContextMojo* context,
                 const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& value) {
    mojom::LevelDBWrapperPtr wrapper;
    context->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));
    wrapper->Put(key, value, "source", base::Bind(&NoOpSuccess));
    wrapper.reset();
    base::RunLoop().RunUntilIdle();
  }

  bool DoTestGet(LocalStorageContextMojo* context,
                 const std::vector<uint8_t>& key,
                 std::vector<uint8_t>* result) {
    mojom::LevelDBWrapperPtr wrapper;
    context->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                              MakeRequest(&wrapper));
    base::RunLoop run_loop;
    bool success = false;
    wrapper->Get(key, base::Bind(&GetCallback, run_loop.QuitClosure(), &success,
                                 result));
    run_loop.Run();
    return success;
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_path_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageContextMojoTestWithService);
};

// Enable when http://crbug.com/677194 is fixed and ServiceTest works
// correctly on Android.
#if defined(OS_ANDROID)
#define MAYBE_InMemory DISABLED_InMemory
#else
#define MAYBE_InMemory InMemory
#endif
TEST_F(LocalStorageContextMojoTestWithService, MAYBE_InMemory) {
  auto context =
      base::MakeUnique<LocalStorageContextMojo>(connector(), base::FilePath());
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojom::LevelDBWrapperPtr wrapper;
  context->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                            MakeRequest(&wrapper));

  DoTestPut(context.get(), key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context.get(), key, &result));
  EXPECT_EQ(value, result);

  context.reset();
  base::RunLoop().RunUntilIdle();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());

  // Re-opening should get fresh data.
  context.reset(new LocalStorageContextMojo(connector(), base::FilePath()));
  EXPECT_FALSE(DoTestGet(context.get(), key, &result));
}

// Enable when http://crbug.com/677194 is fixed and ServiceTest works
// correctly on Android.
#if defined(OS_ANDROID)
#define MAYBE_InMemoryInvalidPath DISABLED_InMemoryInvalidPath
#else
#define MAYBE_InMemoryInvalidPath InMemoryInvalidPath
#endif
TEST_F(LocalStorageContextMojoTestWithService, MAYBE_InMemoryInvalidPath) {
  auto context = base::MakeUnique<LocalStorageContextMojo>(
      connector(), base::FilePath(FILE_PATH_LITERAL("../../")));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojom::LevelDBWrapperPtr wrapper;
  context->OpenLocalStorage(url::Origin(GURL("http://foobar.com")),
                            MakeRequest(&wrapper));

  DoTestPut(context.get(), key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context.get(), key, &result));
  EXPECT_EQ(value, result);

  context.reset();
  base::RunLoop().RunUntilIdle();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());
}

// Enable when http://crbug.com/677194 is fixed and ServiceTest works
// correctly on Android.
#if defined(OS_ANDROID)
#define MAYBE_OnDisk DISABLED_OnDisk
#else
#define MAYBE_OnDisk OnDisk
#endif
TEST_F(LocalStorageContextMojoTestWithService, MAYBE_OnDisk) {
  base::FilePath test_path(FILE_PATH_LITERAL("test_path"));
  auto context =
      base::MakeUnique<LocalStorageContextMojo>(connector(), test_path);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context.get(), key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context.get(), key, &result));
  EXPECT_EQ(value, result);

  context.reset();
  base::RunLoop().RunUntilIdle();

  // Should have created files.
  EXPECT_EQ(test_path, FirstEntryInDir().BaseName());

  // Should be able to re-open.
  context.reset(new LocalStorageContextMojo(connector(), test_path));
  EXPECT_TRUE(DoTestGet(context.get(), key, &result));
  EXPECT_EQ(value, result);
}

}  // namespace content
