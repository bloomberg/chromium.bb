// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webblobregistry_impl.h"

#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "content/common/child_thread.h"
#include "content/common/webblob_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/blob/blob_data.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebBlobData;
using WebKit::WebString;
using WebKit::WebURL;

WebBlobRegistryImpl::WebBlobRegistryImpl(ChildThread* child_thread)
    : child_thread_(child_thread) {
}

WebBlobRegistryImpl::~WebBlobRegistryImpl() {
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, WebBlobData& data) {
  const size_t kLargeThresholdBytes = 250 * 1024;
  const size_t kMaxSharedMemoryBytes = 10 * 1024 * 1024;

  child_thread_->Send(new BlobHostMsg_StartBuildingBlob(url));
  size_t i = 0;
  WebBlobData::Item data_item;
  while (data.itemAt(i++, data_item)) {
    webkit_blob::BlobData::Item item;
    switch (data_item.type) {
      case WebBlobData::Item::TypeData: {
        // WebBlobData does not allow partial data items.
        DCHECK(!data_item.offset && data_item.length == -1);
        if (data_item.data.size() < kLargeThresholdBytes) {
          item.SetToData(data_item.data.data(), data_item.data.size());
          child_thread_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        } else {
          // We handle larger amounts of data via SharedMemory instead of
          // writing it directly to the IPC channel.
          size_t data_size = data_item.data.size();
          const char* data_ptr = data_item.data.data();
          size_t shared_memory_size = std::min(
              data_size, kMaxSharedMemoryBytes);
          scoped_ptr<base::SharedMemory> shared_memory(
              child_thread_->AllocateSharedMemory(shared_memory_size));
          CHECK(shared_memory.get());
          while (data_size) {
            size_t chunk_size = std::min(data_size, shared_memory_size);
            memcpy(shared_memory->memory(), data_ptr, chunk_size);
            child_thread_->Send(new BlobHostMsg_SyncAppendSharedMemory(
                url, shared_memory->handle(), chunk_size));
            data_size -= chunk_size;
            data_ptr += chunk_size;
          }
        }
        break;
      }
      case WebBlobData::Item::TypeFile:
        item.SetToFile(
            webkit_glue::WebStringToFilePath(data_item.filePath),
            static_cast<uint64>(data_item.offset),
            static_cast<uint64>(data_item.length),
            base::Time::FromDoubleT(data_item.expectedModificationTime));
        child_thread_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        break;
      case WebBlobData::Item::TypeBlob:
        if (data_item.length) {
          item.SetToBlob(
              data_item.blobURL,
              static_cast<uint64>(data_item.offset),
              static_cast<uint64>(data_item.length));
        }
        child_thread_->Send(new BlobHostMsg_AppendBlobDataItem(url, item));
        break;
      default:
        NOTREACHED();
    }
  }
  child_thread_->Send(new BlobHostMsg_FinishBuildingBlob(
      url, data.contentType().utf8().data()));
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  child_thread_->Send(new BlobHostMsg_CloneBlob(url, src_url));
}

void WebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  child_thread_->Send(new BlobHostMsg_RemoveBlob(url));
}
