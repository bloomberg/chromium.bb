// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webblobregistry_impl.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "content/child/child_thread.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/fileapi/webblob_messages.h"
#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThreadSafeData.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/common/blob/blob_data.h"

using WebKit::WebBlobData;
using WebKit::WebString;
using WebKit::WebThreadSafeData;
using WebKit::WebURL;

namespace content {

namespace {

const size_t kLargeThresholdBytes = 250 * 1024;
const size_t kMaxSharedMemoryBytes = 10 * 1024 * 1024;

}  // namespace

WebBlobRegistryImpl::WebBlobRegistryImpl(ThreadSafeSender* sender)
    : sender_(sender) {
}

WebBlobRegistryImpl::~WebBlobRegistryImpl() {
}

void WebBlobRegistryImpl::SendDataForBlob(const WebURL& url,
                                          const WebThreadSafeData& data) {

  if (data.size() == 0)
    return;
  if (data.size() < kLargeThresholdBytes) {
    webkit_blob::BlobData::Item item;
    item.SetToBytes(data.data(), data.size());
    sender_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
  } else {
    // We handle larger amounts of data via SharedMemory instead of
    // writing it directly to the IPC channel.
    size_t shared_memory_size = std::min(
        data.size(), kMaxSharedMemoryBytes);
    scoped_ptr<base::SharedMemory> shared_memory(
        ChildThread::AllocateSharedMemory(shared_memory_size,
                                          sender_.get()));
    CHECK(shared_memory.get());

    size_t data_size = data.size();
    const char* data_ptr = data.data();
    while (data_size) {
      size_t chunk_size = std::min(data_size, shared_memory_size);
      memcpy(shared_memory->memory(), data_ptr, chunk_size);
      sender_->Send(new BlobHostMsg_SyncAppendSharedMemory(
          url, shared_memory->handle(), chunk_size));
      data_size -= chunk_size;
      data_ptr += chunk_size;
    }
  }
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, WebBlobData& data) {
  DCHECK(ChildThread::current());
  sender_->Send(new BlobHostMsg_StartBuilding(url));
  size_t i = 0;
  WebBlobData::Item data_item;
  while (data.itemAt(i++, data_item)) {
    switch (data_item.type) {
      case WebBlobData::Item::TypeData: {
        // WebBlobData does not allow partial data items.
        DCHECK(!data_item.offset && data_item.length == -1);
        SendDataForBlob(url, data_item.data);
        break;
      }
      case WebBlobData::Item::TypeFile:
        if (data_item.length) {
          webkit_blob::BlobData::Item item;
          item.SetToFilePathRange(
              base::FilePath::FromUTF16Unsafe(data_item.filePath),
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length),
              base::Time::FromDoubleT(data_item.expectedModificationTime));
          sender_->Send(
              new BlobHostMsg_AppendBlobDataItem(url, item));
        }
        break;
      case WebBlobData::Item::TypeBlob:
        if (data_item.length) {
          webkit_blob::BlobData::Item item;
          item.SetToBlobUrlRange(
              data_item.blobURL,
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length));
          sender_->Send(
              new BlobHostMsg_AppendBlobDataItem(url, item));
        }
        break;
      case WebBlobData::Item::TypeURL:
        if (data_item.length) {
          // We only support filesystem URL as of now.
          DCHECK(GURL(data_item.url).SchemeIsFileSystem());
          webkit_blob::BlobData::Item item;
          item.SetToFileSystemUrlRange(
              data_item.url,
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length),
              base::Time::FromDoubleT(data_item.expectedModificationTime));
          sender_->Send(
              new BlobHostMsg_AppendBlobDataItem(url, item));
        }
        break;
      default:
        NOTREACHED();
    }
  }
  sender_->Send(new BlobHostMsg_FinishBuilding(
      url, data.contentType().utf8().data()));
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(ChildThread::current());
  sender_->Send(new BlobHostMsg_Clone(url, src_url));
}

void WebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  DCHECK(ChildThread::current());
  sender_->Send(new BlobHostMsg_Remove(url));
}

void WebBlobRegistryImpl::registerStreamURL(
    const WebURL& url, const WebString& content_type) {
  DCHECK(ChildThread::current());
  sender_->Send(new StreamHostMsg_StartBuilding(url, content_type.utf8()));
}

void WebBlobRegistryImpl::registerStreamURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(ChildThread::current());
  sender_->Send(new StreamHostMsg_Clone(url, src_url));
}

void WebBlobRegistryImpl::addDataToStream(const WebURL& url,
                                          WebThreadSafeData& data) {
  DCHECK(ChildThread::current());
  if (data.size() == 0)
    return;
  if (data.size() < kLargeThresholdBytes) {
    webkit_blob::BlobData::Item item;
    item.SetToBytes(data.data(), data.size());
    sender_->Send(new StreamHostMsg_AppendBlobDataItem(url, item));
  } else {
    // We handle larger amounts of data via SharedMemory instead of
    // writing it directly to the IPC channel.
    size_t shared_memory_size = std::min(
        data.size(), kMaxSharedMemoryBytes);
    scoped_ptr<base::SharedMemory> shared_memory(
        ChildThread::AllocateSharedMemory(shared_memory_size,
                                          sender_.get()));
    CHECK(shared_memory.get());

    size_t data_size = data.size();
    const char* data_ptr = data.data();
    while (data_size) {
      size_t chunk_size = std::min(data_size, shared_memory_size);
      memcpy(shared_memory->memory(), data_ptr, chunk_size);
      sender_->Send(new StreamHostMsg_SyncAppendSharedMemory(
          url, shared_memory->handle(), chunk_size));
      data_size -= chunk_size;
      data_ptr += chunk_size;
    }
  }
}

void WebBlobRegistryImpl::finalizeStream(const WebURL& url) {
  DCHECK(ChildThread::current());
  sender_->Send(new StreamHostMsg_FinishBuilding(url));
}

void WebBlobRegistryImpl::unregisterStreamURL(const WebURL& url) {
  DCHECK(ChildThread::current());
  sender_->Send(new StreamHostMsg_Remove(url));
}

}  // namespace content
