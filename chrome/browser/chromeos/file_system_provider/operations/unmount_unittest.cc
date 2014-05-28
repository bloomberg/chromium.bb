// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/file_system_provider/operations/unmount.h"
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

// Fake event dispatcher implementation with extra logging capability. Acts as
// a providing extension end-point.
class LoggingDispatchEventImpl {
 public:
  explicit LoggingDispatchEventImpl(bool dispatch_reply)
      : dispatch_reply_(dispatch_reply) {}
  virtual ~LoggingDispatchEventImpl() {}

  bool OnDispatchEventImpl(scoped_ptr<extensions::Event> event) {
    events_.push_back(event->DeepCopy());
    return dispatch_reply_;
  }

  ScopedVector<extensions::Event>& events() { return events_; }

 private:
  ScopedVector<extensions::Event> events_;
  bool dispatch_reply_;

  DISALLOW_COPY_AND_ASSIGN(LoggingDispatchEventImpl);
};

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  CallbackLogger() : weak_ptr_factory_(this) {}
  virtual ~CallbackLogger() {}

  void OnUnmount(base::File::Error result) { events_.push_back(result); }

  std::vector<base::File::Error>& events() { return events_; }

  base::WeakPtr<CallbackLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<base::File::Error> events_;
  bool dispatch_reply_;
  base::WeakPtrFactory<CallbackLogger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CallbackLogger);
};

}  // namespace

class FileSystemProviderOperationsUnmountTest : public testing::Test {
 protected:
  FileSystemProviderOperationsUnmountTest() {}
  virtual ~FileSystemProviderOperationsUnmountTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* file_system_name */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsUnmountTest, Execute) {
  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  Unmount unmount(
      NULL,
      file_system_info_,
      base::Bind(&CallbackLogger::OnUnmount, callback_logger.GetWeakPtr()));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnUnmountRequested::kEventName,
      event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(2u, event_args->GetSize());

  std::string event_file_system_id;
  EXPECT_TRUE(event_args->GetString(0, &event_file_system_id));
  EXPECT_EQ(kFileSystemId, event_file_system_id);

  int event_request_id = -1;
  EXPECT_TRUE(event_args->GetInteger(1, &event_request_id));
  EXPECT_EQ(kRequestId, event_request_id);
}

TEST_F(FileSystemProviderOperationsUnmountTest, Execute_NoListener) {
  LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  Unmount unmount(
      NULL,
      file_system_info_,
      base::Bind(&CallbackLogger::OnUnmount, callback_logger.GetWeakPtr()));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(unmount.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsUnmountTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      UnmountRequestedSuccess::Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  Unmount unmount(
      NULL,
      file_system_info_,
      base::Bind(&CallbackLogger::OnUnmount, callback_logger.GetWeakPtr()));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  unmount.OnSuccess(kRequestId,
                    scoped_ptr<RequestValue>(new RequestValue()),
                    false /* has_more */);
  ASSERT_EQ(1u, callback_logger.events().size());
  base::File::Error event_result = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_OK, event_result);
}

TEST_F(FileSystemProviderOperationsUnmountTest, OnError) {
  using extensions::api::file_system_provider_internal::UnmountRequestedError::
      Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  Unmount unmount(
      NULL,
      file_system_info_,
      base::Bind(&CallbackLogger::OnUnmount, callback_logger.GetWeakPtr()));
  unmount.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(unmount.Execute(kRequestId));

  unmount.OnError(kRequestId, base::File::FILE_ERROR_NOT_FOUND);
  ASSERT_EQ(1u, callback_logger.events().size());
  base::File::Error event_result = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, event_result);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
