// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"

namespace content {

class CacheStorageContextImpl;
class CacheStorageListener;

// Handles Cache Storage related messages sent to the browser process from
// child processes. One host instance exists per child process. All
// messages are processed on the IO thread. Currently, all messages are
// passed directly to the owned CacheStorageListener instance, which
// in turn dispatches calls to the CacheStorageManager owned
// by the CacheStorageContextImpl for the entire browsing context.
// TODO(jsbell): Merge this with CacheStorageListener crbug.com/439389
class CONTENT_EXPORT CacheStorageDispatcherHost : public BrowserMessageFilter {
 public:
  CacheStorageDispatcherHost();

  // Runs on UI thread.
  void Init(CacheStorageContextImpl* context);

  // BrowserMessageFilter implementation
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // Friends to allow OnDestruct() delegation
  friend class BrowserThread;
  friend class base::DeleteHelper<CacheStorageDispatcherHost>;

  ~CacheStorageDispatcherHost() override;

  // Called by Init() on IO thread.
  void CreateCacheListener(CacheStorageContextImpl* context);

  scoped_ptr<CacheStorageListener> cache_listener_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
