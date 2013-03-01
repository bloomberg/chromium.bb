// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The bulk of this file is support code; sorry about that.  Here's an overview
// to hopefully help readers of this code:
// - RenderingHelper is charged with interacting with X11/{EGL/GLES2,GLX/GL} or
//   Win/EGL.
// - ClientState is an enum for the state of the decode client used by the test.
// - ClientStateNotification is a barrier abstraction that allows the test code
//   to be written sequentially and wait for the decode client to see certain
//   state transitions.
// - GLRenderingVDAClient is a VideoDecodeAccelerator::Client implementation
// - Finally actual TEST cases are at the bottom of this file, using the above
//   infrastructure.

#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

// Include gtest.h out of order because <X11/X.h> #define's Bool & None, which
// gtest uses as struct names (inside a namespace).  This means that
// #include'ing gtest after anything that pulls in X.h fails to compile.
// This is http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringize_macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "content/common/gpu/media/rendering_helper.h"
#include "content/public/common/content_switches.h"

#if defined(OS_WIN)
#include "content/common/gpu/media/dxva_video_decode_accelerator.h"
#elif defined(OS_MACOSX)
#include "content/common/gpu/media/mac_video_decode_accelerator.h"
#elif defined(OS_CHROMEOS)
#if defined(ARCH_CPU_ARMEL)
#include "content/common/gpu/media/exynos_video_decode_accelerator.h"
#include "content/common/gpu/media/omx_video_decode_accelerator.h"
#elif defined(ARCH_CPU_X86_FAMILY)
#include "content/common/gpu/media/vaapi_video_decode_accelerator.h"
#endif  // ARCH_CPU_ARMEL
#else
#error The VideoAccelerator tests are not supported on this platform.
#endif  // OS_WIN

using media::VideoDecodeAccelerator;

namespace content {
namespace {

// Values optionally filled in from flags; see main() below.
// The syntax of multiple test videos is:
//  test-video1;test-video2;test-video3
// where only the first video is required and other optional videos would be
// decoded by concurrent decoders.
// The syntax of each test-video is:
//  filename:width:height:numframes:numfragments:minFPSwithRender:minFPSnoRender
// where only the first field is required.  Value details:
// - |filename| must be an h264 Annex B (NAL) stream or an IVF VP8 stream.
// - |width| and |height| are in pixels.
// - |numframes| is the number of picture frames in the file.
// - |numfragments| NALU (h264) or frame (VP8) count in the stream.
// - |minFPSwithRender| and |minFPSnoRender| are minimum frames/second speeds
//   expected to be achieved with and without rendering to the screen, resp.
//   (the latter tests just decode speed).
// - |profile| is the media::VideoCodecProfile set during Initialization.
// An empty value for a numeric field means "ignore".
#if defined(OS_MACOSX)
const base::FilePath::CharType* test_video_data =
    FILE_PATH_LITERAL("test-25fps_high.h264:1280:720:250:252:50:100:4");
#else
const base::FilePath::CharType* test_video_data =
    // FILE_PATH_LITERAL("test-25fps.vp8:320:240:250:250:50:175:11");
    FILE_PATH_LITERAL("test-25fps.h264:320:240:250:258:50:175:1");
#endif

struct TestVideoFile {
  explicit TestVideoFile(base::FilePath::StringType file_name)
      : file_name(file_name),
        width(-1),
        height(-1),
        num_frames(-1),
        num_fragments(-1),
        min_fps_render(-1),
        min_fps_no_render(-1),
        profile(-1) {
  }

