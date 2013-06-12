// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "mkvwriteeofreader.hpp"

#ifdef _MSC_VER
#include <share.h>  // for _SH_DENYWR
#endif

#include <cstdio>
#include <new>

namespace mkvmuxer {

MkvWriteEOFReader::MkvWriteEOFReader() : file_(NULL),
                                         writes_allowed_(true) {
}

MkvWriteEOFReader::~MkvWriteEOFReader() {
  Close();
}

int32 MkvWriteEOFReader::Write(const void* buffer, uint32 length) {
  if (file_ == NULL || !writes_allowed_)
    return -1;

  if (length == 0)
    return 0;

  if (buffer == NULL)
    return -1;

  const size_t bytes_written = fwrite(buffer, 1, length, file_);

  return (bytes_written == length) ? 0 : -1;
}

bool MkvWriteEOFReader::Open(const char* filename, bool create_temp_file) {
  if (filename == NULL && !create_temp_file)
    return false;

  if (file_ != NULL)
    return false;

#ifdef _MSC_VER
  file_ = create_temp_file ? tmpfile() : _fsopen(filename, "wb+", _SH_DENYWR);
#else
  file_ = create_temp_file ? tmpfile() : fopen(filename, "wb+");
#endif
  if (file_ == NULL)
    return false;
  return true;
}

void MkvWriteEOFReader::Close() {
  if (file_ != NULL) {
    fclose(file_);
    file_ = NULL;
  }
}

int64 MkvWriteEOFReader::Position() const {
  if (file_ == NULL)
    return 0;

#ifdef _MSC_VER
    return _ftelli64(file_);
#else
    return ftell(file_);
#endif
}

int32 MkvWriteEOFReader::Position(int64 position) {
  if (file_ == NULL)
    return -1;

#ifdef _MSC_VER
    return _fseeki64(file_, position, SEEK_SET);
#else
    return fseek(file_, position, SEEK_SET);
#endif
}

bool MkvWriteEOFReader::Seekable() const {
  return true;
}

void MkvWriteEOFReader::ElementStartNotify(uint64, int64) {
}

int MkvWriteEOFReader::Read(int64 offset, int32 len, uint8* buffer) {
  if (file_ == NULL)
    return -1;

  if (offset < 0)
    return -1;

  if (len < 0)
    return -1;

  if (writes_allowed_)
    writes_allowed_ = false;

  if (len == 0)
    return 0;

  fflush(file_);

#ifdef _MSC_VER
  int64 pos = _ftelli64(file_);
#else
  int64 pos = ftell(file_);
#endif

#ifdef WIN32
  int status = _fseeki64(file_, offset, SEEK_SET);

  if (status)
    return -1;  //error
#else
  fseek(file_, offset, SEEK_SET);
#endif

  const size_t size = fread(buffer, 1, len, file_);

#ifdef WIN32
  status = _fseeki64(file_, pos, SEEK_SET);

  if (status)
    return -1;  //error
#else
  fseek(file_, pos, SEEK_SET);
#endif

  if (size < static_cast<size_t>(len))
    return -1;  //error

  return 0;  //success
}

}  // namespace mkvmuxer
