// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_directory.h"
#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const base::FilePath::CharType kDirectoryPath[] = "/kitty/and/puppy/happy";

}  // namespace

class FileSystemProviderOperationsCreateDirectoryTest : public testing::Test {
 protected:
  FileSystemProviderOperationsCreateDirectoryTest() {}
  virtual ~FileSystemProviderOperationsCreateDirectoryTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* file_system_name */,
                               true /* writable */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsCreateDirectoryTest, Execute) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CreateDirectory create_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      false /* exclusive */,
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  create_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(create_directory.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(extensions::api::file_system_provider::OnCreateDirectoryRequested::
                kEventName,
            event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(1u, event_args->GetSize());

  base::DictionaryValue* options = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &options));

  std::string event_file_system_id;
  EXPECT_TRUE(options->GetString("fileSystemId", &event_file_system_id));
  EXPECT_EQ(kFileSystemId, event_file_system_id);

  int event_request_id = -1;
  EXPECT_TRUE(options->GetInteger("requestId", &event_request_id));
  EXPECT_EQ(kRequestId, event_request_id);

  std::string event_directory_path;
  EXPECT_TRUE(options->GetString("directoryPath", &event_directory_path));
  EXPECT_EQ(kDirectoryPath, event_directory_path);

  bool event_exclusive;
  EXPECT_TRUE(options->GetBoolean("exclusive", &event_exclusive));
  EXPECT_FALSE(event_exclusive);

  bool event_recursive;
  EXPECT_TRUE(options->GetBoolean("recursive", &event_recursive));
  EXPECT_TRUE(event_recursive);
}

TEST_F(FileSystemProviderOperationsCreateDirectoryTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CreateDirectory create_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      false /* exclusive */,
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  create_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(create_directory.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsCreateDirectoryTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CreateDirectory create_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      false /* exclusive */,
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  create_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(create_directory.Execute(kRequestId));

  create_directory.OnSuccess(kRequestId,
                             scoped_ptr<RequestValue>(new RequestValue()),
                             false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, callback_log[0]);
}

TEST_F(FileSystemProviderOperationsCreateDirectoryTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CreateDirectory create_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      false /* exclusive */,
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  create_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(create_directory.Execute(kRequestId));

  create_directory.OnError(kRequestId,
                           scoped_ptr<RequestValue>(new RequestValue()),
                           base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, callback_log[0]);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