  base::FilePath::StringType file_name;
  int width;
  int height;
  int num_frames;
  int num_fragments;
  int min_fps_render;
  int min_fps_no_render;
  int profile;
  std::string data_str;
};

// Parse |data| into its constituent parts, set the various output fields
// accordingly, and read in video stream. CHECK-fails on unexpected or
// missing required data. Unspecified optional fields are set to -1.
void ParseAndReadTestVideoData(base::FilePath::StringType data,
                               size_t num_concurrent_decoders,
                               int reset_after_frame_num,
                               std::vector<TestVideoFile*>* test_video_files) {
  std::vector<base::FilePath::StringType> entries;
  base::SplitString(data, ';', &entries);
  CHECK_GE(entries.size(), 1U) << data;
  for (size_t index = 0; index < entries.size(); ++index) {
    std::vector<base::FilePath::StringType> fields;
    base::SplitString(entries[index], ':', &fields);
    CHECK_GE(fields.size(), 1U) << entries[index];
    CHECK_LE(fields.size(), 8U) << entries[index];
    TestVideoFile* video_file = new TestVideoFile(fields[0]);
    if (!fields[1].empty())
      CHECK(base::StringToInt(fields[1], &video_file->width));
    if (!fields[2].empty())
      CHECK(base::StringToInt(fields[2], &video_file->height));
    if (!fields[3].empty()) {
      CHECK(base::StringToInt(fields[3], &video_file->num_frames));
      // If we reset mid-stream and start playback over, account for frames
      // that are decoded twice in our expectations.
      if (video_file->num_frames > 0 && reset_after_frame_num >= 0)
        video_file->num_frames += reset_after_frame_num;
    }
    if (!fields[4].empty())
      CHECK(base::StringToInt(fields[4], &video_file->num_fragments));
    if (!fields[5].empty()) {
      CHECK(base::StringToInt(fields[5], &video_file->min_fps_render));
      video_file->min_fps_render /= num_concurrent_decoders;
    }
    if (!fields[6].empty()) {
      CHECK(base::StringToInt(fields[6], &video_file->min_fps_no_render));
      video_file->min_fps_no_render /= num_concurrent_decoders;
    }
    if (!fields[7].empty())
      CHECK(base::StringToInt(fields[7], &video_file->profile));

    // Read in the video data.
    base::FilePath filepath(video_file->file_name);
    CHECK(file_util::ReadFileToString(filepath, &video_file->data_str))
        << "test_video_file: " << filepath.MaybeAsASCII();

    test_video_files->push_back(video_file);
  }
}

// State of the GLRenderingVDAClient below.  Order matters here as the test
// makes assumptions about it.
enum ClientState {
  CS_CREATED = 0,
  CS_DECODER_SET = 1,
  CS_INITIALIZED = 2,
  CS_FLUSHING = 3,
  CS_FLUSHED = 4,
  CS_DONE = 5,
  CS_RESETTING = 6,
  CS_RESET = 7,
  CS_ERROR = 8,
  CS_DESTROYED = 9,
  CS_MAX,  // Must be last entry.
};

// Helper class allowing one thread to wait on a notification from another.
// If notifications come in faster than they are Wait()'d for, they are
// accumulated (so exactly as many Wait() calls will unblock as Notify() calls
// were made, regardless of order).
class ClientStateNotification {
 public:
  ClientStateNotification();
  ~ClientStateNotification();

  // Used to notify a single waiter of a ClientState.
  void Notify(ClientState state);
  // Used by waiters to wait for the next ClientState Notification.
  ClientState Wait();
 private:
  base::Lock lock_;
  base::ConditionVariable cv_;
  std::queue<ClientState> pending_states_for_notification_;
};

ClientStateNotification::ClientStateNotification() : cv_(&lock_) {}

ClientStateNotification::~ClientStateNotification() {}

void ClientStateNotification::Notify(ClientState state) {
  base::AutoLock auto_lock(lock_);
  pending_states_for_notification_.push(state);
  cv_.Signal();
}

ClientState ClientStateNotification::Wait() {
  base::AutoLock auto_lock(lock_);
  while (pending_states_for_notification_.empty())
    cv_.Wait();
  ClientState ret = pending_states_for_notification_.front();
  pending_states_for_notification_.pop();
  return ret;
}

// Magic constants for differentiating the reasons for NotifyResetDone being
// called.
enum ResetPoint {
  MID_STREAM_RESET = -2,
  END_OF_STREAM_RESET = -1
};

// Client that can accept callbacks from a VideoDecodeAccelerator and is used by
// the TESTs below.
class GLRenderingVDAClient : public VideoDecodeAccelerator::Client {
 public:
  // Doesn't take ownership of |rendering_helper| or |note|, which must outlive
  // |*this|.
  // |num_fragments_per_decode| counts NALUs for h264 and frames for VP8.
  // |num_play_throughs| indicates how many times to play through the video.
  // |reset_after_frame_num| can be a frame number >=0 indicating a mid-stream
  // Reset() should be done after that frame number is delivered, or
  // END_OF_STREAM_RESET to indicate no mid-stream Reset().
  // |delete_decoder_state| indicates when the underlying decoder should be
  // Destroy()'d and deleted and can take values: N<0: delete after -N Decode()
  // calls have been made, N>=0 means interpret as ClientState.
  // Both |reset_after_frame_num| & |delete_decoder_state| apply only to the
  // last play-through (governed by |num_play_throughs|).
  GLRenderingVDAClient(RenderingHelper* rendering_helper,
                       int rendering_window_id,
                       ClientStateNotification* note,
                       const std::string& encoded_data,
                       int num_fragments_per_decode,
                       int num_in_flight_decodes,
                       int num_play_throughs,
                       int reset_after_frame_num,
                       int delete_decoder_state,
                       int frame_width,
                       int frame_height,
                       int profile);
  virtual ~GLRenderingVDAClient();
  void CreateDecoder();

