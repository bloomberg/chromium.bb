// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webblobregistry_impl.h"

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_thread_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/fileapi/webblob_messages.h"
#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThreadSafeData.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebBlobData;
using blink::WebString;
using blink::WebThreadSafeData;
using blink::WebURL;
using blink::WebBlobRegistry;
using storage::DataElement;

namespace content {

namespace {

const size_t kLargeThresholdBytes = 250 * 1024;
const size_t kMaxSharedMemoryBytes = 10 * 1024 * 1024;

}  // namespace

WebBlobRegistryImpl::WebBlobRegistryImpl(ThreadSafeSender* sender)
    : sender_(sender) {
  // Record a dummy trace event on startup so the 'Storage' category shows up
  // in the chrome://tracing viewer.
  TRACE_EVENT0("Blob", "Init");
}

WebBlobRegistryImpl::~WebBlobRegistryImpl() {
}

blink::WebBlobRegistry::Builder* WebBlobRegistryImpl::createBuilder(
    const blink::WebString& uuid,
    const blink::WebString& contentType) {
  return new BuilderImpl(uuid, contentType, sender_.get());
}

void WebBlobRegistryImpl::registerBlobData(const blink::WebString& uuid,
                                           const blink::WebBlobData& data) {
  TRACE_EVENT0("Blob", "Registry::RegisterBlob");
  scoped_ptr<Builder> builder(createBuilder(uuid, data.contentType()));

  // This is temporary until we move to createBuilder() as our blob creation
  // method.
  size_t i = 0;
  WebBlobData::Item data_item;
  while (data.itemAt(i++, data_item)) {
    if (data_item.length == 0) {
      continue;
    }
    switch (data_item.type) {
      case WebBlobData::Item::TypeData: {
        // WebBlobData does not allow partial data items.
        DCHECK(!data_item.offset && data_item.length == -1);
        builder->appendData(data_item.data);
        break;
      }
      case WebBlobData::Item::TypeFile:
        builder->appendFile(data_item.filePath,
                            static_cast<uint64_t>(data_item.offset),
                            static_cast<uint64_t>(data_item.length),
                            data_item.expectedModificationTime);
        break;
      case WebBlobData::Item::TypeBlob:
        builder->appendBlob(data_item.blobUUID, data_item.offset,
                            data_item.length);
        break;
      case WebBlobData::Item::TypeFileSystemURL:
        // We only support filesystem URL as of now.
        DCHECK(GURL(data_item.fileSystemURL).SchemeIsFileSystem());
        builder->appendFileSystemURL(data_item.fileSystemURL,
                                     static_cast<uint64_t>(data_item.offset),
                                     static_cast<uint64_t>(data_item.length),
                                     data_item.expectedModificationTime);
        break;
      default:
        NOTREACHED();
    }
  }
  builder->build();
}

void WebBlobRegistryImpl::addBlobDataRef(const WebString& uuid) {
  sender_->Send(new BlobHostMsg_IncrementRefCount(uuid.utf8()));
}

void WebBlobRegistryImpl::removeBlobDataRef(const WebString& uuid) {
  sender_->Send(new BlobHostMsg_DecrementRefCount(uuid.utf8()));
}

void WebBlobRegistryImpl::registerPublicBlobURL(const WebURL& url,
                                                const WebString& uuid) {
  sender_->Send(new BlobHostMsg_RegisterPublicURL(url, uuid.utf8()));
}

void WebBlobRegistryImpl::revokePublicBlobURL(const WebURL& url) {
  sender_->Send(new BlobHostMsg_RevokePublicURL(url));
}

// ------ streams stuff -----

void WebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                            const WebString& content_type) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_StartBuilding(url, content_type.utf8()));
}

void WebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                            const WebURL& src_url) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_Clone(url, src_url));
}

void WebBlobRegistryImpl::addDataToStream(const WebURL& url,
                                          const char* data,
                                          size_t length) {
  DCHECK(ChildThreadImpl::current());
  if (length == 0)
    return;
  if (length < kLargeThresholdBytes) {
    DataElement item;
    item.SetToBytes(data, length);
    sender_->Send(new StreamHostMsg_AppendBlobDataItem(url, item));
  } else {
    // We handle larger amounts of data via SharedMemory instead of
    // writing it directly to the IPC channel.
    size_t shared_memory_size = std::min(length, kMaxSharedMemoryBytes);
    scoped_ptr<base::SharedMemory> shared_memory(
        ChildThreadImpl::AllocateSharedMemory(shared_memory_size,
                                              sender_.get()));
    CHECK(shared_memory.get());
    if (!shared_memory->Map(shared_memory_size))
      CHECK(false);

    size_t remaining_bytes = length;
    const char* current_ptr = data;
    while (remaining_bytes) {
      size_t chunk_size = std::min(remaining_bytes, shared_memory_size);
      memcpy(shared_memory->memory(), current_ptr, chunk_size);
      sender_->Send(new StreamHostMsg_SyncAppendSharedMemory(
          url, shared_memory->handle(),
          base::checked_cast<uint32_t>(chunk_size)));
      remaining_bytes -= chunk_size;
      current_ptr += chunk_size;
    }
  }
}

void WebBlobRegistryImpl::flushStream(const WebURL& url) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_Flush(url));
}

void WebBlobRegistryImpl::finalizeStream(const WebURL& url) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_FinishBuilding(url));
}

void WebBlobRegistryImpl::abortStream(const WebURL& url) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_AbortBuilding(url));
}

