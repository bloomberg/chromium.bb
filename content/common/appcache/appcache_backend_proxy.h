// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_APPCACHE_APPCACHE_BACKEND_PROXY_H_
#define CONTENT_COMMON_APPCACHE_APPCACHE_BACKEND_PROXY_H_
#pragma once

#include "ipc/ipc_message.h"
#include "webkit/appcache/appcache_interfaces.h"

// Sends appcache related messages to the main process.
class AppCacheBackendProxy : public appcache::AppCacheBackend {
 public:
  explicit AppCacheBackendProxy(IPC::Message::Sender* sender)
      : sender_(sender) {}

  IPC::Message::Sender* sender() const { return sender_; }

  // AppCacheBackend methods
  virtual void RegisterHost(int host_id);
  virtual void UnregisterHost(int host_id);
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url);
  virtual void SelectCacheForWorker(
                           int host_id,
                           int parent_process_id,
                           int parent_host_id);
  virtual void SelectCacheForSharedWorker(
                           int host_id,
                           int64 appcache_id);
  virtual void MarkAsForeignEntry(int host_id, const GURL& document_url,
                                  int64 cache_document_was_loaded_from);
  virtual appcache::Status GetStatus(int host_id);
  virtual bool StartUpdate(int host_id);
  virtual bool SwapCache(int host_id);
  virtual void GetResourceList(
      int host_id,
      std::vector<appcache::AppCacheResourceInfo>* resource_infos);

 private:
  IPC::Message::Sender* sender_;
};

#endif  // CONTENT_COMMON_APPCACHE_APPCACHE_BACKEND_PROXY_H_
