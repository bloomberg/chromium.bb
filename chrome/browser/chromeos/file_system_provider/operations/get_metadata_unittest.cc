// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/get_metadata.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/operations/test_util.h"
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
const char kMimeType[] = "text/plain";
const int kRequestId = 2;
const base::FilePath::CharType kDirectoryPath[] =
    FILE_PATH_LITERAL("/directory");

// URLs are case insensitive, so it should pass the sanity check.
const char kThumbnail[] = "DaTa:ImAgE/pNg;base64,";

// Returns the request value as |result| in case of successful parse.
void CreateRequestValueFromJSON(const std::string& json,
                                scoped_ptr<RequestValue>* result) {
  using extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params;

  int json_error_code;
  std::string json_error_msg;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &json_error_code, &json_error_msg));
  ASSERT_TRUE(value.get()) << json_error_msg;

  base::ListValue* value_as_list;
  ASSERT_TRUE(value->GetAsList(&value_as_list));
  scoped_ptr<Params> params(Params::Create(*value_as_list));
  ASSERT_TRUE(params.get());
  *result = RequestValue::CreateForGetMetadataSuccess(params.Pass());
  ASSERT_TRUE(result->get());
}

// Callback invocation logger. Acts as a fileapi end-point.
class CallbackLogger {
 public:
  class Event {
   public:
    Event(scoped_ptr<EntryMetadata> metadata, base::File::Error result)
        : metadata_(metadata.Pass()), result_(result) {}
    virtual ~Event() {}

    const EntryMetadata* metadata() const { return metadata_.get(); }
    base::File::Error result() const { return result_; }

   private:
    scoped_ptr<EntryMetadata> metadata_;
    base::File::Error result_;

    DISALLOW_COPY_AND_ASSIGN(Event);
  };

  CallbackLogger() {}
  virtual ~CallbackLogger() {}

  void OnGetMetadata(scoped_ptr<EntryMetadata> metadata,
                     base::File::Error result) {
    events_.push_back(new Event(metadata.Pass(), result));
  }

  const ScopedVector<Event>& events() const { return events_; }

 private:
  ScopedVector<Event> events_;

  DISALLOW_COPY_AND_ASSIGN(CallbackLogger);
};

}  // namespace

class FileSystemProviderOperationsGetMetadataTest : public testing::Test {
 protected:
  FileSystemProviderOperationsGetMetadataTest() {}
  ~FileSystemProviderOperationsGetMetadataTest() override {}

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId,
        MountOptions(kFileSystemId, "" /* display_name */),
        base::FilePath());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsGetMetadataTest, ValidateName) {
  EXPECT_TRUE(ValidateName("hello-world!@#$%^&*()-_=+\"':,.<>?[]{}|\\",
                           false /* root_entry */));
  EXPECT_FALSE(ValidateName("hello-world!@#$%^&*()-_=+\"':,.<>?[]{}|\\",
                            true /* root_entry */));
  EXPECT_FALSE(ValidateName("", false /* root_path */));
  EXPECT_TRUE(ValidateName("", true /* root_path */));
  EXPECT_FALSE(ValidateName("hello/world", false /* root_path */));
  EXPECT_FALSE(ValidateName("hello/world", true /* root_path */));
}