void WebBlobRegistryImpl::unregisterStreamURL(const WebURL& url) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_Remove(url));
}

WebBlobRegistryImpl::BuilderImpl::BuilderImpl(
    const blink::WebString& uuid,
    const blink::WebString& content_type,
    ThreadSafeSender* sender)
    : uuid_(uuid.utf8()), content_type_(content_type.utf8()), sender_(sender) {
}

WebBlobRegistryImpl::BuilderImpl::~BuilderImpl() {
}

void WebBlobRegistryImpl::BuilderImpl::appendData(
    const WebThreadSafeData& data) {
  consolidation_.AddDataItem(data);
}

void WebBlobRegistryImpl::BuilderImpl::appendBlob(const WebString& uuid,
                                                  uint64_t offset,
                                                  uint64_t length) {
  consolidation_.AddBlobItem(uuid.utf8(), offset, length);
}

void WebBlobRegistryImpl::BuilderImpl::appendFile(
    const WebString& path,
    uint64_t offset,
    uint64_t length,
    double expected_modification_time) {
  consolidation_.AddFileItem(
      base::FilePath::FromUTF16Unsafe(base::string16(path)), offset, length,
      expected_modification_time);
}

void WebBlobRegistryImpl::BuilderImpl::appendFileSystemURL(
    const WebURL& fileSystemURL,
    uint64_t offset,
    uint64_t length,
    double expected_modification_time) {
  DCHECK(GURL(fileSystemURL).SchemeIsFileSystem());
  consolidation_.AddFileSystemItem(GURL(fileSystemURL), offset, length,
                                   expected_modification_time);
}

void WebBlobRegistryImpl::BuilderImpl::build() {
  sender_->Send(new BlobHostMsg_StartBuilding(uuid_));
  const auto& items = consolidation_.consolidated_items();

  // We still need a buffer to hold the continuous block of data so the
  // DataElement can hold it.
  size_t buffer_size = 0;
  scoped_ptr<char[]> buffer;
  for (size_t i = 0; i < items.size(); i++) {
    const BlobConsolidation::ConsolidatedItem& item = items[i];
    DataElement element;
    // NOTE: length == -1 when we want to use the whole file.  This
    // only happens when we are creating a file object in Blink, and the file
    // object is the only item in the 'blob'.  If we use that file blob to
    // create another blob, it is sent here as a 'file' item and not a blob,
    // and the correct size is populated.
    // static_cast<uint64_t>(-1) == kuint64max, which is what DataElement uses
    // to specificy "use the whole file".
    switch (item.type) {
      case DataElement::TYPE_BYTES:
        if (item.length > kLargeThresholdBytes) {
          SendOversizedDataForBlob(i);
          break;
        }
        if (buffer_size < item.length) {
          buffer.reset(new char[item.length]);
          buffer_size = item.length;
        }
        consolidation_.ReadMemory(i, 0, item.length, buffer.get());
        element.SetToSharedBytes(buffer.get(), item.length);
        sender_->Send(new BlobHostMsg_AppendBlobDataItem(uuid_, element));
        break;
      case DataElement::TYPE_FILE:
        element.SetToFilePathRange(
            item.path, item.offset, item.length,
            base::Time::FromDoubleT(item.expected_modification_time));
        sender_->Send(new BlobHostMsg_AppendBlobDataItem(uuid_, element));
        break;
      case DataElement::TYPE_BLOB:
        element.SetToBlobRange(item.blob_uuid, item.offset, item.length);
        sender_->Send(new BlobHostMsg_AppendBlobDataItem(uuid_, element));
        break;
      case DataElement::TYPE_FILE_FILESYSTEM:
        element.SetToFileSystemUrlRange(
            item.filesystem_url, item.offset, item.length,
            base::Time::FromDoubleT(item.expected_modification_time));
        sender_->Send(new BlobHostMsg_AppendBlobDataItem(uuid_, element));
        break;
      default:
        NOTREACHED();
    }
  }
  sender_->Send(new BlobHostMsg_FinishBuilding(uuid_, content_type_));
}

void WebBlobRegistryImpl::BuilderImpl::SendOversizedDataForBlob(
    size_t consolidated_item_index) {
  TRACE_EVENT0("Blob", "Registry::SendOversizedBlobData");
  const BlobConsolidation::ConsolidatedItem& item =
      consolidation_.consolidated_items()[consolidated_item_index];
  // We handle larger amounts of data via SharedMemory instead of
  // writing it directly to the IPC channel.

  size_t data_size = item.length;
  size_t shared_memory_size = std::min(data_size, kMaxSharedMemoryBytes);
  scoped_ptr<base::SharedMemory> shared_memory(
      ChildThreadImpl::AllocateSharedMemory(shared_memory_size, sender_.get()));
  CHECK(shared_memory.get());
  const bool mapped = shared_memory->Map(shared_memory_size);
  CHECK(mapped) << "Unable to map shared memory.";

  size_t offset = 0;
  while (data_size) {
    TRACE_EVENT0("Blob", "Registry::SendOversizedBlobItem");
    size_t chunk_size = std::min(data_size, shared_memory_size);
    consolidation_.ReadMemory(consolidated_item_index, offset, chunk_size,
                              shared_memory->memory());
    sender_->Send(new BlobHostMsg_SyncAppendSharedMemory(
        uuid_, shared_memory->handle(), static_cast<uint32_t>(chunk_size)));
    data_size -= chunk_size;
    offset += chunk_size;
  }
}

}  // namespace content
