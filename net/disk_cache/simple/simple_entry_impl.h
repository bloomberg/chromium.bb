// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class IOBuffer;
}

namespace disk_cache {

class SimpleSynchronousEntry;

// SimpleEntryImpl is the IO thread interface to an entry in the very simple
// disk cache. It proxies for the SimpleSynchronousEntry, which performs IO
// on the worker thread.
class SimpleEntryImpl : public Entry,
                        public base::RefCountedThreadSafe<SimpleEntryImpl> {
  friend class base::RefCountedThreadSafe<SimpleEntryImpl>;
 public:
  static int OpenEntry(const scoped_refptr<SimpleIndex>& index,
                       const base::FilePath& path,
                       const std::string& key,
                       Entry** entry,
                       const CompletionCallback& callback);

  static int CreateEntry(const scoped_refptr<SimpleIndex>& index,
                         const base::FilePath& path,
                         const std::string& key,
                         Entry** entry,
                         const CompletionCallback& callback);

  static int DoomEntry(const scoped_refptr<SimpleIndex>& index,
                       const base::FilePath& path,
                       const std::string& key,
                       const CompletionCallback& callback);

  // From Entry:
  virtual void Doom() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual std::string GetKey() const OVERRIDE;
  virtual base::Time GetLastUsed() const OVERRIDE;
  virtual base::Time GetLastModified() const OVERRIDE;
  virtual int32 GetDataSize(int index) const OVERRIDE;
  virtual int ReadData(int index,
                       int offset,
                       net::IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) OVERRIDE;
  virtual int WriteData(int index,
                        int offset,
                        net::IOBuffer* buf,
                        int buf_len,
                        const CompletionCallback& callback,
                        bool truncate) OVERRIDE;
  virtual int ReadSparseData(int64 offset,
                             net::IOBuffer* buf,
                             int buf_len,
                             const CompletionCallback& callback) OVERRIDE;
  virtual int WriteSparseData(int64 offset,
                              net::IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) OVERRIDE;
  virtual int GetAvailableRange(int64 offset,
                                int len,
                                int64* start,
                                const CompletionCallback& callback) OVERRIDE;
  virtual bool CouldBeSparse() const OVERRIDE;
  virtual void CancelSparseIO() OVERRIDE;
  virtual int ReadyForSparseIO(const CompletionCallback& callback) OVERRIDE;

 private:
  SimpleEntryImpl(const scoped_refptr<SimpleIndex>& index,
                  const base::FilePath& path,
                  const std::string& key);

  virtual ~SimpleEntryImpl();

  // Called after a SimpleSynchronousEntry has completed CreateEntry() or
  // OpenEntry(). If |sync_entry| is non-NULL, creation is successful and we
  // can return |this| SimpleEntryImpl to |*out_entry|. Runs
  // |completion_callback|.
  void CreationOperationComplete(
      Entry** out_entry,
      const CompletionCallback& completion_callback,
      SimpleSynchronousEntry* sync_entry);

  // Called after a SimpleSynchronousEntry has completed an asynchronous IO
  // operation, such as ReadData() or WriteData(). Calls |completion_callback|.
  void EntryOperationComplete(
      const CompletionCallback& completion_callback,
      int result);

  // Called on initialization and also after the completion of asynchronous IO
  // to initialize the IO thread copies of data returned by synchronous accessor
  // functions. Copies data from |synchronous_entry_| into |this|, so that
  // values can be returned during our next IO operation.
  void SetSynchronousData();

  // All nonstatic SimpleEntryImpl methods should always be called on the IO
  // thread, in all cases. |io_thread_checker_| documents and enforces this.
  base::ThreadChecker io_thread_checker_;

  const scoped_refptr<base::MessageLoopProxy> constructor_thread_;
  const scoped_refptr<SimpleIndex> index_;
  const base::FilePath path_;
  const std::string key_;

  // |last_used_|, |last_modified_| and |data_size_| are copied from the
  // synchronous entry at the completion of each item of asynchronous IO.
  base::Time last_used_;
  base::Time last_modified_;
  int32 data_size_[kSimpleEntryFileCount];

  // The |synchronous_entry_| is the worker thread object that performs IO on
  // entries. It's owned by this SimpleEntryImpl whenever
  // |synchronous_entry_in_use_by_worker_| is false (i.e. when an operation
  // is not pending on the worker pool). When an operation is pending on the
  // worker pool, the |synchronous_entry_| is owned by itself.
  SimpleSynchronousEntry* synchronous_entry_;

  // Set to true when a worker operation is posted on the |synchronous_entry_|,
  // and false after. Used to ensure thread safety by not allowing multiple
  // threads to access the |synchronous_entry_| simultaneously.
  bool synchronous_entry_in_use_by_worker_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_
