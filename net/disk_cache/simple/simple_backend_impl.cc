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
#include "net/disk_cache/simple/simple_index.h"

using base::FilePath;
using base::MessageLoopProxy;
using base::SingleThreadTaskRunner;
using base::Time;
using base::WorkerPool;
using file_util::DirectoryExists;
using file_util::CreateDirectory;

namespace {

// Must run on IO Thread.
void DeleteBackendImpl(disk_cache::Backend** backend,
                       const net::CompletionCallback& callback,
                       int result) {
  DCHECK(*backend);
  delete *backend;
  *backend = NULL;
  callback.Run(result);
}

}  // namespace

namespace disk_cache {

SimpleBackendImpl::SimpleBackendImpl(
    const FilePath& path,
    int max_bytes,
    net::CacheType type,
    base::SingleThreadTaskRunner* cache_thread,
    net::NetLog* net_log)
  : path_(path),
    index_(new SimpleIndex(cache_thread,
                           MessageLoopProxy::current(),  // io_thread
                           path)),
    cache_thread_(cache_thread) {}

SimpleBackendImpl::~SimpleBackendImpl() {
  index_->WriteToDisk();
}

int SimpleBackendImpl::Init(const CompletionCallback& completion_callback) {
  InitializeIndexCallback initialize_index_callback =
      base::Bind(&SimpleBackendImpl::InitializeIndex,
                 base::Unretained(this),
                 completion_callback);
  cache_thread_->PostTask(FROM_HERE,
                          base::Bind(&SimpleBackendImpl::CreateDirectory,
                                     MessageLoopProxy::current(),  // io_thread
                                     path_,
                                     initialize_index_callback));
  return net::ERR_IO_PENDING;
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
  return SimpleEntryImpl::OpenEntry(index_.get(), path_, key, entry, callback);
}

int SimpleBackendImpl::CreateEntry(const std::string& key,
                                   Entry** entry,
                                   const CompletionCallback& callback) {
  return SimpleEntryImpl::CreateEntry(index_.get(), path_, key, entry,
                                      callback);
}

int SimpleBackendImpl::DoomEntry(const std::string& key,
                                 const net::CompletionCallback& callback) {
  return SimpleEntryImpl::DoomEntry(index_.get(), path_, key, callback);
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

void SimpleBackendImpl::InitializeIndex(
    const CompletionCallback& callback, int result) {
  if (result == net::OK)
    index_->Initialize();
  callback.Run(result);
}

// static
void SimpleBackendImpl::CreateDirectory(
    SingleThreadTaskRunner* io_thread,
    const base::FilePath& path,
    const InitializeIndexCallback& initialize_index_callback) {
  int rv = net::OK;
  if (!file_util::PathExists(path) && !file_util::CreateDirectory(path)) {
    LOG(ERROR) << "Simple Cache Backend: failed to create: " << path.value();
    rv = net::ERR_FAILED;
  }

  io_thread->PostTask(FROM_HERE, base::Bind(initialize_index_callback, rv));
}

}  // namespace disk_cache
