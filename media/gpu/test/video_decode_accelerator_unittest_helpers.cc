// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/rendering_helper.h"
#include "media/video/h264_parser.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#endif

namespace media {
namespace test {

namespace {
const size_t kMD5StringLength = 32;
}  // namespace

VideoDecodeAcceleratorTestEnvironment::VideoDecodeAcceleratorTestEnvironment(
    bool use_gl_renderer)
    : use_gl_renderer_(use_gl_renderer),
      rendering_thread_("GLRenderingVDAClientThread") {}

VideoDecodeAcceleratorTestEnvironment::
    ~VideoDecodeAcceleratorTestEnvironment() {}

void VideoDecodeAcceleratorTestEnvironment::SetUp() {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::UI;
  rendering_thread_.StartWithOptions(options);

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  rendering_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RenderingHelper::InitializeOneOff,
                                use_gl_renderer_, &done));
  done.Wait();

#if defined(OS_CHROMEOS)
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  // Need to initialize after the rendering side since the rendering side
  // initializes the "GPU" parts of Ozone.
  //
  // This also needs to be done in the test environment since this shouldn't
  // be initialized multiple times for the same Ozone platform.
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif
}

void VideoDecodeAcceleratorTestEnvironment::TearDown() {
#if defined(OS_CHROMEOS)
  gpu_helper_.reset();
#endif
  rendering_thread_.Stop();
}

scoped_refptr<base::SingleThreadTaskRunner>
VideoDecodeAcceleratorTestEnvironment::GetRenderingTaskRunner() const {
  return rendering_thread_.task_runner();
}

EncodedDataHelper::EncodedDataHelper(const std::string& data,
                                     VideoCodecProfile profile)
    : data_(data), profile_(profile) {}

EncodedDataHelper::EncodedDataHelper(const std::vector<uint8_t>& stream,
                                     VideoCodecProfile profile)
    : EncodedDataHelper(
          std::string(reinterpret_cast<const char*>(stream.data()),
                      stream.size()),
          profile) {}

EncodedDataHelper::~EncodedDataHelper() {
  base::STLClearObject(&data_);
}

bool EncodedDataHelper::IsNALHeader(const std::string& data, size_t pos) {
  return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 &&
         data[pos + 3] == 1;
}

std::string EncodedDataHelper::GetBytesForNextData() {
  switch (VideoCodecProfileToVideoCodec(profile_)) {
    case kCodecH264:
      return GetBytesForNextFragment();
    case kCodecVP8:
    case kCodecVP9:
      return GetBytesForNextFrame();
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string EncodedDataHelper::GetBytesForNextFragment() {
  if (next_pos_to_decode_ == 0) {
    size_t skipped_fragments_count = 0;
    if (!LookForSPS(&skipped_fragments_count)) {
      next_pos_to_decode_ = 0;
      return std::string();
    }
    num_skipped_fragments_ += skipped_fragments_count;
  }

  size_t start_pos = next_pos_to_decode_;
  size_t next_nalu_pos = GetBytesForNextNALU(start_pos);

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = next_nalu_pos;
  return data_.substr(start_pos, next_nalu_pos - start_pos);
}

size_t EncodedDataHelper::GetBytesForNextNALU(size_t start_pos) {
  size_t pos = start_pos;
  if (pos + 4 > data_.size())
    return pos;
  LOG_ASSERT(IsNALHeader(data_, pos));
  pos += 4;
  while (pos + 4 <= data_.size() && !IsNALHeader(data_, pos)) {
    ++pos;
  }
  if (pos + 3 >= data_.size())
    pos = data_.size();
  return pos;
}

bool EncodedDataHelper::LookForSPS(size_t* skipped_fragments_count) {
  *skipped_fragments_count = 0;
  while (next_pos_to_decode_ + 4 < data_.size()) {
    if ((data_[next_pos_to_decode_ + 4] & 0x1f) == 0x7) {
      return true;
    }
    *skipped_fragments_count += 1;
    next_pos_to_decode_ = GetBytesForNextNALU(next_pos_to_decode_);
  }
  return false;
}

std::string EncodedDataHelper::GetBytesForNextFrame() {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  size_t pos = next_pos_to_decode_;
  std::string bytes;
  if (pos == 0)
    pos = 32;  // Skip IVF header.

  uint32_t frame_size = *reinterpret_cast<uint32_t*>(&data_[pos]);
  pos += 12;  // Skip frame header.
  bytes.append(data_.substr(pos, frame_size));

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = pos + frame_size;
  return bytes;
}

// static
bool EncodedDataHelper::HasConfigInfo(const uint8_t* data,
                                      size_t size,
                                      VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    H264Parser parser;
    parser.SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result != H264Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }

    return nalu.nal_unit_type == H264NALU::kSPS;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return (size > 0 && !(data[0] & 0x01));
  }
  // Shouldn't happen at this point.
  LOG(FATAL) << "Invalid profile: " << GetProfileName(profile);
  return false;
}

// Read in golden MD5s for the thumbnailed rendering of this video
std::vector<std::string> ReadGoldenThumbnailMD5s(
    const base::FilePath& md5_file_path) {
  std::vector<std::string> golden_md5s;
  std::vector<std::string> md5_strings;
  std::string all_md5s;
  base::ReadFileToString(md5_file_path, &all_md5s);
  md5_strings = base::SplitString(all_md5s, "\n", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  // Check these are legitimate MD5s.
  for (const std::string& md5_string : md5_strings) {
    // Ignore the empty string added by SplitString
    if (!md5_string.length())
      continue;
    // Ignore comments
    if (md5_string.at(0) == '#')
      continue;

    bool valid_length = md5_string.length() == kMD5StringLength;
    LOG_IF(ERROR, !valid_length) << "MD5 length error: " << md5_string;
    bool hex_only = std::count_if(md5_string.begin(), md5_string.end(),
                                  isxdigit) == kMD5StringLength;
    LOG_IF(ERROR, !hex_only) << "MD5 includes non-hex char: " << md5_string;
    if (valid_length && hex_only)
      golden_md5s.push_back(md5_string);
  }
  LOG_IF(ERROR, md5_strings.empty())
      << "  MD5 checksum file (" << md5_file_path.MaybeAsASCII()
      << ") missing or empty.";
  return golden_md5s;
}

bool ConvertRGBAToRGB(const std::vector<unsigned char>& rgba,
                      std::vector<unsigned char>* rgb) {
  size_t num_pixels = rgba.size() / 4;
  rgb->resize(num_pixels * 3);
  // Drop the alpha channel, but check as we go that it is all 0xff.
  bool solid = true;
  for (size_t i = 0; i < num_pixels; i++) {
    (*rgb)[3 * i] = rgba[4 * i];
    (*rgb)[3 * i + 1] = rgba[4 * i + 1];
    (*rgb)[3 * i + 2] = rgba[4 * i + 2];
    solid = solid && (rgba[4 * i + 3] == 0xff);
  }
  return solid;
}
}  // namespace test
}  // namespace media
