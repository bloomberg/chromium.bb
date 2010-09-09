// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_DISK_CACHE_BASED_SSL_HOST_INFO_H
#define NET_HTTP_DISK_CACHE_BASED_SSL_HOST_INFO_H

#include <string>

#include "base/lock.h"
#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "net/base/ssl_non_sensitive_host_info.h"
#include "net/disk_cache/disk_cache.h"

namespace net {

class IOBuffer;
class HttpCache;

// DiskCacheBasedSSLHostInfo fetches information about an SSL host from our
// standard disk cache. Since the information is defined to be non-sensitive,
// it's ok for us to keep it on disk.
class DiskCacheBasedSSLHostInfo : public SSLNonSensitiveHostInfo,
                                  public NonThreadSafe {
 public:
  DiskCacheBasedSSLHostInfo(const std::string& hostname, HttpCache* http_cache);
  ~DiskCacheBasedSSLHostInfo();

  // Implementation of SSLNonSensitiveHostInfo
  virtual int WaitForDataReady(CompletionCallback* callback);
  virtual const std::string& data() const { return data_; }
  virtual void Set(const std::string& new_data);

 private:
  std::string key() const;

  void DoLoop(int rv);

  void DoGetBackendComplete(int rv);
  void DoOpenComplete(int rv);
  void DoReadComplete(int rv);
  void DoWriteComplete(int rv);
  void DoCreateComplete(int rv);

  void DoGetBackend();
  void DoOpen();
  void DoRead();
  void DoCreate();
  void DoWrite();

  // WaitForDataReadyDone is the terminal state of the read operation.
  void WaitForDataReadyDone();
  // SetDone is the terminal state of the write operation.
  void SetDone();

  enum State {
    GET_BACKEND,
    GET_BACKEND_COMPLETE,
    OPEN,
    OPEN_COMPLETE,
    READ,
    READ_COMPLETE,
    CREATE,
    CREATE_COMPLETE,
    WRITE,
    WRITE_COMPLETE,
    NONE,
  };

  scoped_ptr<CompletionCallback> callback_;
  State state_;
  bool ready_;
  std::string new_data_;
  const std::string hostname_;
  HttpCache* const http_cache_;
  disk_cache::Backend* backend_;
  disk_cache::Entry *entry_;
  CompletionCallback* user_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  std::string data_;
};

}  // namespace net

#endif  // NET_HTTP_DISK_CACHE_BASED_SSL_HOST_INFO_H
