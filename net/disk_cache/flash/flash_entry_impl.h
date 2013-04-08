// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_FLASH_ENTRY_IMPL_H_
#define NET_DISK_CACHE_FLASH_ENTRY_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/flash/internal_entry.h"

namespace base {

class MessageLoopProxy;

}  // namespace base

namespace disk_cache {

class InternalEntry;
class IOBuffer;
class LogStore;

// We use split objects to minimize the context switches between the main thread
// and the cache thread in the most common case of creating a new entry.
//
// All calls on a new entry are served synchronously.  When an object is
// destructed (via final Close() call), a message is posted to the cache thread
// to save the object to storage.
//
// When an entry is not new, every asynchronous call is posted to the cache
// thread, just as before; synchronous calls like GetKey() and GetDataSize() are
// served from the main thread.
class NET_EXPORT_PRIVATE FlashEntryImpl
    : public Entry,
      public base::RefCountedThreadSafe<FlashEntryImpl> {
  friend class base::RefCountedThreadSafe<FlashEntryImpl>;
 public:
  FlashEntryImpl(const std::string& key,
                 LogStore* store,
                 base::MessageLoopProxy* cache_thread);
  FlashEntryImpl(int32 id,
                 LogStore* store,
                 base::MessageLoopProxy* cache_thread);

  int Init(const CompletionCallback& callback);

  // disk_cache::Entry interface.
  virtual void Doom() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual std::string GetKey() const OVERRIDE;
  virtual base::Time GetLastUsed() const OVERRIDE;
  virtual base::Time GetLastModified() const OVERRIDE;
  virtual int32 GetDataSize(int index) const OVERRIDE;
  virtual int ReadData(int index, int offset, IOBuffer* buf, int buf_len,
                       const CompletionCallback& callback) OVERRIDE;
  virtual int WriteData(int index, int offset, IOBuffer* buf, int buf_len,
                        const CompletionCallback& callback,
                        bool truncate) OVERRIDE;
  virtual int ReadSparseData(int64 offset, IOBuffer* buf, int buf_len,
                             const CompletionCallback& callback) OVERRIDE;
  virtual int WriteSparseData(int64 offset, IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) OVERRIDE;
  virtual int GetAvailableRange(int64 offset, int len, int64* start,
                                const CompletionCallback& callback) OVERRIDE;
  virtual bool CouldBeSparse() const OVERRIDE;
  virtual void CancelSparseIO() OVERRIDE;
  virtual int ReadyForSparseIO(const CompletionCallback& callback) OVERRIDE;

 private:
  void OnInitComplete(scoped_ptr<KeyAndStreamSizes> key_and_stream_sizes);
  virtual ~FlashEntryImpl();

  bool init_;
  std::string key_;
  int stream_sizes_[kFlashLogStoreEntryNumStreams];

  // Used if |this| is an newly created entry.
  scoped_refptr<InternalEntry> new_internal_entry_;

  // Used if |this| is an existing entry.
  scoped_refptr<InternalEntry> old_internal_entry_;

  // Copy of the callback for asynchronous calls on |old_internal_entry_|.
  CompletionCallback callback_;

  scoped_refptr<base::MessageLoopProxy> cache_thread_;

  DISALLOW_COPY_AND_ASSIGN(FlashEntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FLASH_ENTRY_IMPL_H_
