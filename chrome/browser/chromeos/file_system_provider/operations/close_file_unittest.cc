// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/file_system_provider/operations/close_file.h"
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
const int kOpenRequestId = 3;

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

  void OnCloseFile(base::File::Error result) { events_.push_back(result); }

  std::vector<base::File::Error>& events() { return events_; }

  base::WeakPtr<CallbackLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<base::File::Error> events_;
  base::WeakPtrFactory<CallbackLogger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CallbackLogger);
};

}  // namespace

class FileSystemProviderOperationsCloseFileTest : public testing::Test {
 protected:
  FileSystemProviderOperationsCloseFileTest() {}
  virtual ~FileSystemProviderOperationsCloseFileTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* file_system_name */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsCloseFileTest, Execute) {
  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  CloseFile close_file(
      NULL,
      file_system_info_,
      kOpenRequestId,
      base::Bind(&CallbackLogger::OnCloseFile, callback_logger.GetWeakPtr()));
  close_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(close_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnCloseFileRequested::kEventName,
      event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(3u, event_args->GetSize());

  std::string event_file_system_id;
  EXPECT_TRUE(event_args->GetString(0, &event_file_system_id));
  EXPECT_EQ(kFileSystemId, event_file_system_id);

  int event_request_id = -1;
  EXPECT_TRUE(event_args->GetInteger(1, &event_request_id));
  EXPECT_EQ(kRequestId, event_request_id);

  int event_open_request_id = -1;
  EXPECT_TRUE(event_args->GetInteger(2, &event_open_request_id));
  EXPECT_EQ(kOpenRequestId, event_open_request_id);
}

TEST_F(FileSystemProviderOperationsCloseFileTest, Execute_NoListener) {
  LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  CloseFile close_file(
      NULL,
      file_system_info_,
      kOpenRequestId,
      base::Bind(&CallbackLogger::OnCloseFile, callback_logger.GetWeakPtr()));
  close_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(close_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsCloseFileTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      CloseFileRequestedSuccess::Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  CloseFile close_file(
      NULL,
      file_system_info_,
      kOpenRequestId,
      base::Bind(&CallbackLogger::OnCloseFile, callback_logger.GetWeakPtr()));
  close_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(close_file.Execute(kRequestId));

  close_file.OnSuccess(kRequestId,
                       scoped_ptr<RequestValue>(new RequestValue()),
                       false /* has_next */);
  ASSERT_EQ(1u, callback_logger.events().size());
  EXPECT_EQ(base::File::FILE_OK, callback_logger.events()[0]);
}

TEST_F(FileSystemProviderOperationsCloseFileTest, OnError) {
  using extensions::api::file_system_provider_internal::
      CloseFileRequestedError::Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  CloseFile close_file(
      NULL,
      file_system_info_,
      kOpenRequestId,
      base::Bind(&CallbackLogger::OnCloseFile, callback_logger.GetWeakPtr()));
  close_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(close_file.Execute(kRequestId));

  close_file.OnError(kRequestId, base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_logger.events().size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            callback_logger.events()[0]);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
