// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/memory_allocator.h"

#include <map>

#include "base/file_util.h"

#ifdef OS_WIN

namespace courgette {

// TempFile

TempFile::TempFile() : file_(base::kInvalidPlatformFileValue), size_(0) {
}

TempFile::~TempFile() {
  Close();
}

void TempFile::Close() {
  if (valid()) {
    base::ClosePlatformFile(file_);
    file_ = base::kInvalidPlatformFileValue;
    size_ = 0;
  }
}

bool TempFile::Create() {
  DCHECK(file_ == base::kInvalidPlatformFileValue);
  FilePath path;
  if (file_util::CreateTemporaryFile(&path)) {
    bool created = false;
    base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
    int flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_WRITE |
                base::PLATFORM_FILE_DELETE_ON_CLOSE |
                base::PLATFORM_FILE_TEMPORARY;
    file_ = base::CreatePlatformFile(path, flags, &created, &error_code);
    PLOG_IF(ERROR, file_ == base::kInvalidPlatformFileValue)
        << "CreatePlatformFile";
  }
  return valid();
}

bool TempFile::valid() const {
  return file_ != base::kInvalidPlatformFileValue;
}

base::PlatformFile TempFile::handle() const {
  return file_;
}

size_t TempFile::size() const {
  return size_;
}

bool TempFile::SetSize(size_t size) {
  bool ret = base::TruncatePlatformFile(file_, size);
  if (ret)
    size_ = size;
  return ret;
}

// FileMapping

FileMapping::FileMapping() : mapping_(NULL), view_(NULL) {
}

FileMapping::~FileMapping() {
  Close();
}

bool FileMapping::Create(HANDLE file, size_t size) {
  DCHECK(file != INVALID_HANDLE_VALUE);
  DCHECK(!valid());
  mapping_ = ::CreateFileMapping(file, NULL, PAGE_READWRITE, 0, 0, NULL);
  if (mapping_)
    view_ = ::MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0, size);

  if (!valid()) {
    PLOG(ERROR) << "Failed to map file";
    Close();
  }

  return valid();
}

void FileMapping::Close() {
  if (view_)
    ::UnmapViewOfFile(view_);
  if (mapping_)
    ::CloseHandle(mapping_);
  mapping_ = NULL;
  view_ = NULL;
}

bool FileMapping::valid() const {
  return view_ != NULL;
}

void* FileMapping::view() const {
  return view_;
}

// TempMapping

TempMapping::TempMapping() {
}

TempMapping::~TempMapping() {
}

bool TempMapping::Initialize(size_t size) {
  // TODO(tommi): The assumption here is that the alignment of pointers (this)
  // is as strict or stricter than the alignment of the element type.  This is
  // not always true, e.g. __m128 has 16-byte alignment.
  size += sizeof(this);
  bool ret = file_.Create() && file_.SetSize(size) &&
             mapping_.Create(file_.handle(), size);
  if (ret) {
    TempMapping** write = reinterpret_cast<TempMapping**>(mapping_.view());
    write[0] = this;
  }

  return ret;
}

void* TempMapping::memory() const {
  uint8* mem = reinterpret_cast<uint8*>(mapping_.view());
  if (mem)
    mem += sizeof(this);
  DCHECK(mem);
  return mem;
}

// static
TempMapping* TempMapping::GetMappingFromPtr(void* mem) {
  TempMapping* ret = NULL;
  if (mem) {
    ret = reinterpret_cast<TempMapping**>(mem)[-1];
  }
  DCHECK(ret);
  return ret;
}

}  // namespace courgette

#endif  // OS_WIN
