// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/omx_test/file_reader_util.h"

#include <stdio.h>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/filters/bitstream_converter.h"
#include "media/tools/omx_test/color_space_util.h"

namespace media {

//////////////////////////////////////////////////////////////////////////////
// BasicFileReader
BasicFileReader::BasicFileReader(const FilePath& path)
    : path_(path),
      file_(NULL) {
}

bool BasicFileReader::Initialize() {
  file_.Set(file_util::OpenFile(path_, "rb"));
  if (!file_.get()) {
    LOG(ERROR) << "unable to open " << path_.value();
  }
  return file_.get() != NULL;
}

//////////////////////////////////////////////////////////////////////////////
// YuvFileReader
YuvFileReader::YuvFileReader(const FilePath& path,
                             int width,
                             int height,
                             int loop_count,
                             bool enable_csc)
    : BasicFileReader(path),
      width_(width),
      height_(height),
      loop_count_(loop_count),
      output_nv21_(enable_csc) {
}

void YuvFileReader::Read(uint8** output, int* size) {
  if (!file()) {
    *size = 0;
    *output = NULL;
    return;
  }

  while (true) {
    scoped_array<uint8> data;
    int bytes_read = 0;

    // OMX require encoder input are delivered in frames (or planes).
    // Assume the input file is I420 YUV file.
    const int kFrameSize = width_ * height_ * 3 / 2;
    data.reset(new uint8[kFrameSize]);

    if (output_nv21_) {
      if (!csc_buf_.get())
        csc_buf_.reset(new uint8[kFrameSize]);
      bytes_read = fread(csc_buf_.get(), 1, kFrameSize, file());

      // We do not convert partial frames.
      if (bytes_read == kFrameSize)
        IYUVtoNV21(csc_buf_.get(), data.get(), width_, height_);
      else
        bytes_read = 0;  // force cleanup or loop around.
    } else {
      bytes_read = fread(data.get(), 1, kFrameSize, file());
    }

    if (bytes_read) {
      *size = bytes_read;
      *output = data.release();
      break;
    }

    // Encounter the end of file.
    if (loop_count_ == 1) {
      // Signal end of stream.
      *size = 0;
      *output = data.release();
    }

    --loop_count_;
    fseek(file(), 0, SEEK_SET);
  }
}

//////////////////////////////////////////////////////////////////////////////
// BlockFileReader
BlockFileReader::BlockFileReader(const FilePath& path,
                                 int block_size)
    : BasicFileReader(path),
      block_size_(block_size) {
}

void BlockFileReader::Read(uint8** output, int* size) {
  CHECK(file());
  *output = new uint8[block_size_];
  *size = fread(*output, 1, block_size_, file());
}

//////////////////////////////////////////////////////////////////////////////
// FFmpegFileReader
FFmpegFileReader::FFmpegFileReader(const FilePath& path)
    : path_(path),
      format_context_(NULL),
      codec_context_(NULL),
      target_stream_(-1),
      converter_(NULL) {
}

FFmpegFileReader::~FFmpegFileReader() {
  if (format_context_)
    av_close_input_file(format_context_);
}

bool FFmpegFileReader::Initialize() {
  // av_open_input_file wants a char*, which can't work with wide paths.
  // So we assume ASCII on Windows.  On other platforms we can pass the
  // path bytes through verbatim.
#if defined(OS_WIN)
  std::string string_path = WideToASCII(path_.value());
#else
  const std::string& string_path = path_.value();
#endif
  int result = av_open_input_file(&format_context_, string_path.c_str(),
                                  NULL, 0, NULL);
  if (result < 0) {
    switch (result) {
      case AVERROR_NOFMT:
        LOG(ERROR) << "Error: File format not supported "
                   << path_.value() << std::endl;
        break;
      default:
        LOG(ERROR) << "Error: Could not open input for "
                   << path_.value() << std::endl;
        break;
    }
    return false;
  }
  if (av_find_stream_info(format_context_) < 0) {
    LOG(ERROR) << "can't use FFmpeg to parse stream info";
    return false;
  }

  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    codec_context_ = format_context_->streams[i]->codec;

    // Find the video stream.
    if (codec_context_->codec_type == CODEC_TYPE_VIDEO) {
      target_stream_ = i;
      break;
    }
  }
  if (target_stream_ == -1) {
    LOG(ERROR) << "no video in the stream";
    return false;
  }

