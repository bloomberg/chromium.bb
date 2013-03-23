// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/memory_allocator.h"

#include <map>

#include "base/file_util.h"
#include "base/stringprintf.h"

#if defined(OS_WIN)

namespace courgette {

// TempFile

TempFile::TempFile() : file_(base::kInvalidPlatformFileValue) {
}

TempFile::~TempFile() {
  Close();
}

void TempFile::Close() {
  if (valid()) {
    base::ClosePlatformFile(file_);
    file_ = base::kInvalidPlatformFileValue;
  }
}

bool TempFile::Create() {
  DCHECK(file_ == base::kInvalidPlatformFileValue);
  base::FilePath path;
  if (!file_util::CreateTemporaryFile(&path))
    return false;

  bool created = false;
  base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
  int flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_DELETE_ON_CLOSE |
              base::PLATFORM_FILE_TEMPORARY;
  file_ = base::CreatePlatformFile(path, flags, &created, &error_code);
  if (file_ == base::kInvalidPlatformFileValue)
    return false;

  return true;
}

bool TempFile::valid() const {
  return file_ != base::kInvalidPlatformFileValue;
}

base::PlatformFile TempFile::handle() const {
  return file_;
}

bool TempFile::SetSize(size_t size) {
  return base::TruncatePlatformFile(file_, size);
}

// FileMapping

FileMapping::FileMapping() : mapping_(NULL), view_(NULL) {
}

FileMapping::~FileMapping() {
  Close();
}

bool FileMapping::InitializeView(size_t size) {
  DCHECK(view_ == NULL);
  DCHECK(mapping_ != NULL);
  view_ = ::MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0, size);
  if (!view_) {
    Close();
    return false;
  }
  return true;
}

bool FileMapping::Create(HANDLE file, size_t size) {
  DCHECK(file != INVALID_HANDLE_VALUE);
  DCHECK(!valid());
  mapping_ = ::CreateFileMapping(file, NULL, PAGE_READWRITE, 0, 0, NULL);
  if (!mapping_)
    return false;

  return InitializeView(size);
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
  if (!file_.Create() ||
      !file_.SetSize(size) ||
      !mapping_.Create(file_.handle(), size)) {
    file_.Close();
    return false;
  }

  TempMapping** write = reinterpret_cast<TempMapping**>(mapping_.view());
  write[0] = this;

  return true;
}

void* TempMapping::memory() const {
  uint8* mem = reinterpret_cast<uint8*>(mapping_.view());
  // The 'this' pointer is written at the start of mapping_.view(), so
  // go past it. (See Initialize()).
  if (mem)
    mem += sizeof(this);
  DCHECK(mem);
  return mem;
}

bool TempMapping::valid() const {
  return mapping_.valid();
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