  // VideoDecodeAccelerator::Client implementation.
  // The heart of the Client.
  virtual void ProvidePictureBuffers(uint32 requested_num_of_buffers,
                                     const gfx::Size& dimensions,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  // Simple state changes.
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;
  virtual void NotifyError(VideoDecodeAccelerator::Error error) OVERRIDE;

  // Simple getters for inspecting the state of the Client.
  int num_done_bitstream_buffers() { return num_done_bitstream_buffers_; }
  int num_skipped_fragments() { return num_skipped_fragments_; }
  int num_queued_fragments() { return num_queued_fragments_; }
  int num_decoded_frames() { return num_decoded_frames_; }
  double frames_per_second();
  bool decoder_deleted() { return !decoder_.get(); }

 private:
  typedef std::map<int, media::PictureBuffer*> PictureBufferById;

  void SetState(ClientState new_state);

  // Delete the associated OMX decoder helper.
  void DeleteDecoder();

  // Compute & return the first encoded bytes (including a start frame) to send
  // to the decoder, starting at |start_pos| and returning
  // |num_fragments_per_decode| units. Skips to the first decodable position.
  std::string GetBytesForFirstFragments(size_t start_pos, size_t* end_pos);
  // Compute & return the next encoded bytes to send to the decoder (based on
  // |start_pos| & |num_fragments_per_decode_|).
  std::string GetBytesForNextFragments(size_t start_pos, size_t* end_pos);
  // Helpers for GetRangeForNextFragments above.
  void GetBytesForNextNALU(size_t start_pos, size_t* end_pos);  // For h.264.
  std::string GetBytesForNextFrames(
      size_t start_pos, size_t* end_pos);  // For VP8.

  // Request decode of the next batch of fragments in the encoded data.
  void DecodeNextFragments();

  RenderingHelper* rendering_helper_;
  int rendering_window_id_;
  std::string encoded_data_;
  const int num_fragments_per_decode_;
  const int num_in_flight_decodes_;
  int outstanding_decodes_;
  size_t encoded_data_next_pos_to_decode_;
  int next_bitstream_buffer_id_;
  ClientStateNotification* note_;
  scoped_ptr<VideoDecodeAccelerator> decoder_;
  std::set<int> outstanding_texture_ids_;
  int remaining_play_throughs_;
  int reset_after_frame_num_;
  int delete_decoder_state_;
  ClientState state_;
  int num_skipped_fragments_;
  int num_queued_fragments_;
  int num_decoded_frames_;
  int num_done_bitstream_buffers_;
  PictureBufferById picture_buffers_by_id_;
  base::TimeTicks initialize_done_ticks_;
  base::TimeTicks last_frame_delivered_ticks_;
  int profile_;
};

GLRenderingVDAClient::GLRenderingVDAClient(
    RenderingHelper* rendering_helper,
    int rendering_window_id,
    ClientStateNotification* note,
    const std::string& encoded_data,
    int num_fragments_per_decode,
    int num_in_flight_decodes,
    int num_play_throughs,
    int reset_after_frame_num,
    int delete_decoder_state,
    int frame_width,
    int frame_height,
    int profile)
    : rendering_helper_(rendering_helper),
      rendering_window_id_(rendering_window_id),
      encoded_data_(encoded_data),
      num_fragments_per_decode_(num_fragments_per_decode),
      num_in_flight_decodes_(num_in_flight_decodes), outstanding_decodes_(0),
      encoded_data_next_pos_to_decode_(0), next_bitstream_buffer_id_(0),
      note_(note),
      remaining_play_throughs_(num_play_throughs),
      reset_after_frame_num_(reset_after_frame_num),
      delete_decoder_state_(delete_decoder_state),
      state_(CS_CREATED),
      num_skipped_fragments_(0), num_queued_fragments_(0),
      num_decoded_frames_(0), num_done_bitstream_buffers_(0),
      profile_(profile) {
  CHECK_GT(num_fragments_per_decode, 0);
  CHECK_GT(num_in_flight_decodes, 0);
  CHECK_GT(num_play_throughs, 0);
}

GLRenderingVDAClient::~GLRenderingVDAClient() {
  DeleteDecoder();  // Clean up in case of expected error.
  CHECK(decoder_deleted());
  STLDeleteValues(&picture_buffers_by_id_);
  SetState(CS_DESTROYED);
}

#if !defined(OS_MACOSX)
static bool DoNothingReturnTrue() { return true; }
#endif

void GLRenderingVDAClient::CreateDecoder() {
  CHECK(decoder_deleted());
  CHECK(!decoder_.get());
#if defined(OS_WIN)
  decoder_.reset(new DXVAVideoDecodeAccelerator(
      this, base::Bind(&DoNothingReturnTrue)));
#elif defined(OS_MACOSX)
  decoder_.reset(new MacVideoDecodeAccelerator(
      static_cast<CGLContextObj>(rendering_helper_->GetGLContext()), this));
#elif defined(OS_CHROMEOS)
#if defined(ARCH_CPU_ARMEL)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseExynosVda)) {
    decoder_.reset(
        new ExynosVideoDecodeAccelerator(
            static_cast<EGLDisplay>(rendering_helper_->GetGLDisplay()),
            static_cast<EGLContext>(rendering_helper_->GetGLContext()),
            this, base::Bind(&DoNothingReturnTrue)));
  } else {
    decoder_.reset(
        new OmxVideoDecodeAccelerator(
            static_cast<EGLDisplay>(rendering_helper_->GetGLDisplay()),
            static_cast<EGLContext>(rendering_helper_->GetGLContext()),
            this, base::Bind(&DoNothingReturnTrue)));
  }
#elif defined(ARCH_CPU_X86_FAMILY)
  decoder_.reset(new VaapiVideoDecodeAccelerator(
      static_cast<Display*>(rendering_helper_->GetGLDisplay()),
      static_cast<GLXContext>(rendering_helper_->GetGLContext()),
      this, base::Bind(&DoNothingReturnTrue)));
#endif  // ARCH_CPU_ARMEL
#endif  // OS_WIN
  CHECK(decoder_.get());
  SetState(CS_DECODER_SET);
  if (decoder_deleted())
    return;

