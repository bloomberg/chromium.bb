// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/read_file.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const int kFileHandle = 3;
const int kOffset = 10;
const int kLength = 5;

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  class Event {
   public:
    Event(int chunk_length, bool has_more, base::File::Error result)
        : chunk_length_(chunk_length), has_more_(has_more), result_(result) {}
    virtual ~Event() {}

    int chunk_length() const { return chunk_length_; }
    bool has_more() const { return has_more_; }
    base::File::Error result() const { return result_; }

   private:
    int chunk_length_;
    bool has_more_;
    base::File::Error result_;

    DISALLOW_COPY_AND_ASSIGN(Event);
  };

  CallbackLogger() {}
  virtual ~CallbackLogger() {}

  void OnReadFile(int chunk_length, bool has_more, base::File::Error result) {
    events_.push_back(new Event(chunk_length, has_more, result));
  }

  ScopedVector<Event>& events() { return events_; }

 private:
  ScopedVector<Event> events_;
  bool dispatch_reply_;

  DISALLOW_COPY_AND_ASSIGN(CallbackLogger);
};

}  // namespace

class FileSystemProviderOperationsReadFileTest : public testing::Test {
 protected:
  FileSystemProviderOperationsReadFileTest() {}
  virtual ~FileSystemProviderOperationsReadFileTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* display_name */,
                               false /* writable */,
                               base::FilePath() /* mount_path */);
    io_buffer_ = make_scoped_refptr(new net::IOBuffer(kOffset + kLength));
  }

  ProvidedFileSystemInfo file_system_info_;
  scoped_refptr<net::IOBuffer> io_buffer_;
};

TEST_F(FileSystemProviderOperationsReadFileTest, Execute) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadFile read_file(NULL,
                     file_system_info_,
                     kFileHandle,
                     io_buffer_.get(),
                     kOffset,
                     kLength,
                     base::Bind(&CallbackLogger::OnReadFile,
                                base::Unretained(&callback_logger)));
  read_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(read_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnReadFileRequested::kEventName,
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

  int event_file_handle = -1;
  EXPECT_TRUE(options->GetInteger("openRequestId", &event_file_handle));
  EXPECT_EQ(kFileHandle, event_file_handle);

  double event_offset = -1;
  EXPECT_TRUE(options->GetDouble("offset", &event_offset));
  EXPECT_EQ(kOffset, static_cast<double>(event_offset));

  int event_length = -1;
  EXPECT_TRUE(options->GetInteger("length", &event_length));
  EXPECT_EQ(kLength, event_length);
}

TEST_F(FileSystemProviderOperationsReadFileTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadFile read_file(NULL,
                     file_system_info_,
                     kFileHandle,
                     io_buffer_.get(),
                     kOffset,
                     kLength,
                     base::Bind(&CallbackLogger::OnReadFile,
                                base::Unretained(&callback_logger)));
  read_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(read_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsReadFileTest, OnSuccess) {
  using extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadFile read_file(NULL,
                     file_system_info_,
                     kFileHandle,
                     io_buffer_.get(),
                     kOffset,
                     kLength,
                     base::Bind(&CallbackLogger::OnReadFile,
                                base::Unretained(&callback_logger)));
  read_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(read_file.Execute(kRequestId));

  const std::string data = "ABCDE";
  const bool has_more = false;
  const int execution_time = 0;

  base::ListValue value_as_list;
  value_as_list.Set(0, new base::StringValue(kFileSystemId));
  value_as_list.Set(1, new base::FundamentalValue(kRequestId));
  value_as_list.Set(
      2, base::BinaryValue::CreateWithCopiedBuffer(data.c_str(), data.size()));
  value_as_list.Set(3, new base::FundamentalValue(has_more));
  value_as_list.Set(4, new base::FundamentalValue(execution_time));

  scoped_ptr<Params> params(Params::Create(value_as_list));
  ASSERT_TRUE(params.get());
  scoped_ptr<RequestValue> request_value(
      RequestValue::CreateForReadFileSuccess(params.Pass()));
  ASSERT_TRUE(request_value.get());

  read_file.OnSuccess(kRequestId, request_value.Pass(), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(kLength, event->chunk_length());
  EXPECT_FALSE(event->has_more());
  EXPECT_EQ(data, std::string(io_buffer_->data(), kLength));
  EXPECT_EQ(base::File::FILE_OK, event->result());
}

TEST_F(FileSystemProviderOperationsReadFileTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadFile read_file(NULL,
                     file_system_info_,
                     kFileHandle,
                     io_buffer_.get(),
                     kOffset,
                     kLength,
                     base::Bind(&CallbackLogger::OnReadFile,
                                base::Unretained(&callback_logger)));
  read_file.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(read_file.Execute(kRequestId));

  read_file.OnError(kRequestId,
                    scoped_ptr<RequestValue>(new RequestValue()),
                    base::File::FILE_ERROR_TOO_MANY_OPENED);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
