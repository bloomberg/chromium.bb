// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_data_stream_builder.h"

#include <algorithm>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/common/resource_request_body.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using storage::BlobDataBuilder;
using storage::BlobDataHandle;
using storage::BlobStorageContext;

namespace content {
namespace {

bool AreElementsEqual(const net::UploadElementReader& reader,
                      const ResourceRequestBody::Element& element) {
  switch(element.type()) {
    case ResourceRequestBody::Element::TYPE_BYTES: {
      const net::UploadBytesElementReader* bytes_reader =
          reader.AsBytesReader();
      return bytes_reader &&
          element.length() == bytes_reader->length() &&
          std::equal(element.bytes(), element.bytes() + element.length(),
                     bytes_reader->bytes());
    }
    case ResourceRequestBody::Element::TYPE_FILE: {
      const net::UploadFileElementReader* file_reader = reader.AsFileReader();
      return file_reader &&
          file_reader->path() == element.path() &&
          file_reader->range_offset() == element.offset() &&
          file_reader->range_length() == element.length() &&
          file_reader->expected_modification_time() ==
          element.expected_modification_time();
      break;
    }
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace

TEST(UploadDataStreamBuilderTest, CreateUploadDataStreamWithoutBlob) {
  base::MessageLoop message_loop;
  scoped_refptr<ResourceRequestBody> request_body = new ResourceRequestBody;

  const char kData[] = "123";
  const base::FilePath::StringType kFilePath = FILE_PATH_LITERAL("abc");
  const uint64 kFileOffset = 10U;
  const uint64 kFileLength = 100U;
  const base::Time kFileTime = base::Time::FromDoubleT(999);
  const int64 kIdentifier = 12345;

  request_body->AppendBytes(kData, arraysize(kData) - 1);
  request_body->AppendFileRange(base::FilePath(kFilePath),
                                kFileOffset, kFileLength, kFileTime);
  request_body->set_identifier(kIdentifier);

  scoped_ptr<net::UploadDataStream> upload(UploadDataStreamBuilder::Build(
      request_body.get(), NULL, NULL, base::MessageLoopProxy::current().get()));

  EXPECT_EQ(kIdentifier, upload->identifier());
  ASSERT_TRUE(upload->GetElementReaders());
  ASSERT_EQ(request_body->elements()->size(),
            upload->GetElementReaders()->size());

  const net::UploadBytesElementReader* r1 =
      (*upload->GetElementReaders())[0]->AsBytesReader();
  ASSERT_TRUE(r1);
  EXPECT_EQ(kData, std::string(r1->bytes(), r1->length()));

  const net::UploadFileElementReader* r2 =
      (*upload->GetElementReaders())[1]->AsFileReader();
  ASSERT_TRUE(r2);
  EXPECT_EQ(kFilePath, r2->path().value());
  EXPECT_EQ(kFileOffset, r2->range_offset());
  EXPECT_EQ(kFileLength, r2->range_length());
  EXPECT_EQ(kFileTime, r2->expected_modification_time());
}

TEST(UploadDataStreamBuilderTest, ResolveBlobAndCreateUploadDataStream) {
  base::MessageLoop message_loop;
  {
    // Setup blob data for testing.
    base::Time time1, time2;
    base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
    base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

    BlobStorageContext blob_storage_context;

    const std::string blob_id0("id-0");
    scoped_ptr<BlobDataBuilder> blob_data_builder(
        new BlobDataBuilder(blob_id0));
    scoped_ptr<BlobDataHandle> handle1 =
        blob_storage_context.AddFinishedBlob(blob_data_builder.get());

    const std::string blob_id1("id-1");
    const std::string kBlobData = "BlobData";
    blob_data_builder.reset(new BlobDataBuilder(blob_id1));
    blob_data_builder->AppendData(kBlobData);
    blob_data_builder->AppendFile(
        base::FilePath(FILE_PATH_LITERAL("BlobFile.txt")), 0, 20, time1);
    scoped_ptr<BlobDataHandle> handle2 =
        blob_storage_context.AddFinishedBlob(blob_data_builder.get());

    // Setup upload data elements for comparison.
    ResourceRequestBody::Element blob_element1, blob_element2;
    blob_element1.SetToBytes(kBlobData.c_str(), kBlobData.size());
    blob_element2.SetToFilePathRange(
        base::FilePath(FILE_PATH_LITERAL("BlobFile.txt")), 0, 20, time1);

    ResourceRequestBody::Element upload_element1, upload_element2;
    upload_element1.SetToBytes("Hello", 5);
    upload_element2.SetToFilePathRange(
        base::FilePath(FILE_PATH_LITERAL("foo1.txt")), 0, 20, time2);

    // Test no blob reference.
    scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    scoped_ptr<net::UploadDataStream> upload(
        UploadDataStreamBuilder::Build(
            request_body.get(),
            &blob_storage_context,
            NULL,
            base::MessageLoopProxy::current().get()));

    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(2U, upload->GetElementReaders()->size());
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[0], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[1], upload_element2));

    // Test having only one blob reference that refers to empty blob data.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id0);

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(0U, upload->GetElementReaders()->size());

    // Test having only one blob reference.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id1);

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(2U, upload->GetElementReaders()->size());
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[0], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[1], blob_element2));

