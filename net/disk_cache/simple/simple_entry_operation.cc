// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_entry_operation.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_entry_impl.h"

namespace disk_cache {

SimpleEntryOperation::SimpleEntryOperation(const SimpleEntryOperation& other)
    : entry_(other.entry_.get()),
      buf_(other.buf_),
      callback_(other.callback_),
      out_entry_(other.out_entry_),
      offset_(other.offset_),
      length_(other.length_),
      type_(other.type_),
      have_index_(other.have_index_),
      index_(other.index_),
      truncate_(other.truncate_),
      optimistic_(other.optimistic_),
      alone_in_queue_(other.alone_in_queue_) {
}

SimpleEntryOperation::~SimpleEntryOperation() {}

// Static.
SimpleEntryOperation SimpleEntryOperation::OpenOperation(
    SimpleEntryImpl* entry,
    bool have_index,
    const CompletionCallback& callback,
    Entry** out_entry) {
  return SimpleEntryOperation(entry,
                              NULL,
                              callback,
                              out_entry,
                              0,
                              0,
                              TYPE_OPEN,
                              have_index,
                              0,
                              false,
                              false,
                              false);
}

// Static.
SimpleEntryOperation SimpleEntryOperation::CreateOperation(
    SimpleEntryImpl* entry,
    bool have_index,
    const CompletionCallback& callback,
    Entry** out_entry) {
  return SimpleEntryOperation(entry,
                              NULL,
                              callback,
                              out_entry,
                              0,
                              0,
                              TYPE_CREATE,
                              have_index,
                              0,
                              false,
                              false,
                              false);
}

// Static.
SimpleEntryOperation SimpleEntryOperation::CloseOperation(
    SimpleEntryImpl* entry) {
  return SimpleEntryOperation(entry,
                              NULL,
                              CompletionCallback(),
                              NULL,
                              0,
                              0,
                              TYPE_CLOSE,
                              false,
                              0,
                              false,
                              false,
                              false);
}

// Static.
SimpleEntryOperation SimpleEntryOperation::ReadOperation(
    SimpleEntryImpl* entry,
    int index,
    int offset,
    int length,
    net::IOBuffer* buf,
    const CompletionCallback& callback,
    bool alone_in_queue) {
  return SimpleEntryOperation(entry,
                              buf,
                              callback,
                              NULL,
                              offset,
                              length,
                              TYPE_READ,
                              false,
                              index,
                              false,
                              false,
                              alone_in_queue);
}

// Static.
SimpleEntryOperation SimpleEntryOperation::WriteOperation(
    SimpleEntryImpl* entry,
    int index,
    int offset,
    int length,
    net::IOBuffer* buf,
    bool truncate,
    bool optimistic,
    const CompletionCallback& callback) {
  return SimpleEntryOperation(entry,
                              buf,
                              callback,
                              NULL,
                              offset,
                              length,
                              TYPE_WRITE,
                              false,
                              index,
                              truncate,
                              optimistic,
                              false);
}

bool SimpleEntryOperation::ConflictsWith(
    const SimpleEntryOperation& other_op) const {
  if (type_ != TYPE_READ && type_ != TYPE_WRITE)
    return true;
  if (other_op.type() != TYPE_READ && other_op.type() != TYPE_WRITE)
    return true;
  if (type() == TYPE_READ && other_op.type() == TYPE_READ)
    return false;
  if (index_ != other_op.index_)
    return false;
  int end = (type_ == TYPE_WRITE && truncate_) ? INT_MAX : offset_ + length_;
  int other_op_end = (other_op.type() == TYPE_WRITE && other_op.truncate())
                         ? INT_MAX
                         : other_op.offset() + other_op.length();
  return (offset_ < other_op_end && other_op.offset() < end);
}

void SimpleEntryOperation::ReleaseReferences() {
  callback_ = CompletionCallback();
  buf_ = NULL;
  entry_ = NULL;
}

SimpleEntryOperation::SimpleEntryOperation(SimpleEntryImpl* entry,
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
                                           bool alone_in_queue)
    : entry_(entry),
      buf_(buf),
      callback_(callback),
      out_entry_(out_entry),
      offset_(offset),
      length_(length),
      type_(type),
      have_index_(have_index),
      index_(index),
      truncate_(truncate),
      optimistic_(optimistic),
      alone_in_queue_(alone_in_queue) {
}

}  // namespace disk_cache
