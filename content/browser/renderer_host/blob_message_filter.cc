// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/blob_message_filter.h"

#include "content/browser/child_process_security_policy.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/common/webblob_messages.h"
#include "googleurl/src/gurl.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using content::BrowserMessageFilter;
using content::BrowserThread;
using webkit_blob::BlobData;

BlobMessageFilter::BlobMessageFilter(
    int process_id,
    ChromeBlobStorageContext* blob_storage_context)
    : process_id_(process_id),
      blob_storage_context_(blob_storage_context) {
}

BlobMessageFilter::~BlobMessageFilter() {
}

void BlobMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Unregister all the blob URLs that are previously registered in this
  // process.
  for (base::hash_set<std::string>::const_iterator iter = blob_urls_.begin();
       iter != blob_urls_.end(); ++iter) {
    blob_storage_context_->controller()->RemoveBlob(GURL(*iter));
  }
}

bool BlobMessageFilter::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(BlobMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(BlobHostMsg_StartBuildingBlob, OnStartBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_AppendBlobDataItem, OnAppendBlobDataItem)
    IPC_MESSAGE_HANDLER(BlobHostMsg_SyncAppendSharedMemory,
                        OnAppendSharedMemory)
    IPC_MESSAGE_HANDLER(BlobHostMsg_FinishBuildingBlob, OnFinishBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_CloneBlob, OnCloneBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_RemoveBlob, OnRemoveBlob)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BlobMessageFilter::OnStartBuildingBlob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->StartBuildingBlob(url);
  blob_urls_.insert(url.spec());
}

void BlobMessageFilter::OnAppendBlobDataItem(
    const GURL& url, const BlobData::Item& item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (item.type == BlobData::TYPE_FILE &&
      !ChildProcessSecurityPolicy::GetInstance()->CanReadFile(
          process_id_, item.file_path)) {
    OnRemoveBlob(url);
    return;
  }
  blob_storage_context_->controller()->AppendBlobDataItem(url, item);
}

void BlobMessageFilter::OnAppendSharedMemory(
    const GURL& url, base::SharedMemoryHandle handle, size_t buffer_size) {
  DCHECK(base::SharedMemory::IsHandleValid(handle));
#if defined(OS_WIN)
  base::SharedMemory shared_memory(handle, true, peer_handle());
#else
  base::SharedMemory shared_memory(handle, true);
#endif
  if (!shared_memory.Map(buffer_size)) {
    OnRemoveBlob(url);
    return;
  }

  BlobData::Item item;
  item.SetToDataExternal(static_cast<char*>(shared_memory.memory()),
                        buffer_size);
  blob_storage_context_->controller()->AppendBlobDataItem(url, item);
}

void BlobMessageFilter::OnFinishBuildingBlob(
    const GURL& url, const std::string& content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->FinishBuildingBlob(url, content_type);
}

void BlobMessageFilter::OnCloneBlob(
    const GURL& url, const GURL& src_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->CloneBlob(url, src_url);
  blob_urls_.insert(url.spec());
}

void BlobMessageFilter::OnRemoveBlob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->RemoveBlob(url);
  blob_urls_.erase(url.spec());
}
