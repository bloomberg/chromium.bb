// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_OMX_TEST_FILE_READER_UTIL_H_
#define MEDIA_TOOLS_OMX_TEST_FILE_READER_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"

struct AVCodecContext;
struct AVFormatContext;

namespace media {

class BitstreamConverter;

// A class to help reading and parsing input file for use in omx_test.
class FileReader {
 public:
  virtual ~FileReader() {}

  // Initialize FileReader object, returns true if successful.
  virtual bool Initialize() = 0;

  // Read the file into |output|, and output the number of bytes read to
  // |size|.
  virtual void Read(uint8** output, int* size) = 0;
};

class BasicFileReader : public FileReader {
 public:
  explicit BasicFileReader(const FilePath& path);
  virtual bool Initialize();
  virtual void Read(uint8** output, int* size) = 0;

 protected:
  FILE* file() const { return file_.get(); }

 private:
  FilePath path_;
  ScopedStdioHandle file_;

  DISALLOW_COPY_AND_ASSIGN(BasicFileReader);
};

class YuvFileReader : public BasicFileReader {
 public:
  // Construct a YUV file reader with looping and color space conversion
  // ability. |loop_count| specifies the number of times the input file
  // is read. If |enable_csc| is true, input in YV420 is converted to
  // NV21.
  // TODO(jiesun): Make color space more generic not a hard coded color
  // space conversion.
  YuvFileReader(const FilePath& path,
                int width,
                int height,
                int loop_count,
                bool output_nv21);
  virtual ~YuvFileReader();

  virtual void Read(uint8** output, int* size);

 private:
  int width_;
  int height_;
  int loop_count_;
  bool output_nv21_;
  scoped_array<uint8> csc_buf_;

  DISALLOW_COPY_AND_ASSIGN(YuvFileReader);
};

class BlockFileReader : public BasicFileReader {
 public:
  BlockFileReader(const FilePath& path,
                  int block_size);
  virtual void Read(uint8** output, int* size);

 private:
  int block_size_;

  DISALLOW_COPY_AND_ASSIGN(BlockFileReader);
};

class FFmpegFileReader : public FileReader {
 public:
  explicit FFmpegFileReader(const FilePath& path);
  virtual ~FFmpegFileReader();
  virtual bool Initialize();
  virtual void Read(uint8** output, int* size);

 private:
  FilePath path_;
  AVFormatContext* format_context_;
  AVCodecContext* codec_context_;
  int target_stream_;
  scoped_ptr<media::BitstreamConverter> converter_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegFileReader);
};

class H264FileReader : public BasicFileReader {
 public:
  explicit H264FileReader(const FilePath& path);
  virtual ~H264FileReader();
  virtual void Read(uint8** output, int* size);

 private:
  scoped_array<uint8> read_buf_;
  int current_;
  int used_;

  DISALLOW_COPY_AND_ASSIGN(H264FileReader);
};

}  // namespace media

#endif  // MEDIA_TOOLS_OMX_TEST_FILE_READER_UTIL_H_
