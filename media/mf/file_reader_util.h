// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Borrowed from media/tools/omx_test/file_reader_util.h.
// Added some functionalities related to timestamps on packets and Media
// Foundation.

#ifndef MEDIA_MF_FILE_READER_UTIL_H_
#define MEDIA_MF_FILE_READER_UTIL_H_

#include <string>

#include "base/basictypes.h"
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

class FFmpegFileReader : public FileReader {
 public:
  explicit FFmpegFileReader(const std::string& filename);
  virtual ~FFmpegFileReader();
  virtual bool Initialize();
  virtual void Read(uint8** output, int* size);

  // Reads a video packet, converts it into Annex B stream, and allocates a
  // buffer to |*output| and copies the contents into it.
  void Read(uint8** output, int* size, int* duration, int64* sample_time);
  bool GetFrameRate(int* num, int* denom) const;
  bool GetWidth(int* width) const;
  bool GetHeight(int* height) const;
  bool GetAspectRatio(int* num, int* denom) const;
  int64 ConvertFFmpegTimeBaseTo100Ns(int64 time_base_unit) const;
  bool end_of_stream() const { return end_of_stream_; }

 private:
  std::string filename_;
  AVFormatContext* format_context_;
  AVCodecContext* codec_context_;
  int target_stream_;
  scoped_ptr<media::BitstreamConverter> converter_;
  bool end_of_stream_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegFileReader);
};

}  // namespace media

#endif  // MEDIA_MF_FILE_READER_UTIL_H_
