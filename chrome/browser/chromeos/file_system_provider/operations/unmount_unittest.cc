// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/unmount.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;

}  // namespace

class FileSystemProviderOperationsUnmountTest : public testing::Test {
 protected:
  FileSystemProviderOperationsUnmountTest() {}
  virtual ~FileSystemProviderOperationsUnmountTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* display_name */,
                               false /* writable */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsUnmountTest, Execute) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Unmount unmount(NULL,
                  file_system_info_,
                  base::Bind(&util::LogStatusCallback, &callback_log));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnUnmountRequested::kEventName,
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
}

TEST_F(FileSystemProviderOperationsUnmountTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Unmount unmount(NULL,
                  file_system_info_,
                  base::Bind(&util::LogStatusCallback, &callback_log));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(unmount.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsUnmountTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      UnmountRequestedSuccess::Params;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Unmount unmount(NULL,
                  file_system_info_,
                  base::Bind(&util::LogStatusCallback, &callback_log));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  unmount.OnSuccess(kRequestId,
                    scoped_ptr<RequestValue>(new RequestValue()),
                    false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  base::File::Error event_result = callback_log[0];
  EXPECT_EQ(base::File::FILE_OK, event_result);
}

TEST_F(FileSystemProviderOperationsUnmountTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Unmount unmount(NULL,
                  file_system_info_,
                  base::Bind(&util::LogStatusCallback, &callback_log));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  unmount.OnError(kRequestId,
                  scoped_ptr<RequestValue>(new RequestValue()),
                  base::File::FILE_ERROR_NOT_FOUND);
  ASSERT_EQ(1u, callback_log.size());
  base::File::Error event_result = callback_log[0];
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, event_result);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
