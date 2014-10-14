// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/observe_directory.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const base::FilePath::CharType kDirectoryPath[] = "/kitty/and/puppy/happy";

}  // namespace

class FileSystemProviderOperationsObserveDirectoryTest : public testing::Test {
 protected:
  FileSystemProviderOperationsObserveDirectoryTest() {}
  virtual ~FileSystemProviderOperationsObserveDirectoryTest() {}

  virtual void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId,
        MountOptions(kFileSystemId, "" /* display_name */),
        base::FilePath());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsObserveDirectoryTest, Execute) {
  using extensions::api::file_system_provider::ObserveDirectoryRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ObserveDirectory observe_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  observe_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(observe_directory.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(extensions::api::file_system_provider::OnObserveDirectoryRequested::
                kEventName,
            event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(1u, event_args->GetSize());

  const base::DictionaryValue* options_as_value = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &options_as_value));

  ObserveDirectoryRequestedOptions options;
  ASSERT_TRUE(
      ObserveDirectoryRequestedOptions::Populate(*options_as_value, &options));
  EXPECT_EQ(kFileSystemId, options.file_system_id);
  EXPECT_EQ(kRequestId, options.request_id);
  EXPECT_EQ(kDirectoryPath, options.directory_path);
  EXPECT_TRUE(options.recursive);
}

TEST_F(FileSystemProviderOperationsObserveDirectoryTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ObserveDirectory observe_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  observe_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(observe_directory.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsObserveDirectoryTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ObserveDirectory observe_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  observe_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(observe_directory.Execute(kRequestId));

  observe_directory.OnSuccess(kRequestId,
                              scoped_ptr<RequestValue>(new RequestValue()),
                              false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, callback_log[0]);
}

TEST_F(FileSystemProviderOperationsObserveDirectoryTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  ObserveDirectory observe_directory(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kDirectoryPath),
      true /* recursive */,
      base::Bind(&util::LogStatusCallback, &callback_log));
  observe_directory.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(observe_directory.Execute(kRequestId));

  observe_directory.OnError(kRequestId,
                            scoped_ptr<RequestValue>(new RequestValue()),
                            base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, callback_log[0]);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
