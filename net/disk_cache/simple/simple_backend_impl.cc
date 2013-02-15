// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_backend_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/threading/worker_pool.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_entry_impl.h"

using base::FilePath;
using base::MessageLoopProxy;
using base::Time;
using base::WorkerPool;
using file_util::DirectoryExists;
using file_util::CreateDirectory;

namespace {

const char* kSimpleBackendSubdirectory = "Simple";

}  // namespace

namespace disk_cache {

// static
int SimpleBackendImpl::CreateBackend(
    const FilePath& full_path,
    bool force,
    int max_bytes,
    net::CacheType type,
    uint32 flags,
    scoped_refptr<base::TaskRunner> task_runner,
    net::NetLog* net_log,
    Backend** backend,
    const CompletionCallback& callback) {
  // TODO(gavinp): Use the |net_log|.
  DCHECK_EQ(net::DISK_CACHE, type);
  FilePath simple_cache_path =
      full_path.AppendASCII(kSimpleBackendSubdirectory);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleBackendImpl::EnsureCachePathExists,
                                  simple_cache_path,
                                  MessageLoopProxy::current(), callback,
                                  backend),
                       true);
  return net::ERR_IO_PENDING;
}

SimpleBackendImpl::~SimpleBackendImpl() {
}

net::CacheType SimpleBackendImpl::GetCacheType() const {
  return net::DISK_CACHE;
}

int32 SimpleBackendImpl::GetEntryCount() const {
  NOTIMPLEMENTED();
  return 0;
}

int SimpleBackendImpl::OpenEntry(const std::string& key,
                                 Entry** entry,
                                 const CompletionCallback& callback) {
  return SimpleEntryImpl::OpenEntry(path_, key, entry, callback);
}

int SimpleBackendImpl::CreateEntry(const std::string& key,
                                   Entry** entry,
                                   const CompletionCallback& callback) {
  return SimpleEntryImpl::CreateEntry(path_, key, entry, callback);
}

int SimpleBackendImpl::DoomEntry(const std::string& key,
                                 const net::CompletionCallback& callback) {
  return SimpleEntryImpl::DoomEntry(path_, key, callback);
}

int SimpleBackendImpl::DoomAllEntries(const CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

int SimpleBackendImpl::DoomEntriesBetween(
    const Time initial_time,
    const Time end_time,
    const CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

int SimpleBackendImpl::DoomEntriesSince(
    const Time initial_time,
    const CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

int SimpleBackendImpl::OpenNextEntry(void** iter,
                                     Entry** next_entry,
                                     const CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

void SimpleBackendImpl::EndEnumeration(void** iter) {
  NOTIMPLEMENTED();
}

void SimpleBackendImpl::GetStats(
    std::vector<std::pair<std::string, std::string> >* stats) {
  NOTIMPLEMENTED();
}

void SimpleBackendImpl::OnExternalCacheHit(const std::string& key) {
  NOTIMPLEMENTED();
}

SimpleBackendImpl::SimpleBackendImpl(
    const FilePath& path) : path_(path) {
}

// static
void SimpleBackendImpl::EnsureCachePathExists(
    const FilePath& path,
    const scoped_refptr<base::TaskRunner>& callback_runner,
    const CompletionCallback& callback,
    Backend** backend) {
  int result = net::OK;
  if (!DirectoryExists(path) && !CreateDirectory(path))
    result = net::ERR_FAILED;
  callback_runner->PostTask(FROM_HERE,
                            base::Bind(&SimpleBackendImpl::OnCachePathCreated,
                                       result, path, callback, backend));
}

// static
void SimpleBackendImpl::OnCachePathCreated(int result,
                                           const FilePath& path,
                                           const CompletionCallback& callback,
                                           Backend** backend) {
  if (result == net::OK)
    *backend = new SimpleBackendImpl(path);
  else
    *backend = NULL;
  callback.Run(result);
}

}  // namespace disk_cache
