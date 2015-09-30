// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_data_stream_builder.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "content/browser/fileapi/upload_file_system_file_element_reader.h"
#include "content/common/resource_request_body.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_disk_cache_entry_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace disk_cache {
class Entry;
}

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

  ~BytesElementReader() override {}

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

  ~FileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

// This owns the provided ResourceRequestBody. This is necessary to ensure the
// BlobData and open disk cache entries survive until upload completion.
class DiskCacheElementReader : public net::UploadDiskCacheEntryElementReader {
 public:
  DiskCacheElementReader(ResourceRequestBody* resource_request_body,
                         disk_cache::Entry* disk_cache_entry,
                         int disk_cache_stream_index,
                         const ResourceRequestBody::Element& element)
      : net::UploadDiskCacheEntryElementReader(disk_cache_entry,
                                               disk_cache_stream_index,
                                               element.offset(),
                                               element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY,
              element.type());
  }

  ~DiskCacheElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(DiskCacheElementReader);
};

void ResolveBlobReference(
    ResourceRequestBody* body,
    storage::BlobStorageContext* blob_context,
    const ResourceRequestBody::Element& element,
    std::vector<std::pair<const ResourceRequestBody::Element*,
                          const storage::BlobDataItem*>>* resolved_elements) {
  DCHECK(blob_context);
  scoped_ptr<storage::BlobDataHandle> handle =
      blob_context->GetBlobDataFromUUID(element.blob_uuid());
  DCHECK(handle);
  if (!handle)
    return;

  // TODO(dmurph): Create a reader for blobs instead of decomposing the blob
  // and storing the snapshot on the request to keep the resources around.
  // Currently a handle is attached to the request in the resource dispatcher
  // host, so we know the blob won't go away, but it's not very clear or useful.
  scoped_ptr<storage::BlobDataSnapshot> snapshot = handle->CreateSnapshot();
  // If there is no element in the referred blob data, just return.
  if (snapshot->items().empty())
    return;

  // Append the elements in the referenced blob data.
  for (const auto& item : snapshot->items()) {
    DCHECK_NE(storage::DataElement::TYPE_BLOB, item->type());
    resolved_elements->push_back(
        std::make_pair(item->data_element_ptr(), item.get()));
  }
  const void* key = snapshot.get();
  body->SetUserData(key, snapshot.release());
}

}  // namespace

scoped_ptr<net::UploadDataStream> UploadDataStreamBuilder::Build(
    ResourceRequestBody* body,
    storage::BlobStorageContext* blob_context,
    storage::FileSystemContext* file_system_context,
    base::TaskRunner* file_task_runner) {
  // Resolve all blob elements.
  std::vector<std::pair<const ResourceRequestBody::Element*,
                        const storage::BlobDataItem*>> resolved_elements;
  for (size_t i = 0; i < body->elements()->size(); ++i) {
    const ResourceRequestBody::Element& element = (*body->elements())[i];
    if (element.type() == ResourceRequestBody::Element::TYPE_BLOB) {
      ResolveBlobReference(body, blob_context, element, &resolved_elements);
    } else if (element.type() !=
               ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY) {
      resolved_elements.push_back(std::make_pair(&element, nullptr));
    } else {
      NOTREACHED();
    }
  }

  ScopedVector<net::UploadElementReader> element_readers;
  for (const auto& element_and_blob_item_pair : resolved_elements) {
    const ResourceRequestBody::Element& element =
        *element_and_blob_item_pair.first;
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        element_readers.push_back(new BytesElementReader(body, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        element_readers.push_back(
            new FileElementReader(body, file_task_runner, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        // If |body| contains any filesystem URLs, the caller should have
        // supplied a FileSystemContext.
        DCHECK(file_system_context);
        element_readers.push_back(
            new content::UploadFileSystemFileElementReader(
                file_system_context,
                element.filesystem_url(),
                element.offset(),
                element.length(),
                element.expected_modification_time()));
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        // TODO(dmurph): Create blob reader and store the snapshot in there.
        NOTREACHED();
        break;
      case ResourceRequestBody::Element::TYPE_DISK_CACHE_ENTRY: {
        // TODO(gavinp): If Build() is called with a DataElement of
        // TYPE_DISK_CACHE_ENTRY then this code won't work because we won't call
        // ResolveBlobReference() and so we won't find |item|. Is this OK?
        const storage::BlobDataItem* item = element_and_blob_item_pair.second;
        element_readers.push_back(
            new DiskCacheElementReader(body, item->disk_cache_entry(),
                                       item->disk_cache_stream_index(),
                                       element));
        break;
      }
      case ResourceRequestBody::Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  return make_scoped_ptr(
      new net::ElementsUploadDataStream(element_readers.Pass(),
                                        body->identifier()));
}

}  // namespace content
