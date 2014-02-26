// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mapped_file.h"

namespace disk_cache {

// Note: Most of this class is implemented in platform-specific files.

bool MappedFile::Load(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Read(block->buffer(), block->size(), offset);
}

bool MappedFile::Store(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Write(block->buffer(), block->size(), offset);
}

bool MappedFile::Load(const FileBlock* block,
                      FileIOCallback* callback,
                      bool* completed) {
  size_t offset = block->offset() + view_size_;
  return Read(block->buffer(), block->size(), offset, callback, completed);
}

bool MappedFile::Store(const FileBlock* block,
                       FileIOCallback* callback,
                       bool* completed) {
  size_t offset = block->offset() + view_size_;
  return Write(block->buffer(), block->size(), offset, callback, completed);
}

}  // namespace disk_cache