  // Initialize the bitstream filter if needed.
  // TODO(hclam): find a better way to identify mp4 container.
  if (codec_context_->codec_id == CODEC_ID_H264) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "h264_mp4toannexb", codec_context_));
  } else if (codec_context_->codec_id == CODEC_ID_MPEG4) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "mpeg4video_es", codec_context_));
  } else if (codec_context_->codec_id == CODEC_ID_WMV3) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "vc1_asftorcv", codec_context_));
  } else if (codec_context_->codec_id == CODEC_ID_VC1) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "vc1_asftoannexg", codec_context_));
  }

  if (converter_.get() && !converter_->Initialize()) {
    converter_.reset();
    LOG(ERROR) << "failed to initialize h264_mp4toannexb filter";
    return false;
  }
  return true;
}

void FFmpegFileReader::Read(uint8** output, int* size) {
  if (!format_context_ || !codec_context_ || target_stream_ == -1) {
    *size = 0;
    *output = NULL;
    return;
  }

  AVPacket packet;
  bool found = false;
  while (!found) {
    int result = av_read_frame(format_context_, &packet);
    if (result < 0) {
      *output = NULL;
      *size = 0;
      return;
    }
    if (packet.stream_index == target_stream_) {
      if (converter_.get() && !converter_->ConvertPacket(&packet)) {
        LOG(ERROR) << "failed to convert AVPacket";
      }
      *output = new uint8[packet.size];
      *size = packet.size;
      memcpy(*output, packet.data, packet.size);
      found = true;
    }
    av_free_packet(&packet);
  }
}

//////////////////////////////////////////////////////////////////////////////
// H264FileReader
const int kH264ReadSize = 1024 * 1024;

H264FileReader::H264FileReader(const FilePath& path)
    : BasicFileReader(path),
      read_buf_(new uint8[kH264ReadSize]),
      current_(0),
      used_(0) {
}

void H264FileReader::Read(uint8** output, int *size) {
  // Fill the buffer when it's less than half full.
  int read = 0;
  if (used_ < kH264ReadSize / 2) {
    read = fread(read_buf_.get(), 1, kH264ReadSize - used_, file());
    CHECK(read >= 0);
    used_ += read;
  }

  // If we failed to read.
  if (current_ == used_) {
    *output = NULL;
    *size = 0;
    return;
  }

  // Try to find start code of 0x00, 0x00, 0x01.
  bool found = false;
  int pos = current_ + 3;
  for (; pos < used_ - 2; ++pos) {
    if (read_buf_[pos] == 0 &&
        read_buf_[pos+1] == 0 &&
        read_buf_[pos+2] == 1) {
      found = true;
      break;
    }
  }

  // If next NALU is found.
  if (found) {
    CHECK(pos > current_);
    *size = pos - current_;
    *output = new uint8[*size];
    memcpy(*output, read_buf_.get() + current_, *size);
    current_ = pos;

    // If we have used_ more than half of the available buffer.
    // Then move the unused_ buffer to the front to give space
    // for more incoming output.
    if (current_ > used_ / 2) {
      CHECK(used_ > current_);
      memcpy(read_buf_.get(),
             read_buf_.get() + current_,
             used_ - current_);
      used_ = used_ - current_;
      current_ = 0;
    }
    return;
  }

  // If next NALU is not found, assume the remaining data is a NALU
  // and return the data.
  CHECK(used_ > current_);
  *size = used_ - current_;
  *output = new uint8[*size];
  memcpy(*output, read_buf_.get() + current_, *size);
  current_ = used_;
}

}  // namespace media
