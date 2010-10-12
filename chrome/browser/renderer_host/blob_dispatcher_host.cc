// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/blob_dispatcher_host.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_blob_storage_context.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

BlobDispatcherHost::BlobDispatcherHost(
    int process_id,
    ChromeBlobStorageContext* blob_storage_context)
    : process_id_(process_id),
      blob_storage_context_(blob_storage_context) {
}

BlobDispatcherHost::~BlobDispatcherHost() {
}

void BlobDispatcherHost::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Unregister all the blob URLs that are previously registered in this
  // process.
  for (base::hash_set<std::string>::const_iterator iter = blob_urls_.begin();
       iter != blob_urls_.end(); ++iter) {
    blob_storage_context_->controller()->UnregisterBlobUrl(GURL(*iter));
  }
}

bool BlobDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                           bool* msg_is_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  *msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(BlobDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterBlobUrl, OnRegisterBlobUrl)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterBlobUrlFrom, OnRegisterBlobUrlFrom)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnregisterBlobUrl, OnUnregisterBlobUrl)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Check if the child process has been granted permission to register the files.
bool BlobDispatcherHost::CheckPermission(
    webkit_blob::BlobData* blob_data) const {
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  for (std::vector<webkit_blob::BlobData::Item>::const_iterator iter =
           blob_data->items().begin();
       iter != blob_data->items().end(); ++iter) {
    if (iter->type() == webkit_blob::BlobData::TYPE_FILE) {
      if (!policy->CanReadFile(process_id_, iter->file_path()))
        return false;
    }
  }
  return true;
}

void BlobDispatcherHost::OnRegisterBlobUrl(
    const GURL& url, const scoped_refptr<webkit_blob::BlobData>& blob_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!CheckPermission(blob_data.get()))
    return;
  blob_storage_context_->controller()->RegisterBlobUrl(url, blob_data);
  blob_urls_.insert(url.spec());
}

void BlobDispatcherHost::OnRegisterBlobUrlFrom(
    const GURL& url, const GURL& src_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->RegisterBlobUrlFrom(url, src_url);
  blob_urls_.insert(url.spec());
}

void BlobDispatcherHost::OnUnregisterBlobUrl(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->UnregisterBlobUrl(url);
  blob_urls_.erase(url.spec());
}