TEST_F(FileSystemProviderOperationsGetMetadataTest, ValidateIDLEntryMetadata) {
  using extensions::api::file_system_provider::EntryMetadata;
  const std::string kValidFileName = "hello-world";
  const std::string kValidThumbnailUrl = "data:something";

  // Correct metadata for non-root.
  {
    EntryMetadata metadata;
    metadata.name = kValidFileName;
    metadata.modification_time.additional_properties.SetString(
        "value", "invalid-date-time");  // Invalid modification time is OK.
    metadata.thumbnail.reset(new std::string(kValidThumbnailUrl));
    EXPECT_TRUE(ValidateIDLEntryMetadata(metadata, false /* root_path */));
  }

  // Correct metadata for non-root (without thumbnail).
  {
    EntryMetadata metadata;
    metadata.name = kValidFileName;
    metadata.modification_time.additional_properties.SetString(
        "value", "invalid-date-time");  // Invalid modification time is OK.
    EXPECT_TRUE(ValidateIDLEntryMetadata(metadata, false /* root_path */));
  }

  // Correct metadata for root.
  {
    EntryMetadata metadata;
    metadata.name = "";
    metadata.modification_time.additional_properties.SetString(
        "value", "invalid-date-time");  // Invalid modification time is OK.
    EXPECT_TRUE(ValidateIDLEntryMetadata(metadata, true /* root_path */));
  }

  // Invalid characters in the name.
  {
    EntryMetadata metadata;
    metadata.name = "hello/world";
    metadata.modification_time.additional_properties.SetString(
        "value", "invalid-date-time");  // Invalid modification time is OK.
    metadata.thumbnail.reset(new std::string(kValidThumbnailUrl));
    EXPECT_FALSE(ValidateIDLEntryMetadata(metadata, false /* root_path */));
  }

  // Empty name for non-root.
  {
    EntryMetadata metadata;
    metadata.name = "";
    metadata.modification_time.additional_properties.SetString(
        "value", "invalid-date-time");  // Invalid modification time is OK.
    metadata.thumbnail.reset(new std::string(kValidThumbnailUrl));
    EXPECT_FALSE(ValidateIDLEntryMetadata(metadata, false /* root_path */));
  }

  // Missing date time.
  {
    EntryMetadata metadata;
    metadata.name = kValidFileName;
    metadata.thumbnail.reset(new std::string(kValidThumbnailUrl));
    EXPECT_FALSE(ValidateIDLEntryMetadata(metadata, false /* root_path */));
  }

  // Invalid thumbnail.
  {
    EntryMetadata metadata;
    metadata.name = kValidFileName;
    metadata.modification_time.additional_properties.SetString(
        "value", "invalid-date-time");  // Invalid modification time is OK.
    metadata.thumbnail.reset(new std::string("http://invalid-scheme"));
    EXPECT_FALSE(ValidateIDLEntryMetadata(metadata, false /* root_path */));
  }
}

TEST_F(FileSystemProviderOperationsGetMetadataTest, Execute) {
  using extensions::api::file_system_provider::GetMetadataRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  GetMetadata get_metadata(
      NULL, file_system_info_, base::FilePath(kDirectoryPath),
      ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL,
      base::Bind(&CallbackLogger::OnGetMetadata,
                 base::Unretained(&callback_logger)));
  get_metadata.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(get_metadata.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0];
  EXPECT_EQ(
      extensions::api::file_system_provider::OnGetMetadataRequested::kEventName,
      event->event_name);
  base::ListValue* event_args = event->event_args.get();
  ASSERT_EQ(1u, event_args->GetSize());

  const base::DictionaryValue* options_as_value = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &options_as_value));

  GetMetadataRequestedOptions options;
  ASSERT_TRUE(
      GetMetadataRequestedOptions::Populate(*options_as_value, &options));
  EXPECT_EQ(kFileSystemId, options.file_system_id);
  EXPECT_EQ(kRequestId, options.request_id);
  EXPECT_EQ(kDirectoryPath, options.entry_path);
  EXPECT_TRUE(options.thumbnail);
}

