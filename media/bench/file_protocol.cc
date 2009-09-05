// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/bench/file_protocol.h"

#include <stdio.h>
#include "base/file_util.h"
#include "base/logging.h"
#include "media/filters/ffmpeg_common.h"

namespace {

FILE* ToFile(void* data) {
  return reinterpret_cast<FILE*>(data);
}

// FFmpeg protocol interface.
int OpenContext(URLContext* h, const char* filename, int flags) {
  FILE* file = file_util::OpenFile(filename, "rb");
  if (!file)
    return AVERROR_IO;

  h->priv_data = file;
  h->flags = URL_RDONLY;
  h->is_streamed = false;
  return 0;
}

int ReadContext(URLContext* h, unsigned char* buf, int size) {
  return fread(buf, 1, size, ToFile(h->priv_data));
}

int WriteContext(URLContext* h, unsigned char* buf, int size) {
  NOTIMPLEMENTED();
  return AVERROR_IO;
}

offset_t SeekContext(URLContext* h, offset_t offset, int whence) {
#if defined(OS_WIN)
  return static_cast<offset_t> (_fseeki64(ToFile(h->priv_data),
                                          static_cast<int64>(offset),
                                          whence));
#else
  return fseek(ToFile(h->priv_data), offset, whence);
#endif
}

int CloseContext(URLContext* h) {
  if (file_util::CloseFile(ToFile(h->priv_data)))
    return 0;
  return AVERROR_IO;
}

}  // namespace

URLProtocol kFFmpegFileProtocol = {
  "file",
  &OpenContext,
  &ReadContext,
  &WriteContext,
  &SeekContext,
  &CloseContext,
};
