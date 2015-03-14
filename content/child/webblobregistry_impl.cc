// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webblobregistry_impl.h"

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
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

void WebBlobRegistryImpl::registerBlobData(
    const blink::WebString& uuid, const blink::WebBlobData& data) {
  TRACE_EVENT0("Blob", "Registry::RegisterBlob");
  const std::string uuid_str(uuid.utf8());

  storage::DataElement data_buffer;
  data_buffer.SetToEmptyBytes();

  sender_->Send(new BlobHostMsg_StartBuilding(uuid_str));
  size_t i = 0;
  WebBlobData::Item data_item;
  while (data.itemAt(i++, data_item)) {
    // NOTE: data_item.length == -1 when we want to use the whole file.  This
    // only happens when we are creating a file object in Blink, and the file
    // object is the only item in the 'blob'.  If we use that file blob to
    // create another blob, it is sent here as a 'file' item and not a blob,
    // and the correct size is populated.
    // static_cast<uint64>(-1) == kuint64max, which is what DataElement uses
    // to specificy "use the whole file".
    if (data_item.length == 0) {
      continue;
    }
    if (data_item.type != WebBlobData::Item::TypeData &&
        data_buffer.length() != 0) {
      FlushBlobItemBuffer(uuid_str, &data_buffer);
    }
    storage::DataElement item;
    switch (data_item.type) {
      case WebBlobData::Item::TypeData: {
        // WebBlobData does not allow partial data items.
        DCHECK(!data_item.offset && data_item.length == -1);
        if (data_item.data.size() == 0) {
          continue;
        }
        BufferBlobData(uuid_str, data_item.data, &data_buffer);
        break;
      }
      case WebBlobData::Item::TypeFile:
        item.SetToFilePathRange(
            base::FilePath::FromUTF16Unsafe(data_item.filePath),
            static_cast<uint64>(data_item.offset),
            static_cast<uint64>(data_item.length),
            base::Time::FromDoubleT(data_item.expectedModificationTime));
        sender_->Send(new BlobHostMsg_AppendBlobDataItem(uuid_str, item));
        break;
      case WebBlobData::Item::TypeBlob:
        item.SetToBlobRange(
            data_item.blobUUID.utf8(),
            static_cast<uint64>(data_item.offset),
            static_cast<uint64>(data_item.length));
        sender_->Send(
            new BlobHostMsg_AppendBlobDataItem(uuid_str, item));
        break;
      case WebBlobData::Item::TypeFileSystemURL:
        // We only support filesystem URL as of now.
        DCHECK(GURL(data_item.fileSystemURL).SchemeIsFileSystem());
        item.SetToFileSystemUrlRange(
            data_item.fileSystemURL,
            static_cast<uint64>(data_item.offset),
            static_cast<uint64>(data_item.length),
            base::Time::FromDoubleT(data_item.expectedModificationTime));
        sender_->Send(
            new BlobHostMsg_AppendBlobDataItem(uuid_str, item));
        break;
      default:
        NOTREACHED();
    }
  }
  if (data_buffer.length() != 0) {
    FlushBlobItemBuffer(uuid_str, &data_buffer);
  }
  sender_->Send(new BlobHostMsg_FinishBuilding(
      uuid_str, data.contentType().utf8().data()));
}

void WebBlobRegistryImpl::addBlobDataRef(const WebString& uuid) {
  sender_->Send(new BlobHostMsg_IncrementRefCount(uuid.utf8()));
}

void WebBlobRegistryImpl::removeBlobDataRef(const WebString& uuid) {
  sender_->Send(new BlobHostMsg_DecrementRefCount(uuid.utf8()));
}

void WebBlobRegistryImpl::registerPublicBlobURL(
    const WebURL& url, const WebString& uuid) {
  sender_->Send(new BlobHostMsg_RegisterPublicURL(url, uuid.utf8()));
}

void WebBlobRegistryImpl::revokePublicBlobURL(const WebURL& url) {
  sender_->Send(new BlobHostMsg_RevokePublicURL(url));
}

void WebBlobRegistryImpl::FlushBlobItemBuffer(
    const std::string& uuid_str,
    storage::DataElement* data_buffer) const {
  DCHECK_NE(data_buffer->length(), 0ul);
  DCHECK_LT(data_buffer->length(), kLargeThresholdBytes);
  sender_->Send(new BlobHostMsg_AppendBlobDataItem(uuid_str, *data_buffer));
  data_buffer->SetToEmptyBytes();
}

void WebBlobRegistryImpl::BufferBlobData(const std::string& uuid_str,
                                         const blink::WebThreadSafeData& data,
                                         storage::DataElement* data_buffer) {
  size_t buffer_size = data_buffer->length();
  size_t data_size = data.size();
  DCHECK_NE(data_size, 0ul);
  if (buffer_size != 0 && buffer_size + data_size >= kLargeThresholdBytes) {
    FlushBlobItemBuffer(uuid_str, data_buffer);
    buffer_size = 0;
  }
  if (data_size >= kLargeThresholdBytes) {
    TRACE_EVENT0("Blob", "Registry::SendOversizedBlobData");
    SendOversizedDataForBlob(uuid_str, data);
  } else {
    DCHECK_LT(buffer_size + data_size, kLargeThresholdBytes);
    data_buffer->AppendBytes(data.data(), data_size);
  }
}

void WebBlobRegistryImpl::SendOversizedDataForBlob(
    const std::string& uuid_str,
    const blink::WebThreadSafeData& data) {
  DCHECK_GE(data.size(), kLargeThresholdBytes);
  // We handle larger amounts of data via SharedMemory instead of
  // writing it directly to the IPC channel.
  size_t shared_memory_size = std::min(data.size(), kMaxSharedMemoryBytes);
  scoped_ptr<base::SharedMemory> shared_memory(
      ChildThreadImpl::AllocateSharedMemory(shared_memory_size, sender_.get()));
  CHECK(shared_memory.get());
  if (!shared_memory->Map(shared_memory_size))
    CHECK(false);

  size_t data_size = data.size();
  const char* data_ptr = data.data();
  while (data_size) {
    TRACE_EVENT0("Blob", "Registry::SendOversizedBlobItem");
    size_t chunk_size = std::min(data_size, shared_memory_size);
    memcpy(shared_memory->memory(), data_ptr, chunk_size);
    sender_->Send(new BlobHostMsg_SyncAppendSharedMemory(
        uuid_str, shared_memory->handle(), chunk_size));
    data_size -= chunk_size;
    data_ptr += chunk_size;
  }
}

// ------ streams stuff -----

void WebBlobRegistryImpl::registerStreamURL(
    const WebURL& url, const WebString& content_type) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_StartBuilding(url, content_type.utf8()));
}

void WebBlobRegistryImpl::registerStreamURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(ChildThreadImpl::current());
  sender_->Send(new StreamHostMsg_Clone(url, src_url));
}

void WebBlobRegistryImpl::addDataToStream(const WebURL& url,
                                          const char* data, size_t length) {
  DCHECK(ChildThreadImpl::current());
  if (length == 0)
    return;
  if (length < kLargeThresholdBytes) {
    storage::DataElement item;
    item.SetToBytes(data, length);
    sender_->Send(new StreamHostMsg_AppendBlobDataItem(url, item));
  } else {
    // We handle larger amounts of data via SharedMemory instead of
    // writing it directly to the IPC channel.
    size_t shared_memory_size = std::min(
        length, kMaxSharedMemoryBytes);
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
          url, shared_memory->handle(), chunk_size));
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

}  // namespace content
