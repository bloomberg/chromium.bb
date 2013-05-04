// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_

#include <queue>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_entry_format.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class IOBuffer;
}

namespace disk_cache {

class SimpleBackendImpl;
class SimpleSynchronousEntry;

// SimpleEntryImpl is the IO thread interface to an entry in the very simple
// disk cache. It proxies for the SimpleSynchronousEntry, which performs IO
// on the worker thread.
class SimpleEntryImpl : public Entry, public base::RefCounted<SimpleEntryImpl>,
    public base::SupportsWeakPtr<SimpleEntryImpl> {
  friend class base::RefCounted<SimpleEntryImpl>;
 public:
  SimpleEntryImpl(SimpleBackendImpl* backend,
                  const base::FilePath& path,
                  const std::string& key,
                  uint64 entry_hash);

  // Adds another reader/writer to this entry, if possible, returning |this| to
  // |entry|.
  int OpenEntry(Entry** entry, const CompletionCallback& callback);

  // Creates this entry, if possible. Returns |this| to |entry|.
  int CreateEntry(Entry** entry, const CompletionCallback& callback);

  // Identical to Backend::Doom() except that it accepts a CompletionCallback.
  int DoomEntry(const CompletionCallback& callback);

  const std::string& key() const { return key_; }
  uint64 entry_hash() const { return entry_hash_; }

  // From Entry:
  virtual void Doom() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual std::string GetKey() const OVERRIDE;
  virtual base::Time GetLastUsed() const OVERRIDE;
  virtual base::Time GetLastModified() const OVERRIDE;
  virtual int32 GetDataSize(int index) const OVERRIDE;
  virtual int ReadData(int stream_index,
                       int offset,
                       net::IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) OVERRIDE;
  virtual int WriteData(int stream_index,
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
  class ScopedOperationRunner;
  friend class ScopedOperationRunner;

  enum State {
    // The state immediately after construction, but before |synchronous_entry_|
    // has been assigned. This is the state at construction, and is the only
    // legal state to destruct an entry in.
    STATE_UNINITIALIZED,

    // This entry is available for regular IO.
    STATE_READY,

    // IO is currently in flight, operations must wait for completion before
    // launching.
    STATE_IO_PENDING,

    // A failure occurred in the current or previous operation. All operations
    // after that must fail, until we receive a Close().
    STATE_FAILURE,
  };

  virtual ~SimpleEntryImpl();

  // Sets entry to STATE_UNINITIALIZED.
  void MakeUninitialized();

  // Return this entry to a user of the API in |out_entry|. Increments the user
  // count.
  void ReturnEntryToCaller(Entry** out_entry);

  // Ensures that |this| is no longer referenced by our |backend_|, this
  // guarantees that this entry cannot have OpenEntry/CreateEntry called again.
  void RemoveSelfFromBackend();

  // An error occured, and the SimpleSynchronousEntry should have Doomed
  // us at this point. We need to remove |this| from the Backend and the
  // index.
  void MarkAsDoomed();

  // Runs the next operation in the queue, if any and if there is no other
  // operation running at the moment.
  // WARNING: May delete |this|, as an operation in the queue can contain
  // the last reference.
  void RunNextOperationIfNeeded();

  void OpenEntryInternal(const CompletionCallback& callback, Entry** out_entry);

  void CreateEntryInternal(const CompletionCallback& callback,
                           Entry** out_entry);

  void CloseInternal();

  void ReadDataInternal(int index,
                        int offset,
                        net::IOBuffer* buf,
                        int buf_len,
                        const CompletionCallback& callback);

  void WriteDataInternal(int index,
                         int offset,
                         net::IOBuffer* buf,
                         int buf_len,
                         const CompletionCallback& callback,
                         bool truncate);

  // Called after a SimpleSynchronousEntry has completed CreateEntry() or
  // OpenEntry(). If |in_sync_entry| is non-NULL, creation is successful and we
  // can return |this| SimpleEntryImpl to |*out_entry|. Runs
  // |completion_callback|.
  void CreationOperationComplete(
      const CompletionCallback& completion_callback,
      const base::TimeTicks& start_time,
      scoped_ptr<SimpleSynchronousEntry*> in_sync_entry,
      scoped_ptr<int> in_result,
      Entry** out_entry);

  // Called after we've closed and written the EOF record to our entry. Until
  // this point it hasn't been safe to OpenEntry() the same entry, but from this
  // point it is.
  void CloseOperationComplete();

  // Called after a SimpleSynchronousEntry has completed an asynchronous IO
  // operation, such as ReadData() or WriteData(). Calls |completion_callback|.
  void EntryOperationComplete(
      int stream_index,
      const CompletionCallback& completion_callback,
      scoped_ptr<int> result);

  // Called after an asynchronous read. Updates |crc32s_| if possible.
  void ReadOperationComplete(
      int stream_index,
      int offset,
      const CompletionCallback& completion_callback,
      scoped_ptr<uint32> read_crc32,
      scoped_ptr<int> result);

  // Called after validating the checksums on an entry. Passes through the
  // original result if successful, propogates the error if the checksum does
  // not validate.
  void ChecksumOperationComplete(
      int stream_index,
      int orig_result,
      const CompletionCallback& completion_callback,
      scoped_ptr<int> result);

  // Called on initialization and also after the completion of asynchronous IO
  // to initialize the IO thread copies of data returned by synchronous accessor
  // functions. Copies data from |synchronous_entry_| into |this|, so that
  // values can be returned during our next IO operation.
  void SetSynchronousData();

  // All nonstatic SimpleEntryImpl methods should always be called on the IO
  // thread, in all cases. |io_thread_checker_| documents and enforces this.
  base::ThreadChecker io_thread_checker_;

  base::WeakPtr<SimpleBackendImpl> backend_;
  const base::FilePath path_;
  const std::string key_;
  const uint64 entry_hash_;

  // |last_used_|, |last_modified_| and |data_size_| are copied from the
  // synchronous entry at the completion of each item of asynchronous IO.
  base::Time last_used_;
  base::Time last_modified_;
  int32 data_size_[kSimpleEntryFileCount];

  // Number of times this object has been returned from Backend::OpenEntry() and
  // Backend::CreateEntry() without subsequent Entry::Close() calls. Used to
  // notify the backend when this entry not used by any callers.
  int open_count_;

  State state_;

  // When possible, we compute a crc32, for the data in each entry as we read or
  // write. For each stream, |crc32s_[index]| is the crc32 of that stream from
  // [0 .. |crc32s_end_offset_|). If |crc32s_end_offset_[index] == 0| then the
  // value of |crc32s_[index]| is undefined.
  int32 crc32s_end_offset_[kSimpleEntryFileCount];
  uint32 crc32s_[kSimpleEntryFileCount];

  // If |have_written_[index]| is true, we have written to the stream |index|.
  bool have_written_[kSimpleEntryFileCount];

  // The |synchronous_entry_| is the worker thread object that performs IO on
  // entries. It's owned by this SimpleEntryImpl whenever |operation_running_|
  // is false (i.e. when an operation is not pending on the worker pool).
  SimpleSynchronousEntry* synchronous_entry_;

  std::queue<base::Closure> pending_operations_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_IMPL_H_
