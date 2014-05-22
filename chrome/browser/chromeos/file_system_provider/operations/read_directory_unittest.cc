// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/operations/read_directory.h"
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
const base::FilePath::CharType kDirectoryPath[] = "/directory";

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
    Event(base::File::Error result,
          const fileapi::AsyncFileUtil::EntryList& entry_list,
          bool has_more)
        : result_(result), entry_list_(entry_list), has_more_(has_more) {}
    virtual ~Event() {}

    base::File::Error result() { return result_; }
    const fileapi::AsyncFileUtil::EntryList& entry_list() {
      return entry_list_;
    }
    bool has_more() { return has_more_; }

   private:
    base::File::Error result_;
    fileapi::AsyncFileUtil::EntryList entry_list_;
    bool has_more_;

    DISALLOW_COPY_AND_ASSIGN(Event);
  };

  CallbackLogger() : weak_ptr_factory_(this) {}
  virtual ~CallbackLogger() {}

  void OnReadDirectory(base::File::Error result,
                       const fileapi::AsyncFileUtil::EntryList& entry_list,
                       bool has_more) {
    events_.push_back(new Event(result, entry_list, has_more));
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

class FileSystemProviderOperationsReadDirectoryTest : public testing::Test {
 protected:
  FileSystemProviderOperationsReadDirectoryTest() {}
  virtual ~FileSystemProviderOperationsReadDirectoryTest() {}

  virtual void SetUp() OVERRIDE {
    file_system_info_ =
        ProvidedFileSystemInfo(kExtensionId,
                               kFileSystemId,
                               "" /* file_system_name */,
                               base::FilePath() /* mount_path */);
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsReadDirectoryTest, Execute) {
  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(NULL,
                               file_system_info_,
                               base::FilePath::FromUTF8Unsafe(kDirectoryPath),
                               base::Bind(&CallbackLogger::OnReadDirectory,
                                          callback_logger.GetWeakPtr()));
  read_directory.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(extensions::api::file_system_provider::OnReadDirectoryRequested::
                kEventName,
            event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(3u, event_args->GetSize());

  std::string event_file_system_id;
  EXPECT_TRUE(event_args->GetString(0, &event_file_system_id));
  EXPECT_EQ(kFileSystemId, event_file_system_id);

  int event_request_id = -1;
  EXPECT_TRUE(event_args->GetInteger(1, &event_request_id));
  EXPECT_EQ(kRequestId, event_request_id);

  std::string event_directory_path;
  EXPECT_TRUE(event_args->GetString(2, &event_directory_path));
  EXPECT_EQ(kDirectoryPath, event_directory_path);
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest, Execute_NoListener) {
  LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(NULL,
                               file_system_info_,
                               base::FilePath::FromUTF8Unsafe(kDirectoryPath),
                               base::Bind(&CallbackLogger::OnReadDirectory,
                                          callback_logger.GetWeakPtr()));
  read_directory.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(read_directory.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest, OnSuccess) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(NULL,
                               file_system_info_,
                               base::FilePath::FromUTF8Unsafe(kDirectoryPath),
                               base::Bind(&CallbackLogger::OnReadDirectory,
                                          callback_logger.GetWeakPtr()));
  read_directory.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  // Sample input as JSON. Keep in sync with file_system_provider_api.idl.
  // As for now, it is impossible to create *::Params class directly, not from
  // base::Value.
  const std::string input =
      "[\n"
      "  \"testing-file-system\",\n"  // kFileSystemId
      "  2,\n"                        // kRequestId
      "  [\n"
      "    {\n"
      "      \"isDirectory\": false,\n"
      "      \"name\": \"blueberries.txt\",\n"
      "      \"size\": 4096,\n"
      "      \"modificationTime\": {\n"
      "        \"value\": \"Thu Apr 24 00:46:52 UTC 2014\"\n"
      "      }\n"
      "    }\n"
      "  ],\n"
      "  false\n"  // has_next
      "]\n";

  int json_error_code;
  std::string json_error_msg;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      input, base::JSON_PARSE_RFC, &json_error_code, &json_error_msg));
  ASSERT_TRUE(value.get()) << json_error_msg;

  base::ListValue* value_as_list;
  ASSERT_TRUE(value->GetAsList(&value_as_list));
  scoped_ptr<Params> params(Params::Create(*value_as_list));
  ASSERT_TRUE(params.get());
  scoped_ptr<RequestValue> request_value(
      RequestValue::CreateForReadDirectorySuccess(params.Pass()));
  ASSERT_TRUE(request_value.get());

  const bool has_next = false;
  read_directory.OnSuccess(kRequestId, request_value.Pass(), has_next);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_OK, event->result());

  ASSERT_EQ(1u, event->entry_list().size());
  const fileapi::DirectoryEntry entry = event->entry_list()[0];
  EXPECT_FALSE(entry.is_directory);
  EXPECT_EQ("blueberries.txt", entry.name);
  EXPECT_EQ(4096, entry.size);
  base::Time expected_time;
  EXPECT_TRUE(
      base::Time::FromString("Thu Apr 24 00:46:52 UTC 2014", &expected_time));
  EXPECT_EQ(expected_time, entry.last_modified_time);
}

TEST_F(FileSystemProviderOperationsReadDirectoryTest, OnError) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params;

  LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  ReadDirectory read_directory(NULL,
                               file_system_info_,
                               base::FilePath::FromUTF8Unsafe(kDirectoryPath),
                               base::Bind(&CallbackLogger::OnReadDirectory,
                                          callback_logger.GetWeakPtr()));
  read_directory.SetDispatchEventImplForTesting(
      base::Bind(&LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(read_directory.Execute(kRequestId));

  read_directory.OnError(kRequestId, base::File::FILE_ERROR_TOO_MANY_OPENED);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
  ASSERT_EQ(0u, event->entry_list().size());
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
