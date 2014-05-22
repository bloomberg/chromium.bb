// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/file_system_provider/operations/open_file.h"
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
const base::FilePath::CharType kFilePath[] = "/directory/blueberries.txt";

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
  class Event {
   public:
    Event(int file_handle, base::File::Error result)
        : file_handle_(file_handle), result_(result) {}
    virtual ~Event() {}

    int file_handle() { return file_handle_; }
    base::File::Error result() { return result_; }

   private:
    int file_handle_;
    base::File::Error result_;

    DISALLOW_COPY_AND_ASSIGN(Event);
  };

  CallbackLogger() : weak_ptr_factory_(this) {}
  virtual ~CallbackLogger() {}

  void OnOpenFile(int file_handle, base::File::Error result) {
    events_.push_back(new Event(file_handle, result));
  }

  ScopedVector<Event>& events() { return events_; }

  base::WeakPtr<CallbackLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ScopedVector<Event> events_;
  bool dispatch_reply_;
  base::WeakPtrFactory<CallbackLogger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CallbackLogger);
};

}  // namespace

class FileSystemProviderOperationsOpenFileTest : public testing::Test {
 protected:
  FileSystemProviderOperationsOpenFileTest() {}
  virtual ~FileSystemProviderOperationsOpenFileTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* file_system_name */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute) {
  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kFilePath),
      ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
      false /* create */,
      base::Bind(&CallbackLogger::OnOpenFile, callback_logger.GetWeakPtr()));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
      event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(5u, event_args->GetSize());

  std::string event_file_system_id;
  EXPECT_TRUE(event_args->GetString(0, &event_file_system_id));
  EXPECT_EQ(kFileSystemId, event_file_system_id);

  int event_request_id = -1;
  EXPECT_TRUE(event_args->GetInteger(1, &event_request_id));
  EXPECT_EQ(kRequestId, event_request_id);

  std::string event_file_path;
  EXPECT_TRUE(event_args->GetString(2, &event_file_path));
  EXPECT_EQ(kFilePath, event_file_path);

  std::string event_file_open_mode;
  EXPECT_TRUE(event_args->GetString(3, &event_file_open_mode));
  const std::string expected_file_open_mode =
      extensions::api::file_system_provider::ToString(
          extensions::api::file_system_provider::OPEN_FILE_MODE_READ);
  EXPECT_EQ(expected_file_open_mode, event_file_open_mode);

  bool event_create;
  EXPECT_TRUE(event_args->GetBoolean(4, &event_create));
  EXPECT_FALSE(event_create);
}

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute_NoListener) {
  LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kFilePath),
      ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
      false /* create */,
      base::Bind(&CallbackLogger::OnOpenFile, callback_logger.GetWeakPtr()));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(open_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsOpenFileTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      OpenFileRequestedSuccess::Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kFilePath),
      ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
      false /* create */,
      base::Bind(&CallbackLogger::OnOpenFile, callback_logger.GetWeakPtr()));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  open_file.OnSuccess(kRequestId,
                      scoped_ptr<RequestValue>(new RequestValue()),
                      false /* has_next */);
  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_OK, event->result());
  EXPECT_LT(0, event->file_handle());
}

TEST_F(FileSystemProviderOperationsOpenFileTest, OnError) {
  using extensions::api::file_system_provider_internal::OpenFileRequestedError::
      Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(
      NULL,
      file_system_info_,
      base::FilePath::FromUTF8Unsafe(kFilePath),
      ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
      false /* create */,
      base::Bind(&CallbackLogger::OnOpenFile, callback_logger.GetWeakPtr()));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  open_file.OnError(kRequestId, base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
  ASSERT_EQ(0, event->file_handle());
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
