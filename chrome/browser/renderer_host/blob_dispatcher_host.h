// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BLOB_DISPATCHER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_BLOB_DISPATCHER_HOST_H_

#include "base/hash_tables.h"
#include "base/ref_counted.h"

class ChromeBlobStorageContext;
class GURL;

namespace IPC {
class Message;
}

namespace webkit_blob {
class BlobData;
}

class BlobDispatcherHost {
 public:
  BlobDispatcherHost(int process_id,
                     ChromeBlobStorageContext* blob_storage_context);
  ~BlobDispatcherHost();

  void Shutdown();
  bool OnMessageReceived(const IPC::Message& message, bool* msg_is_ok);

 private:
  void OnRegisterBlobUrl(const GURL& url,
                         const scoped_refptr<webkit_blob::BlobData>& blob_data);
  void OnRegisterBlobUrlFrom(const GURL& url, const GURL& src_url);
  void OnUnregisterBlobUrl(const GURL& url);

  bool CheckPermission(webkit_blob::BlobData* blob_data) const;

  int process_id_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Keep track of blob URLs registered in this process. Need to unregister
  // all of them when the renderer process dies.
  base::hash_set<std::string> blob_urls_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlobDispatcherHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BLOB_DISPATCHER_HOST_H_
