// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_OPERATION_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_OPERATION_H_

#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

class Entry;
class SimpleEntryImpl;

// SimpleEntryOperation stores the information regarding operations in
// SimpleEntryImpl, between the moment they are issued by users of the backend,
// and the moment when they are executed.
class SimpleEntryOperation {
 public:
  typedef net::CompletionCallback CompletionCallback;

  enum EntryOperationType {
    TYPE_OPEN = 0,
    TYPE_CREATE = 1,
    TYPE_CLOSE = 2,
    TYPE_READ = 3,
    TYPE_WRITE = 4,
  };

  SimpleEntryOperation(const SimpleEntryOperation& other);
  ~SimpleEntryOperation();

  static SimpleEntryOperation OpenOperation(SimpleEntryImpl* entry,
                                            bool have_index,
                                            const CompletionCallback& callback,
                                            Entry** out_entry);
  static SimpleEntryOperation CreateOperation(
      SimpleEntryImpl* entry,
      bool have_index,
      const CompletionCallback& callback,
      Entry** out_entry);
  static SimpleEntryOperation CloseOperation(SimpleEntryImpl* entry);
  static SimpleEntryOperation ReadOperation(SimpleEntryImpl* entry,
                                            int index,
                                            int offset,
                                            int length,
                                            net::IOBuffer* buf,
                                            const CompletionCallback& callback,
                                            bool alone_in_queue);
  static SimpleEntryOperation WriteOperation(
      SimpleEntryImpl* entry,
      int index,
      int offset,
      int length,
      net::IOBuffer* buf,
      bool truncate,
      bool optimistic,
      const CompletionCallback& callback);

  bool ConflictsWith(const SimpleEntryOperation& other_op) const;
  // Releases all references. After calling this operation, SimpleEntryOperation
  // will only hold POD members.
  void ReleaseReferences();

  EntryOperationType type() const {
    return static_cast<EntryOperationType>(type_);
  }
  const CompletionCallback& callback() const { return callback_; }
  Entry** out_entry() { return out_entry_; }
  bool have_index() const { return have_index_; }
  int index() const { return index_; }
  int offset() const { return offset_; }
  int length() const { return length_; }
  net::IOBuffer* buf() { return buf_.get(); }
  bool truncate() const { return truncate_; }
  bool optimistic() const { return optimistic_; }
  bool alone_in_queue() const { return alone_in_queue_; }

 private:
  SimpleEntryOperation(SimpleEntryImpl* entry,
                       net::IOBuffer* buf,
                       const CompletionCallback& callback,
                       Entry** out_entry,
                       int offset,
                       int length,
                       EntryOperationType type,
                       bool have_index,
                       int index,
                       bool truncate,
                       bool optimistic,
                       bool alone_in_queue);

  // This ensures entry will not be deleted until the operation has ran.
  scoped_refptr<SimpleEntryImpl> entry_;
  scoped_refptr<net::IOBuffer> buf_;
  CompletionCallback callback_;

  // Used in open and create operations.
  Entry** out_entry_;

  // Used in write and read operations.
  const int offset_;
  const int length_;

  const unsigned int type_ : 3;           /* 3 */
  // Used in open and create operations.
  const unsigned int have_index_ : 1;     /* 4 */
  // Used in write and read operations.
  const unsigned int index_ : 2;          /* 6 */
  // Used only in write operations.
  const unsigned int truncate_ : 1;       /* 7 */
  const unsigned int optimistic_ : 1;     /* 8 */
  // Used only in SimpleCache.ReadIsParallelizable histogram.
  const unsigned int alone_in_queue_ : 1; /* 9 */
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_OPERATION_H_