  // Configure the decoder.
  media::VideoCodecProfile profile = media::H264PROFILE_BASELINE;
  if (profile_ != -1)
    profile = static_cast<media::VideoCodecProfile>(profile_);
  CHECK(decoder_->Initialize(profile));
}

void GLRenderingVDAClient::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target) {
  if (decoder_deleted())
    return;
  std::vector<media::PictureBuffer> buffers;

  for (uint32 i = 0; i < requested_num_of_buffers; ++i) {
    uint32 id = picture_buffers_by_id_.size();
    uint32 texture_id;
    base::WaitableEvent done(false, false);
    rendering_helper_->CreateTexture(
        rendering_window_id_, texture_target, &texture_id, &done);
    done.Wait();
    CHECK(outstanding_texture_ids_.insert(texture_id).second);
    media::PictureBuffer* buffer =
        new media::PictureBuffer(id, dimensions, texture_id);
    CHECK(picture_buffers_by_id_.insert(std::make_pair(id, buffer)).second);
    buffers.push_back(*buffer);
  }
  decoder_->AssignPictureBuffers(buffers);
}

void GLRenderingVDAClient::DismissPictureBuffer(int32 picture_buffer_id) {
  PictureBufferById::iterator it =
      picture_buffers_by_id_.find(picture_buffer_id);
  CHECK(it != picture_buffers_by_id_.end());
  CHECK_EQ(outstanding_texture_ids_.erase(it->second->texture_id()), 1U);
  rendering_helper_->DeleteTexture(it->second->texture_id());
  delete it->second;
  picture_buffers_by_id_.erase(it);
}

void GLRenderingVDAClient::PictureReady(const media::Picture& picture) {
  // We shouldn't be getting pictures delivered after Reset has completed.
  CHECK_LT(state_, CS_RESET);

  if (decoder_deleted())
    return;
  last_frame_delivered_ticks_ = base::TimeTicks::Now();

  CHECK_LE(picture.bitstream_buffer_id(), next_bitstream_buffer_id_);
  ++num_decoded_frames_;

  // Mid-stream reset applies only to the last play-through per constructor
  // comment.
  if (remaining_play_throughs_ == 1 &&
      reset_after_frame_num_ == num_decoded_frames_) {
    reset_after_frame_num_ = MID_STREAM_RESET;
    decoder_->Reset();
    // Re-start decoding from the beginning of the stream to avoid needing to
    // know how to find I-frames and so on in this test.
    encoded_data_next_pos_to_decode_ = 0;
  }

  media::PictureBuffer* picture_buffer =
      picture_buffers_by_id_[picture.picture_buffer_id()];
  CHECK(picture_buffer);
  rendering_helper_->RenderTexture(picture_buffer->texture_id());

  decoder_->ReusePictureBuffer(picture.picture_buffer_id());
}

void GLRenderingVDAClient::NotifyInitializeDone() {
  SetState(CS_INITIALIZED);
  initialize_done_ticks_ = base::TimeTicks::Now();
  for (int i = 0; i < num_in_flight_decodes_; ++i)
    DecodeNextFragments();
  DCHECK_EQ(outstanding_decodes_, num_in_flight_decodes_);
}

void GLRenderingVDAClient::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  // TODO(fischman): this test currently relies on this notification to make
  // forward progress during a Reset().  But the VDA::Reset() API doesn't
  // guarantee this, so stop relying on it (and remove the notifications from
  // VaapiVideoDecodeAccelerator::FinishReset()).
  ++num_done_bitstream_buffers_;
  --outstanding_decodes_;
  DecodeNextFragments();
}