    // Test having one blob reference at the beginning.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id1);
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(4U, upload->GetElementReaders()->size());
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[0], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[1], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[2], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[3], upload_element2));

    // Test having one blob reference at the end.
    request_body = new ResourceRequestBody();
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());
    request_body->AppendBlob(blob_id1);

    upload =
        UploadDataStreamBuilder::Build(request_body.get(),
                                       &blob_storage_context,
                                       NULL,
                                       base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(4U, upload->GetElementReaders()->size());
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[0], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[1], upload_element2));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[2], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[3], blob_element2));

    // Test having one blob reference in the middle.
    request_body = new ResourceRequestBody();
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendBlob(blob_id1);
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(4U, upload->GetElementReaders()->size());
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[0], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[1], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[2], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[3], upload_element2));

    // Test having multiple blob references.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id1);
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendBlob(blob_id1);
    request_body->AppendBlob(blob_id1);
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    ASSERT_EQ(8U, upload->GetElementReaders()->size());
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[0], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[1], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[2], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[3], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[4], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[5], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[6], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *(*upload->GetElementReaders())[7], upload_element2));
  }
  // Clean up for ASAN.
  base::RunLoop().RunUntilIdle();
}

TEST(UploadDataStreamBuilderTest,
     WriteUploadDataStreamWithEmptyFileBackedBlob) {
  base::MessageLoopForIO message_loop;
  {
    base::FilePath test_blob_path;
    ASSERT_TRUE(base::CreateTemporaryFile(&test_blob_path));

    const uint64_t kZeroLength = 0;
    base::Time blob_time;
    base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &blob_time);
    ASSERT_TRUE(base::TouchFile(test_blob_path, blob_time, blob_time));

    BlobStorageContext blob_storage_context;

    // A blob created from an empty file added several times.
    const std::string blob_id("id-0");
    scoped_ptr<BlobDataBuilder> blob_data_builder(new BlobDataBuilder(blob_id));
    blob_data_builder->AppendFile(test_blob_path, 0, kZeroLength, blob_time);
    scoped_ptr<BlobDataHandle> handle =
        blob_storage_context.AddFinishedBlob(blob_data_builder.get());

    ResourceRequestBody::Element blob_element;
    blob_element.SetToFilePathRange(test_blob_path, 0, kZeroLength, blob_time);

    scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
    scoped_ptr<net::UploadDataStream> upload(UploadDataStreamBuilder::Build(
        request_body.get(), &blob_storage_context, NULL,
        base::MessageLoopProxy::current().get()));

    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id);
    request_body->AppendBlob(blob_id);
    request_body->AppendBlob(blob_id);

    upload = UploadDataStreamBuilder::Build(
        request_body.get(), &blob_storage_context, NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_TRUE(upload->GetElementReaders());
    const auto& readers = *upload->GetElementReaders();
    ASSERT_EQ(3U, readers.size());
    EXPECT_TRUE(AreElementsEqual(*readers[0], blob_element));
    EXPECT_TRUE(AreElementsEqual(*readers[1], blob_element));
    EXPECT_TRUE(AreElementsEqual(*readers[2], blob_element));

    net::TestCompletionCallback init_callback;
    ASSERT_EQ(net::ERR_IO_PENDING, upload->Init(init_callback.callback()));
    EXPECT_EQ(net::OK, init_callback.WaitForResult());

    EXPECT_EQ(kZeroLength, upload->size());

    // Purposely (try to) read more than what is in the stream. If we try to
    // read zero bytes then UploadDataStream::Read will fail a DCHECK.
    int kBufferLength = kZeroLength + 1;
    scoped_ptr<char[]> buffer(new char[kBufferLength]);
    scoped_refptr<net::IOBuffer> io_buffer =
        new net::WrappedIOBuffer(buffer.get());
    net::TestCompletionCallback read_callback;
    int result =
        upload->Read(io_buffer.get(), kBufferLength, read_callback.callback());
    EXPECT_EQ(static_cast<int>(kZeroLength), read_callback.GetResult(result));

    base::DeleteFile(test_blob_path, false);
  }
  // Clean up for ASAN.
  base::RunLoop().RunUntilIdle();
}
}  // namespace content
