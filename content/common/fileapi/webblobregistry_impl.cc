// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fileapi/webblobregistry_impl.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "content/common/child_thread.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/common/thread_safe_sender.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebBlobData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/blob/blob_data.h"

using WebKit::WebBlobData;
using WebKit::WebString;
using WebKit::WebURL;

namespace content {

WebBlobRegistryImpl::WebBlobRegistryImpl(ThreadSafeSender* sender)
    : sender_(sender) {
}

WebBlobRegistryImpl::~WebBlobRegistryImpl() {
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, WebBlobData& data) {
  DCHECK(ChildThread::current()->message_loop() ==
         base::MessageLoop::current());
  const size_t kLargeThresholdBytes = 250 * 1024;
  const size_t kMaxSharedMemoryBytes = 10 * 1024 * 1024;

  sender_->Send(new BlobHostMsg_StartBuildingBlob(url));
  size_t i = 0;
  WebBlobData::Item data_item;
  while (data.itemAt(i++, data_item)) {
    webkit_blob::BlobData::Item item;
    switch (data_item.type) {
      case WebBlobData::Item::TypeData: {
        // WebBlobData does not allow partial data items.
        DCHECK(!data_item.offset && data_item.length == -1);
        if (data_item.data.size() == 0)
          break;
        if (data_item.data.size() < kLargeThresholdBytes) {
          item.SetToBytes(data_item.data.data(), data_item.data.size());
          sender_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        } else {
          // We handle larger amounts of data via SharedMemory instead of
          // writing it directly to the IPC channel.
          size_t data_size = data_item.data.size();
          const char* data_ptr = data_item.data.data();
          size_t shared_memory_size = std::min(
              data_size, kMaxSharedMemoryBytes);
          scoped_ptr<base::SharedMemory> shared_memory(
              ChildThread::AllocateSharedMemory(shared_memory_size, sender_));
          CHECK(shared_memory.get());
          while (data_size) {
            size_t chunk_size = std::min(data_size, shared_memory_size);
            memcpy(shared_memory->memory(), data_ptr, chunk_size);
            sender_->Send(new BlobHostMsg_SyncAppendSharedMemory(
                url, shared_memory->handle(), chunk_size));
            data_size -= chunk_size;
            data_ptr += chunk_size;
          }
        }
        break;
      }
      case WebBlobData::Item::TypeFile:
        if (data_item.length) {
          item.SetToFilePathRange(
              webkit_base::WebStringToFilePath(data_item.filePath),
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length),
              base::Time::FromDoubleT(data_item.expectedModificationTime));
          sender_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        }
        break;
      case WebBlobData::Item::TypeBlob:
        if (data_item.length) {
          item.SetToBlobUrlRange(
              data_item.blobURL,
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length));
          sender_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        }
        break;
      case WebBlobData::Item::TypeURL:
        if (data_item.length) {
          // We only support filesystem URL as of now.
          DCHECK(GURL(data_item.url).SchemeIsFileSystem());
          item.SetToFileSystemUrlRange(
              data_item.url,
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length),
              base::Time::FromDoubleT(data_item.expectedModificationTime));
          sender_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        }
        break;
      default:
        NOTREACHED();
    }
  }
  sender_->Send(new BlobHostMsg_FinishBuildingBlob(
      url, data.contentType().utf8().data()));
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(ChildThread::current()->message_loop() ==
         base::MessageLoop::current());
  sender_->Send(new BlobHostMsg_CloneBlob(url, src_url));
}

void WebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  DCHECK(ChildThread::current()->message_loop() ==
         base::MessageLoop::current());
  sender_->Send(new BlobHostMsg_RemoveBlob(url));
}

}  // namespace content
