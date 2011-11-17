// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BLOB_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_BLOB_MESSAGE_FILTER_H_

#include "base/hash_tables.h"
#include "base/shared_memory.h"
#include "content/browser/browser_message_filter.h"
#include "webkit/blob/blob_data.h"

class ChromeBlobStorageContext;
class GURL;

namespace IPC {
class Message;
}

class BlobMessageFilter : public BrowserMessageFilter {
 public:
  BlobMessageFilter(int process_id,
                    ChromeBlobStorageContext* blob_storage_context);
  virtual ~BlobMessageFilter();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  void OnStartBuildingBlob(const GURL& url);
  void OnAppendBlobDataItem(const GURL& url,
                            const webkit_blob::BlobData::Item& item);
  void OnAppendSharedMemory(const GURL& url, base::SharedMemoryHandle handle,
                            size_t buffer_size);
  void OnFinishBuildingBlob(const GURL& url, const std::string& content_type);
  void OnCloneBlob(const GURL& url, const GURL& src_url);
  void OnRemoveBlob(const GURL& url);

  int process_id_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Keep track of blob URLs registered in this process. Need to unregister
  // all of them when the renderer process dies.
  base::hash_set<std::string> blob_urls_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlobMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BLOB_MESSAGE_FILTER_H_