TEST_F(FileSystemProviderOperationsGetMetadataTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  CallbackLogger callback_logger;

  GetMetadata get_metadata(
      NULL, file_system_info_, base::FilePath(kDirectoryPath),
      ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL,
      base::Bind(&CallbackLogger::OnGetMetadata,
                 base::Unretained(&callback_logger)));
  get_metadata.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_FALSE(get_metadata.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsGetMetadataTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  GetMetadata get_metadata(
      NULL, file_system_info_, base::FilePath(kDirectoryPath),
      ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL,
      base::Bind(&CallbackLogger::OnGetMetadata,
                 base::Unretained(&callback_logger)));
  get_metadata.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(get_metadata.Execute(kRequestId));

  // Sample input as JSON. Keep in sync with file_system_provider_api.idl.
  // As for now, it is impossible to create *::Params class directly, not from
  // base::Value.
  const std::string input =
      "[\n"
      "  \"testing-file-system\",\n"  // kFileSystemId
      "  2,\n"                        // kRequestId
      "  {\n"
      "    \"isDirectory\": false,\n"
      "    \"name\": \"blueberries.txt\",\n"
      "    \"size\": 4096,\n"
      "    \"modificationTime\": {\n"
      "      \"value\": \"Thu Apr 24 00:46:52 UTC 2014\"\n"
      "    },\n"
      "    \"mimeType\": \"text/plain\",\n"              // kMimeType
      "    \"thumbnail\": \"DaTa:ImAgE/pNg;base64,\"\n"  // kThumbnail
      "  },\n"
      "  0\n"  // execution_time
      "]\n";
  scoped_ptr<RequestValue> request_value;
  ASSERT_NO_FATAL_FAILURE(CreateRequestValueFromJSON(input, &request_value));

  const bool has_more = false;
  get_metadata.OnSuccess(kRequestId, request_value.Pass(), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_OK, event->result());

  const EntryMetadata* metadata = event->metadata();
  EXPECT_FALSE(metadata->is_directory);
  EXPECT_EQ(4096, metadata->size);
  base::Time expected_time;
  EXPECT_TRUE(
      base::Time::FromString("Thu Apr 24 00:46:52 UTC 2014", &expected_time));
  EXPECT_EQ(expected_time, metadata->modification_time);
  EXPECT_EQ(kMimeType, metadata->mime_type);
  EXPECT_EQ(kThumbnail, metadata->thumbnail);
}

TEST_F(FileSystemProviderOperationsGetMetadataTest, OnSuccess_InvalidMetadata) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  GetMetadata get_metadata(
      NULL, file_system_info_, base::FilePath(kDirectoryPath),
      ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL,
      base::Bind(&CallbackLogger::OnGetMetadata,
                 base::Unretained(&callback_logger)));
  get_metadata.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(get_metadata.Execute(kRequestId));

  // Sample input as JSON. Keep in sync with file_system_provider_api.idl.
  // As for now, it is impossible to create *::Params class directly, not from
  // base::Value.
  const std::string input =
      "[\n"
      "  \"testing-file-system\",\n"  // kFileSystemId
      "  2,\n"                        // kRequestId
      "  {\n"
      "    \"isDirectory\": false,\n"
      "    \"name\": \"blue/berries.txt\",\n"
      "    \"size\": 4096,\n"
      "    \"modificationTime\": {\n"
      "      \"value\": \"Thu Apr 24 00:46:52 UTC 2014\"\n"
      "    },\n"
      "    \"mimeType\": \"text/plain\",\n"                  // kMimeType
      "    \"thumbnail\": \"http://www.foobar.com/evil\"\n"  // kThumbnail
      "  },\n"
      "  0\n"  // execution_time
      "]\n";

  scoped_ptr<RequestValue> request_value;
  ASSERT_NO_FATAL_FAILURE(CreateRequestValueFromJSON(input, &request_value));

  const bool has_more = false;
  get_metadata.OnSuccess(kRequestId, request_value.Pass(), has_more);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_IO, event->result());

  const EntryMetadata* metadata = event->metadata();
  EXPECT_FALSE(metadata);
}

TEST_F(FileSystemProviderOperationsGetMetadataTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  CallbackLogger callback_logger;

  GetMetadata get_metadata(
      NULL, file_system_info_, base::FilePath(kDirectoryPath),
      ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL,
      base::Bind(&CallbackLogger::OnGetMetadata,
                 base::Unretained(&callback_logger)));
  get_metadata.SetDispatchEventImplForTesting(
      base::Bind(&util::LoggingDispatchEventImpl::OnDispatchEventImpl,
                 base::Unretained(&dispatcher)));

  EXPECT_TRUE(get_metadata.Execute(kRequestId));

  get_metadata.OnError(kRequestId,
                       scoped_ptr<RequestValue>(new RequestValue()),
                       base::File::FILE_ERROR_TOO_MANY_OPENED);

  ASSERT_EQ(1u, callback_logger.events().size());
  CallbackLogger::Event* event = callback_logger.events()[0];
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, event->result());
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