void GLRenderingVDAClient::NotifyFlushDone() {
  if (decoder_deleted())
    return;
  SetState(CS_FLUSHED);
  --remaining_play_throughs_;
  DCHECK_GE(remaining_play_throughs_, 0);
  if (decoder_deleted())
    return;
  decoder_->Reset();
  SetState(CS_RESETTING);
}

void GLRenderingVDAClient::NotifyResetDone() {
  if (decoder_deleted())
    return;

  if (reset_after_frame_num_ == MID_STREAM_RESET) {
    reset_after_frame_num_ = END_OF_STREAM_RESET;
    DecodeNextFragments();
    return;
  }

  if (remaining_play_throughs_) {
    encoded_data_next_pos_to_decode_ = 0;
    NotifyInitializeDone();
    return;
  }

  SetState(CS_RESET);
  if (!decoder_deleted())
    DeleteDecoder();
}

void GLRenderingVDAClient::NotifyError(VideoDecodeAccelerator::Error error) {
  SetState(CS_ERROR);
}

static bool LookingAtNAL(const std::string& encoded, size_t pos) {
  return encoded[pos] == 0 && encoded[pos + 1] == 0 &&
      encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

void GLRenderingVDAClient::SetState(ClientState new_state) {
  note_->Notify(new_state);
  state_ = new_state;
  if (!remaining_play_throughs_ && new_state == delete_decoder_state_) {
    CHECK(!decoder_deleted());
    DeleteDecoder();
  }
}

void GLRenderingVDAClient::DeleteDecoder() {
  if (decoder_deleted())
    return;
  decoder_.release()->Destroy();
  STLClearObject(&encoded_data_);
  for (std::set<int>::iterator it = outstanding_texture_ids_.begin();
       it != outstanding_texture_ids_.end(); ++it) {
    rendering_helper_->DeleteTexture(*it);
  }
  outstanding_texture_ids_.clear();
  // Cascade through the rest of the states to simplify test code below.
  for (int i = state_ + 1; i < CS_MAX; ++i)
    SetState(static_cast<ClientState>(i));
}

std::string GLRenderingVDAClient::GetBytesForFirstFragments(
    size_t start_pos, size_t* end_pos) {
  if (profile_ < media::H264PROFILE_MAX) {
    *end_pos = start_pos;
    while (*end_pos + 4 < encoded_data_.size()) {
      if ((encoded_data_[*end_pos + 4] & 0x1f) == 0x7) // SPS start frame
        return GetBytesForNextFragments(*end_pos, end_pos);
      GetBytesForNextNALU(*end_pos, end_pos);
      num_skipped_fragments_++;
    }
    *end_pos = start_pos;
    return std::string();
  }
  DCHECK_LE(profile_, media::VP8PROFILE_MAX);
  return GetBytesForNextFragments(start_pos, end_pos);
}

std::string GLRenderingVDAClient::GetBytesForNextFragments(
    size_t start_pos, size_t* end_pos) {
  if (profile_ < media::H264PROFILE_MAX) {
    size_t new_end_pos = start_pos;
    *end_pos = start_pos;
    for (int i = 0; i < num_fragments_per_decode_; ++i) {
      GetBytesForNextNALU(*end_pos, &new_end_pos);
      if (*end_pos == new_end_pos)
        break;
      *end_pos = new_end_pos;
      num_queued_fragments_++;
    }
    return encoded_data_.substr(start_pos, *end_pos - start_pos);
  }
  DCHECK_LE(profile_, media::VP8PROFILE_MAX);
  return GetBytesForNextFrames(start_pos, end_pos);
}

void GLRenderingVDAClient::GetBytesForNextNALU(
    size_t start_pos, size_t* end_pos) {
  *end_pos = start_pos;
  if (*end_pos + 4 > encoded_data_.size())
    return;
  CHECK(LookingAtNAL(encoded_data_, start_pos));
  *end_pos += 4;
  while (*end_pos + 4 <= encoded_data_.size() &&
         !LookingAtNAL(encoded_data_, *end_pos)) {
    ++*end_pos;
  }
  if (*end_pos + 3 >= encoded_data_.size())
    *end_pos = encoded_data_.size();
}

std::string GLRenderingVDAClient::GetBytesForNextFrames(
    size_t start_pos, size_t* end_pos) {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  std::string bytes;
  if (start_pos == 0)
    start_pos = 32;  // Skip IVF header.
  *end_pos = start_pos;
  for (int i = 0; i < num_fragments_per_decode_; ++i) {
    uint32 frame_size = *reinterpret_cast<uint32*>(&encoded_data_[*end_pos]);
    *end_pos += 12;  // Skip frame header.
    bytes.append(encoded_data_.substr(*end_pos, frame_size));
    *end_pos += frame_size;
    num_queued_fragments_++;
    if (*end_pos + 12 >= encoded_data_.size())
      return bytes;
  }
  return bytes;
}

void GLRenderingVDAClient::DecodeNextFragments() {
  if (decoder_deleted())
    return;
  if (encoded_data_next_pos_to_decode_ == encoded_data_.size()) {
    if (outstanding_decodes_ == 0) {
      decoder_->Flush();
      SetState(CS_FLUSHING);
    }
    return;
  }
  size_t end_pos;
  std::string next_fragment_bytes;
  if (encoded_data_next_pos_to_decode_ == 0) {
    next_fragment_bytes = GetBytesForFirstFragments(0, &end_pos);
  } else {
    next_fragment_bytes =
        GetBytesForNextFragments(encoded_data_next_pos_to_decode_, &end_pos);
  }
  size_t next_fragment_size = next_fragment_bytes.size();

  // Populate the shared memory buffer w/ the fragments, duplicate its handle,
  // and hand it off to the decoder.
  base::SharedMemory shm;
  CHECK(shm.CreateAndMapAnonymous(next_fragment_size));
  memcpy(shm.memory(), next_fragment_bytes.data(), next_fragment_size);
  base::SharedMemoryHandle dup_handle;
  CHECK(shm.ShareToProcess(base::Process::Current().handle(), &dup_handle));
  media::BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_, dup_handle, next_fragment_size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  decoder_->Decode(bitstream_buffer);
  ++outstanding_decodes_;
  encoded_data_next_pos_to_decode_ = end_pos;

  if (!remaining_play_throughs_ &&
      -delete_decoder_state_ == next_bitstream_buffer_id_) {
    DeleteDecoder();
  }
}

double GLRenderingVDAClient::frames_per_second() {
  base::TimeDelta delta = last_frame_delivered_ticks_ - initialize_done_ticks_;
  if (delta.InSecondsF() == 0)
    return 0;
  return num_decoded_frames_ / delta.InSecondsF();
}

// Test parameters:
// - Number of fragments per Decode() call.
// - Number of concurrent decoders.
// - Number of concurrent in-flight Decode() calls per decoder.
// - Number of play-throughs.
// - reset_after_frame_num: see GLRenderingVDAClient ctor.
// - delete_decoder_phase: see GLRenderingVDAClient ctor.
class VideoDecodeAcceleratorTest
    : public ::testing::TestWithParam<
  Tuple6<int, int, int, int, ResetPoint, ClientState> > {
};

// Helper so that gtest failures emit a more readable version of the tuple than
// its byte representation.
::std::ostream& operator<<(
    ::std::ostream& os,
    const Tuple6<int, int, int, int, ResetPoint, ClientState>& t) {
  return os << t.a << ", " << t.b << ", " << t.c << ", " << t.d << ", " << t.e
            << ", " << t.f;
}

// Wait for |note| to report a state and if it's not |expected_state| then
// assert |client| has deleted its decoder.
static void AssertWaitForStateOrDeleted(ClientStateNotification* note,
                                        GLRenderingVDAClient* client,
                                        ClientState expected_state) {
  ClientState state = note->Wait();
  if (state == expected_state) return;
  ASSERT_TRUE(client->decoder_deleted())
      << "Decoder not deleted but Wait() returned " << state
      << ", instead of " << expected_state;
}

// We assert a minimal number of concurrent decoders we expect to succeed.
// Different platforms can support more concurrent decoders, so we don't assert
// failure above this.
#if defined(OS_MACOSX)
enum { kMinSupportedNumConcurrentDecoders = 1 };
#else
enum { kMinSupportedNumConcurrentDecoders = 3 };
#endif

// Test the most straightforward case possible: data is decoded from a single
// chunk and rendered to the screen.
TEST_P(VideoDecodeAcceleratorTest, TestSimpleDecode) {
  // Can be useful for debugging VLOGs from OVDA.
  // logging::SetMinLogLevel(-1);

  // Required for Thread to work.  Not used otherwise.
  base::ShadowingAtExitManager at_exit_manager;

  const int num_fragments_per_decode = GetParam().a;
  const size_t num_concurrent_decoders = GetParam().b;
  const size_t num_in_flight_decodes = GetParam().c;
  const int num_play_throughs = GetParam().d;
  const int reset_after_frame_num = GetParam().e;
  const int delete_decoder_state = GetParam().f;

  std::vector<TestVideoFile*> test_video_files;
  ParseAndReadTestVideoData(test_video_data, num_concurrent_decoders,
                            reset_after_frame_num, &test_video_files);

  // Suppress GL swapping in all but a few tests, to cut down overall test
  // runtime.
  const bool suppress_swap_to_display = num_fragments_per_decode > 1;

  std::vector<ClientStateNotification*> notes(num_concurrent_decoders, NULL);
  std::vector<GLRenderingVDAClient*> clients(num_concurrent_decoders, NULL);

  // Initialize the rendering helper.
  base::Thread rendering_thread("GLRenderingVDAClientThread");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_DEFAULT;
#if defined(OS_WIN)
  // For windows the decoding thread initializes the media foundation decoder
  // which uses COM. We need the thread to be a UI thread.
  options.message_loop_type = MessageLoop::TYPE_UI;
#endif  // OS_WIN

  rendering_thread.StartWithOptions(options);
  scoped_ptr<RenderingHelper> rendering_helper(RenderingHelper::Create());

  base::WaitableEvent done(false, false);
  std::vector<gfx::Size> frame_dimensions;
  for (size_t index = 0; index < test_video_files.size(); ++index) {
    frame_dimensions.push_back(gfx::Size(
        test_video_files[index]->width, test_video_files[index]->height));
  }
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::Initialize,
                 base::Unretained(rendering_helper.get()),
                 suppress_swap_to_display, num_concurrent_decoders,
                 frame_dimensions, &done));
  done.Wait();

  // First kick off all the decoders.
  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    TestVideoFile* video_file =
        test_video_files[index % test_video_files.size()];
    ClientStateNotification* note = new ClientStateNotification();
    notes[index] = note;
    GLRenderingVDAClient* client = new GLRenderingVDAClient(
        rendering_helper.get(), index, note, video_file->data_str,
        num_fragments_per_decode, num_in_flight_decodes, num_play_throughs,
        reset_after_frame_num, delete_decoder_state, video_file->width,
        video_file->height, video_file->profile);
    clients[index] = client;

    rendering_thread.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&GLRenderingVDAClient::CreateDecoder,
                   base::Unretained(client)));

    ASSERT_EQ(note->Wait(), CS_DECODER_SET);
  }
  // Then wait for all the decodes to finish.
  // Only check performance & correctness later if we play through only once.
  bool skip_performance_and_correctness_checks = num_play_throughs > 1;
  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    ClientStateNotification* note = notes[i];
    ClientState state = note->Wait();
    if (state != CS_INITIALIZED) {
      skip_performance_and_correctness_checks = true;
      // We expect initialization to fail only when more than the supported
      // number of decoders is instantiated.  Assert here that something else
      // didn't trigger failure.
      ASSERT_GT(num_concurrent_decoders,
                static_cast<size_t>(kMinSupportedNumConcurrentDecoders));
      continue;
    }
    ASSERT_EQ(state, CS_INITIALIZED);
    for (int n = 0; n < num_play_throughs; ++n) {
      // For play-throughs other than the first, we expect initialization to
      // succeed unconditionally.
      if (n > 0) {
        ASSERT_NO_FATAL_FAILURE(
            AssertWaitForStateOrDeleted(note, clients[i], CS_INITIALIZED));
      }
      // InitializeDone kicks off decoding inside the client, so we just need to
      // wait for Flush.
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_FLUSHING));
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_FLUSHED));
      // FlushDone requests Reset().
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_RESETTING));
    }
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients[i], CS_RESET));
    // ResetDone requests Destroy().
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients[i], CS_DESTROYED));
  }
  // Finally assert that decoding went as expected.
  for (size_t i = 0; i < num_concurrent_decoders &&
           !skip_performance_and_correctness_checks; ++i) {
    // We can only make performance/correctness assertions if the decoder was
    // allowed to finish.
    if (delete_decoder_state < CS_FLUSHED)
      continue;
    GLRenderingVDAClient* client = clients[i];
    TestVideoFile* video_file = test_video_files[i % test_video_files.size()];
    if (video_file->num_frames > 0)
      EXPECT_EQ(client->num_decoded_frames(), video_file->num_frames);
    if (reset_after_frame_num < 0) {
      EXPECT_EQ(video_file->num_fragments, client->num_skipped_fragments() +
                client->num_queued_fragments());
      EXPECT_EQ(client->num_done_bitstream_buffers(),
                ceil(static_cast<double>(client->num_queued_fragments()) /
                     num_fragments_per_decode));
    }
    LOG(INFO) << "Decoder " << i << " fps: " << client->frames_per_second();
    int min_fps = suppress_swap_to_display ?
        video_file->min_fps_no_render : video_file->min_fps_render;
    if (min_fps > 0)
      EXPECT_GT(client->frames_per_second(), min_fps);
  }

  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<std::vector<GLRenderingVDAClient*> >,
                 &clients));
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<std::vector<ClientStateNotification*> >,
                 &notes));
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<std::vector<TestVideoFile*> >,
                 &test_video_files));
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::UnInitialize,
                 base::Unretained(rendering_helper.get()),
                 &done));
  done.Wait();
  rendering_thread.Stop();
};

