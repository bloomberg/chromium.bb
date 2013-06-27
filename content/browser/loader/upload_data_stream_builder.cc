// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_data_stream_builder.h"

#include "base/logging.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "webkit/browser/blob/blob_storage_controller.h"
#include "webkit/browser/fileapi/upload_file_system_file_element_reader.h"
#include "webkit/common/resource_request_body.h"

using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;
using webkit_glue::ResourceRequestBody;

namespace content {
namespace {

// A subclass of net::UploadBytesElementReader which owns ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const ResourceRequestBody::Element& element)
      : net::UploadBytesElementReader(element.bytes(), element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_BYTES, element.type());
  }

  virtual ~BytesElementReader() {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const ResourceRequestBody::Element& element)
      : net::UploadFileElementReader(task_runner,
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_FILE, element.type());
  }

  virtual ~FileElementReader() {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

void ResolveBlobReference(
    ResourceRequestBody* body,
    webkit_blob::BlobStorageController* blob_controller,
    const GURL& blob_url,
    std::vector<const ResourceRequestBody::Element*>* resolved_elements) {
  DCHECK(blob_controller);
  BlobData* blob_data = blob_controller->GetBlobDataFromUrl(blob_url);
  DCHECK(blob_data);
  if (!blob_data)
    return;

  // If there is no element in the referred blob data, just return.
  if (blob_data->items().empty())
    return;

  // Ensure the blob and any attached shareable files survive until
  // upload completion.
  body->SetUserData(blob_data, new base::UserDataAdapter<BlobData>(blob_data));

  // Append the elements in the referred blob data.
  for (size_t i = 0; i < blob_data->items().size(); ++i) {
    const BlobData::Item& item = blob_data->items().at(i);
    DCHECK_NE(BlobData::Item::TYPE_BLOB, item.type());
    resolved_elements->push_back(&item);
  }
}

}  // namespace

scoped_ptr<net::UploadDataStream> UploadDataStreamBuilder::Build(
    ResourceRequestBody* body,
    BlobStorageController* blob_controller,
    fileapi::FileSystemContext* file_system_context,
    base::TaskRunner* file_task_runner) {
  // Resolve all blob elements.
  std::vector<const ResourceRequestBody::Element*> resolved_elements;
  for (size_t i = 0; i < body->elements()->size(); ++i) {
    const ResourceRequestBody::Element& element = (*body->elements())[i];
    if (element.type() == ResourceRequestBody::Element::TYPE_BLOB) {
      ResolveBlobReference(body, blob_controller, element.url(),
                           &resolved_elements);
    } else {
      // No need to resolve, just append the element.
      resolved_elements.push_back(&element);
    }
  }

  ScopedVector<net::UploadElementReader> element_readers;
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const ResourceRequestBody::Element& element = *resolved_elements[i];
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        element_readers.push_back(new BytesElementReader(body, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        element_readers.push_back(
            new FileElementReader(body, file_task_runner, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        element_readers.push_back(
            new fileapi::UploadFileSystemFileElementReader(
                file_system_context,
                element.url(),
                element.offset(),
                element.length(),
                element.expected_modification_time()));
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        NOTREACHED();
        break;
      case ResourceRequestBody::Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  return make_scoped_ptr(
      new net::UploadDataStream(&element_readers, body->identifier()));
}

}  // namespace content
