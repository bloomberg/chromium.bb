// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/open_file.h"

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

  CallbackLogger() {}
  virtual ~CallbackLogger() {}

  void OnOpenFile(int file_handle, base::File::Error result) {
    events_.push_back(new Event(file_handle, result));
  }

  ScopedVector<Event>& events() { return events_; }

 private:
  ScopedVector<Event> events_;
  bool dispatch_reply_;

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
                               "" /* display_name */,
                               false /* writable */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(NULL,
                     file_system_info_,
                     base::FilePath::FromUTF8Unsafe(kFilePath),
                     ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
                     base::Bind(&CallbackLogger::OnOpenFile,
                                base::Unretained(&callback_logger)));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
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

  std::string event_file_path;
  EXPECT_TRUE(options->GetString("filePath", &event_file_path));
  EXPECT_EQ(kFilePath, event_file_path);

  std::string event_file_open_mode;
  EXPECT_TRUE(options->GetString("mode", &event_file_open_mode));
  const std::string expected_file_open_mode =
      extensions::api::file_system_provider::ToString(
          extensions::api::file_system_provider::OPEN_FILE_MODE_READ);
  EXPECT_EQ(expected_file_open_mode, event_file_open_mode);
}

TEST_F(FileSystemProviderOperationsOpenFileTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(NULL,
                     file_system_info_,
                     base::FilePath::FromUTF8Unsafe(kFilePath),
                     ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
                     base::Bind(&CallbackLogger::OnOpenFile,
                                base::Unretained(&callback_logger)));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(open_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsOpenFileTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(NULL,
                     file_system_info_,
                     base::FilePath::FromUTF8Unsafe(kFilePath),
                     ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
                     base::Bind(&CallbackLogger::OnOpenFile,
                                base::Unretained(&callback_logger)));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  open_file.OnSuccess(kRequestId,
                      scoped_ptr<RequestValue>(new RequestValue()),
                      false /* has_more */);
  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_OK, event->result());
  EXPECT_LT(0, event->file_handle());
}

TEST_F(FileSystemProviderOperationsOpenFileTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  OpenFile open_file(NULL,
                     file_system_info_,
                     base::FilePath::FromUTF8Unsafe(kFilePath),
                     ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
                     base::Bind(&CallbackLogger::OnOpenFile,
                                base::Unretained(&callback_logger)));
  open_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(open_file.Execute(kRequestId));

  open_file.OnError(kRequestId,
                    scoped_ptr<RequestValue>(new RequestValue()),
                    base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
  ASSERT_EQ(0, event->file_handle());
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