// Test that replay after EOS works fine.
INSTANTIATE_TEST_CASE_P(
    ReplayAfterEOS, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 4, END_OF_STREAM_RESET, CS_RESET)));

// Test that Reset() mid-stream works fine and doesn't affect decoding even when
// Decode() calls are made during the reset.
INSTANTIATE_TEST_CASE_P(
    MidStreamReset, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 1, static_cast<ResetPoint>(100), CS_RESET)));

// Test that Destroy() mid-stream works fine (primarily this is testing that no
// crashes occur).
INSTANTIATE_TEST_CASE_P(
    TearDownTiming, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_DECODER_SET),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_INITIALIZED),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_FLUSHING),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_FLUSHED),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_RESETTING),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET,
                  static_cast<ClientState>(-1)),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET,
                  static_cast<ClientState>(-10)),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET,
                  static_cast<ClientState>(-100))));

// Test that decoding various variation works: multiple concurrent decoders and
// multiple fragments per Decode() call.
INSTANTIATE_TEST_CASE_P(
    DecodeVariations, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, 1, 10, 1, END_OF_STREAM_RESET, CS_RESET),
        // Tests queuing.
        MakeTuple(1, 1, 15, 1, END_OF_STREAM_RESET, CS_RESET),
        // +0 hack below to promote enum to int.
        MakeTuple(1, kMinSupportedNumConcurrentDecoders + 0, 1, 1,
                  END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(2, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(3, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(5, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(8, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        // TODO(fischman): decoding more than 15 NALUs at once breaks decode -
        // visual artifacts are introduced as well as spurious frames are
        // delivered (more pictures are returned than NALUs are fed to the
        // decoder).  Increase the "15" below when
        // http://code.google.com/p/chrome-os-partner/issues/detail?id=4378 is
        // fixed.
        MakeTuple(15, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET)));

// Find out how many concurrent decoders can go before we exhaust system
// resources.
INSTANTIATE_TEST_CASE_P(
    ResourceExhaustion, VideoDecodeAcceleratorTest,
    ::testing::Values(
        // +0 hack below to promote enum to int.
        MakeTuple(1, kMinSupportedNumConcurrentDecoders + 0, 1, 1,
                  END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, kMinSupportedNumConcurrentDecoders + 1, 1, 1,
                  END_OF_STREAM_RESET, CS_RESET)));

// TODO(fischman, vrk): add more tests!  In particular:
// - Test life-cycle: Seek/Stop/Pause/Play for a single decoder.
// - Test alternate configurations
// - Test failure conditions.
// - Test frame size changes mid-stream

}  // namespace
}  // namespace content

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  CommandLine::Init(argc, argv);

  // Needed to enable DVLOG through --vmodule.
  CHECK(logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::DONT_LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE,
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS));

  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_video_data") {
      content::test_video_data = it->second.c_str();
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
    if (it->first == switches::kUseExynosVda)
      continue;
#endif
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }

  base::ShadowingAtExitManager at_exit_manager;
  content::RenderingHelper::InitializePlatform();

#if defined(OS_WIN)
  base::WaitableEvent event(true, false);
  content::DXVAVideoDecodeAccelerator::PreSandboxInitialization(
      base::Bind(&base::WaitableEvent::Signal,
                 base::Unretained(&event)));
  event.Wait();
#elif defined(OS_CHROMEOS)
#if defined(ARCH_CPU_ARMEL)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseExynosVda))
    content::ExynosVideoDecodeAccelerator::PreSandboxInitialization();
  else
    content::OmxVideoDecodeAccelerator::PreSandboxInitialization();
#elif defined(ARCH_CPU_X86_FAMILY)
  content::VaapiVideoDecodeAccelerator::PreSandboxInitialization();
#endif  // ARCH_CPU_ARMEL
#endif  // OS_CHROMEOS

  return RUN_ALL_TESTS();
}
