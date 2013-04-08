// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/flash/internal_entry.h"

#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/flash/log_store.h"
#include "net/disk_cache/flash/log_store_entry.h"

using net::IOBuffer;
using net::StringIOBuffer;
using net::CompletionCallback;

namespace disk_cache {

KeyAndStreamSizes::KeyAndStreamSizes() {
}

InternalEntry::InternalEntry(const std::string& key, LogStore* store)
    : store_(store),
      entry_(new LogStoreEntry(store_)) {
  entry_->Init();
  WriteKey(entry_.get(), key);
}

InternalEntry::InternalEntry(int32 id, LogStore* store)
    : store_(store),
      entry_(new LogStoreEntry(store_, id)) {
}

InternalEntry::~InternalEntry() {
}

scoped_ptr<KeyAndStreamSizes> InternalEntry::Init() {
  scoped_ptr<KeyAndStreamSizes> null;
  if (entry_->IsNew())
    return null.Pass();
  if (!entry_->Init())
    return null.Pass();

  scoped_ptr<KeyAndStreamSizes> rv(new KeyAndStreamSizes);
  if (!ReadKey(entry_.get(), &rv->key))
    return null.Pass();
  for (int i = 0; i < kFlashLogStoreEntryNumStreams; ++i)
    rv->stream_sizes[i] = entry_->GetDataSize(i+1);
  return rv.Pass();
}

int32 InternalEntry::GetDataSize(int index) const {
  return entry_->GetDataSize(++index);
}

int InternalEntry::ReadData(int index, int offset, IOBuffer* buf, int buf_len,
                            const CompletionCallback& callback) {
  return entry_->ReadData(++index, offset, buf, buf_len);
}

int InternalEntry::WriteData(int index, int offset, IOBuffer* buf, int buf_len,
                             const CompletionCallback& callback) {
  return entry_->WriteData(++index, offset, buf, buf_len);
}

void InternalEntry::Close() {
  entry_->Close();
}

bool InternalEntry::WriteKey(LogStoreEntry* entry, const std::string& key) {
  int key_size = static_cast<int>(key.size());
  scoped_refptr<IOBuffer> key_buf(new StringIOBuffer(key));
  return entry->WriteData(0, 0, key_buf, key_size) == key_size;
}

bool InternalEntry::ReadKey(LogStoreEntry* entry, std::string* key) {
  int key_size = entry->GetDataSize(0);
  scoped_refptr<net::IOBuffer> key_buf(new net::IOBuffer(key_size));
  if (entry->ReadData(0, 0, key_buf, key_size) != key_size)
    return false;
  key->assign(key_buf->data(), key_size);
  return true;
}

}  // namespace disk_cache
