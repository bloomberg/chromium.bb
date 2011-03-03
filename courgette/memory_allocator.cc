// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/memory_allocator.h"

#include <map>

#include "base/file_util.h"
#include "base/stringprintf.h"

#ifdef OS_WIN

// A helper function to help diagnose failures we've seen in the field.
// The function constructs a string containing the error and throws a runtime
// exception.  The calling convention is set to stdcall to force argument
// passing via the stack.
__declspec(noinline)
void __stdcall RuntimeError(DWORD err) {
  char buffer[20] = {0};
  wsprintfA(buffer, "err: %u", err);
  throw buffer;
}

namespace courgette {

// TempFile

TempFile::TempFile() : file_(base::kInvalidPlatformFileValue), size_(0) {
}

TempFile::~TempFile() {
  Close();
}

FilePath TempFile::PrepareTempFile() {
  FilePath path;
  if (!file_util::CreateTemporaryFile(&path))
    RuntimeError(::GetLastError());
  return path;
}

void TempFile::Close() {
  if (valid()) {
    base::ClosePlatformFile(file_);
    file_ = base::kInvalidPlatformFileValue;
    size_ = 0;
  }
}

void TempFile::Create() {
  DCHECK(file_ == base::kInvalidPlatformFileValue);
  FilePath path(PrepareTempFile());
  bool created = false;
  base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
  int flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_DELETE_ON_CLOSE |
              base::PLATFORM_FILE_TEMPORARY;
  file_ = base::CreatePlatformFile(path, flags, &created, &error_code);
  if (file_ == base::kInvalidPlatformFileValue)
    RuntimeError(error_code);
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

void TempFile::SetSize(size_t size) {
  bool set = base::TruncatePlatformFile(file_, size);
  if (!set)
    RuntimeError(::GetLastError());
  size_ = size;
}

// FileMapping

FileMapping::FileMapping() : mapping_(NULL), view_(NULL) {
}

FileMapping::~FileMapping() {
  Close();
}

void FileMapping::InitializeView(size_t size) {
  DCHECK(view_ == NULL);
  DCHECK(mapping_ != NULL);
  view_ = ::MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0, size);
  if (!view_)
    RuntimeError(::GetLastError());
}

void FileMapping::Create(HANDLE file, size_t size) {
  DCHECK(file != INVALID_HANDLE_VALUE);
  DCHECK(!valid());
  mapping_ = ::CreateFileMapping(file, NULL, PAGE_READWRITE, 0, 0, NULL);
  if (!mapping_)
    RuntimeError(::GetLastError());

  InitializeView(size);
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

void TempMapping::Initialize(size_t size) {
  // TODO(tommi): The assumption here is that the alignment of pointers (this)
  // is as strict or stricter than the alignment of the element type.  This is
  // not always true, e.g. __m128 has 16-byte alignment.
  size += sizeof(this);
  file_.Create();
  file_.SetSize(size);
  mapping_.Create(file_.handle(), size);

  TempMapping** write = reinterpret_cast<TempMapping**>(mapping_.view());
  write[0] = this;
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
