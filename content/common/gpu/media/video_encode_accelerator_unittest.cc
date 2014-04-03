// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_vector.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "content/common/gpu/media/exynos_video_encode_accelerator.h"
#include "content/common/gpu/media/video_accelerator_unittest_helpers.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/h264_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::VideoEncodeAccelerator;

namespace content {
namespace {

const media::VideoFrame::Format kInputFormat = media::VideoFrame::I420;

// Arbitrarily chosen to add some depth to the pipeline.
const unsigned int kNumOutputBuffers = 4;
const unsigned int kNumExtraInputFrames = 4;
// Maximum delay between requesting a keyframe and receiving one, in frames.
// Arbitrarily chosen as a reasonable requirement.
const unsigned int kMaxKeyframeDelay = 4;
// Value to use as max frame number for keyframe detection.
const unsigned int kMaxFrameNum =
    std::numeric_limits<unsigned int>::max() - kMaxKeyframeDelay;
const uint32 kDefaultBitrate = 2000000;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
const double kBitrateTolerance = 0.1;
const uint32 kDefaultFPS = 30;

// The syntax of each test stream is:
// "in_filename:width:height:out_filename:requested_bitrate"
// - |in_filename| must be an I420 (YUV planar) raw stream
//   (see http://www.fourcc.org/yuv.php#IYUV).
// - |width| and |height| are in pixels.
// - |profile| to encode into (values of media::VideoCodecProfile).
// - |out_filename| filename to save the encoded stream to (optional).
//   Output stream is saved for the simple encode test only.
// - |requested_bitrate| requested bitrate in bits per second (optional).
//   Bitrate is only forced for tests that test bitrate.
base::FilePath::StringType test_stream_data =
    media::GetTestDataFilePath("sync_192p20_frames.yuv").value() +
    ":320:192:1:out.h264:200000";

struct TestStream {
  TestStream() : requested_bitrate(0) {}
  ~TestStream() {}

  gfx::Size size;
  base::MemoryMappedFile input_file;
  media::VideoCodecProfile requested_profile;
  std::string out_filename;
  unsigned int requested_bitrate;
};

static void ParseAndReadTestStreamData(base::FilePath::StringType data,
                                       TestStream* test_stream) {
  std::vector<base::FilePath::StringType> fields;
  base::SplitString(data, ':', &fields);
  CHECK_GE(fields.size(), 4U) << data;
  CHECK_LE(fields.size(), 6U) << data;

  base::FilePath::StringType filename = fields[0];
  int width, height;
  CHECK(base::StringToInt(fields[1], &width));
  CHECK(base::StringToInt(fields[2], &height));
  test_stream->size = gfx::Size(width, height);
  CHECK(!test_stream->size.IsEmpty());
  int profile;
  CHECK(base::StringToInt(fields[3], &profile));
  CHECK_GT(profile, media::VIDEO_CODEC_PROFILE_UNKNOWN);
  CHECK_LE(profile, media::VIDEO_CODEC_PROFILE_MAX);
  test_stream->requested_profile =
      static_cast<media::VideoCodecProfile>(profile);
  if (fields.size() >= 5 && !fields[4].empty())
    test_stream->out_filename = fields[4];
  if (fields.size() >= 6 && !fields[5].empty())
    CHECK(base::StringToUint(fields[5], &test_stream->requested_bitrate));

  CHECK(test_stream->input_file.Initialize(base::FilePath(filename)));
}

enum ClientState {
  CS_CREATED,
  CS_ENCODER_SET,
  CS_INITIALIZED,
  CS_ENCODING,
  CS_FINISHING,
  CS_FINISHED,
  CS_ERROR,
};

// Performs basic, codec-specific sanity checks on the stream buffers passed
// to ProcessStreamBuffer(): whether we've seen keyframes before non-keyframes,
// correct sequences of H.264 NALUs (SPS before PPS and before slices), etc.
// Calls given FrameFoundCallback when a complete frame is found while
// processing.
class StreamValidator {
 public:
  // To be called when a complete frame is found while processing a stream
  // buffer, passing true if the frame is a keyframe. Returns false if we
  // are not interested in more frames and further processing should be aborted.
  typedef base::Callback<bool(bool)> FrameFoundCallback;

  virtual ~StreamValidator() {}

  // Provide a StreamValidator instance for the given |profile|.
  static scoped_ptr<StreamValidator> Create(media::VideoCodecProfile profile,
                                            const FrameFoundCallback& frame_cb);

  // Process and verify contents of a bitstream buffer.
  virtual void ProcessStreamBuffer(const uint8* stream, size_t size) = 0;

 protected:
  explicit StreamValidator(const FrameFoundCallback& frame_cb)
      : frame_cb_(frame_cb) {}

  FrameFoundCallback frame_cb_;
};

class H264Validator : public StreamValidator {
 public:
  explicit H264Validator(const FrameFoundCallback& frame_cb)
      : StreamValidator(frame_cb),
        seen_sps_(false),
        seen_pps_(false),
        seen_idr_(false) {}

  void ProcessStreamBuffer(const uint8* stream, size_t size) OVERRIDE;

 private:
  // Set to true when encoder provides us with the corresponding NALU type.
  bool seen_sps_;
  bool seen_pps_;
  bool seen_idr_;
};

void H264Validator::ProcessStreamBuffer(const uint8* stream, size_t size) {
  media::H264Parser h264_parser;
  h264_parser.SetStream(stream, size);

  while (1) {
    media::H264NALU nalu;
    media::H264Parser::Result result;

    result = h264_parser.AdvanceToNextNALU(&nalu);
    if (result == media::H264Parser::kEOStream)
      break;

    ASSERT_EQ(result, media::H264Parser::kOk);

    bool keyframe = false;

    switch (nalu.nal_unit_type) {
      case media::H264NALU::kIDRSlice:
        ASSERT_TRUE(seen_sps_);
        ASSERT_TRUE(seen_pps_);
        seen_idr_ = keyframe = true;
        // fallthrough
      case media::H264NALU::kNonIDRSlice:
        ASSERT_TRUE(seen_idr_);
        if (!frame_cb_.Run(keyframe))
          return;
        break;

      case media::H264NALU::kSPS:
        seen_sps_ = true;
        break;

      case media::H264NALU::kPPS:
        ASSERT_TRUE(seen_sps_);
        seen_pps_ = true;
        break;

      default:
        break;
    }
  }
}

class VP8Validator : public StreamValidator {
 public:
  explicit VP8Validator(const FrameFoundCallback& frame_cb)
      : StreamValidator(frame_cb),
        seen_keyframe_(false) {}

  void ProcessStreamBuffer(const uint8* stream, size_t size) OVERRIDE;

 private:
  // Have we already got a keyframe in the stream?
  bool seen_keyframe_;
};

void VP8Validator::ProcessStreamBuffer(const uint8* stream, size_t size) {
  bool keyframe = !(stream[0] & 0x01);
  if (keyframe)
    seen_keyframe_ = true;

  EXPECT_TRUE(seen_keyframe_);

  frame_cb_.Run(keyframe);
  // TODO(posciak): We could be getting more frames in the buffer, but there is
  // no simple way to detect this. We'd need to parse the frames and go through
  // partition numbers/sizes. For now assume one frame per buffer.
}

// static
scoped_ptr<StreamValidator> StreamValidator::Create(
    media::VideoCodecProfile profile,
    const FrameFoundCallback& frame_cb) {
  scoped_ptr<StreamValidator> validator;

  if (profile >= media::H264PROFILE_MIN &&
      profile <= media::H264PROFILE_MAX) {
    validator.reset(new H264Validator(frame_cb));
  } else if (profile >= media::VP8PROFILE_MIN &&
             profile <= media::VP8PROFILE_MAX) {
    validator.reset(new VP8Validator(frame_cb));
  } else {
    LOG(FATAL) << "Unsupported profile: " << profile;
  }

  return validator.Pass();
}

class VEAClient : public VideoEncodeAccelerator::Client {
 public:
  VEAClient(const TestStream& test_stream,
            ClientStateNotification<ClientState>* note,
            bool save_to_file,
            unsigned int keyframe_period,
            bool force_bitrate);
  virtual ~VEAClient();
  void CreateEncoder();
  void DestroyEncoder();

  // VideoDecodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) OVERRIDE;
  void BitstreamBufferReady(int32 bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame) OVERRIDE;
  void NotifyError(VideoEncodeAccelerator::Error error) OVERRIDE;

 private:
  bool has_encoder() { return encoder_.get(); }

  void SetState(ClientState new_state);
  // Called before starting encode to set initial configuration of the encoder.
  void SetInitialConfiguration();
  // Called when encoder is done with a VideoFrame.
  void InputNoLongerNeededCallback(int32 input_id);
  // Ensure encoder has at least as many inputs as it asked for
  // via RequireBitstreamBuffers().
  void FeedEncoderWithInputs();
  // Provide the encoder with a new output buffer.
  void FeedEncoderWithOutput(base::SharedMemory* shm);
  // Feed the encoder with num_required_input_buffers_ of black frames to force
  // it to encode and return all inputs that came before this, effectively
  // flushing it.
  void FlushEncoder();

  // Called on finding a complete frame (with |keyframe| set to true for
  // keyframes) in the stream, to perform codec-independent, per-frame checks
  // and accounting. Returns false once we have collected all frames we needed.
  bool HandleEncodedFrame(bool keyframe);

  // Perform any checks required at the end of the stream, called after
  // receiving the last frame from the encoder.
  void ChecksAtFinish();

  // Prepare and return a frame wrapping the data at |position| bytes in
  // the input stream, ready to be sent to encoder.
  scoped_refptr<media::VideoFrame> PrepareInputFrame(off_t position);

  ClientState state_;
  scoped_ptr<VideoEncodeAccelerator> encoder_;

  const TestStream& test_stream_;
  ClientStateNotification<ClientState>* note_;

  // Ids assigned to VideoFrames (start at 1 for easy comparison with
  // num_encoded_frames_).
  std::set<int32> inputs_at_client_;
  int32 next_input_id_;

  // Ids for output BitstreamBuffers.
  typedef std::map<int32, base::SharedMemory*> IdToSHM;
  ScopedVector<base::SharedMemory> output_shms_;
  IdToSHM output_buffers_at_client_;
  int32 next_output_buffer_id_;

  // Current offset into input stream.
  off_t pos_in_input_stream_;
  // Calculated from input_coded_size_, in bytes.
  size_t input_buffer_size_;
  gfx::Size input_coded_size_;
  // Requested by encoder.
  unsigned int num_required_input_buffers_;
  size_t output_buffer_size_;

  // Precalculated number of frames in the stream.
  unsigned int num_frames_in_stream_;
  // Number of encoded frames we've got from the encoder thus far.
  unsigned int num_encoded_frames_;

  // True if received a keyframe while processing current bitstream buffer.
  bool seen_keyframe_in_this_buffer_;

  // True if we are to save the encoded stream to a file.
  bool save_to_file_;

  // Request a keyframe every keyframe_period_ frames.
  const unsigned int keyframe_period_;

  // Frame number for which we requested a keyframe.
  unsigned int keyframe_requested_at_;

  // True if we are asking encoder for a particular bitrate.
  bool force_bitrate_;

  // Byte size of the encoded stream (for bitrate calculation).
  size_t encoded_stream_size_;

  scoped_ptr<StreamValidator> validator_;

  // All methods of this class should be run on the same thread.
  base::ThreadChecker thread_checker_;
};

VEAClient::VEAClient(const TestStream& test_stream,
                     ClientStateNotification<ClientState>* note,
                     bool save_to_file,
                     unsigned int keyframe_period,
                     bool force_bitrate)
    : state_(CS_CREATED),
      test_stream_(test_stream),
      note_(note),
      next_input_id_(1),
      next_output_buffer_id_(0),
      pos_in_input_stream_(0),
      input_buffer_size_(0),
      num_required_input_buffers_(0),
      output_buffer_size_(0),
      num_frames_in_stream_(0),
      num_encoded_frames_(0),
      seen_keyframe_in_this_buffer_(false),
      save_to_file_(save_to_file),
      keyframe_period_(keyframe_period),
      keyframe_requested_at_(kMaxFrameNum),
      force_bitrate_(force_bitrate),
      encoded_stream_size_(0) {
  if (keyframe_period_)
    CHECK_LT(kMaxKeyframeDelay, keyframe_period_);

  validator_ = StreamValidator::Create(
      test_stream_.requested_profile,
      base::Bind(&VEAClient::HandleEncodedFrame, base::Unretained(this)));

  CHECK(validator_.get());

  if (save_to_file_) {
    CHECK(!test_stream_.out_filename.empty());
    base::FilePath out_filename(test_stream_.out_filename);
    // This creates or truncates out_filename.
    // Without it, AppendToFile() will not work.
    EXPECT_EQ(0, base::WriteFile(out_filename, NULL, 0));
  }

  thread_checker_.DetachFromThread();
}

VEAClient::~VEAClient() { CHECK(!has_encoder()); }

void VEAClient::CreateEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!has_encoder());

  encoder_.reset(new ExynosVideoEncodeAccelerator());
  SetState(CS_ENCODER_SET);

  DVLOG(1) << "Profile: " << test_stream_.requested_profile
           << ", requested bitrate: " << test_stream_.requested_bitrate;
  if (!encoder_->Initialize(kInputFormat,
                            test_stream_.size,
                            test_stream_.requested_profile,
                            test_stream_.requested_bitrate,
                            this)) {
    DLOG(ERROR) << "VideoEncodeAccelerator::Initialize() failed";
    SetState(CS_ERROR);
    return;
  }
  SetInitialConfiguration();
  SetState(CS_INITIALIZED);
}

void VEAClient::DestroyEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;
  encoder_.release()->Destroy();
}

void VEAClient::RequireBitstreamBuffers(unsigned int input_count,
                                        const gfx::Size& input_coded_size,
                                        size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_EQ(state_, CS_INITIALIZED);
  SetState(CS_ENCODING);

  // TODO(posciak): For now we only support input streams that meet encoder
  // size requirements exactly (i.e. coded size == visible size).
  input_coded_size_ = input_coded_size;
  ASSERT_EQ(input_coded_size_, test_stream_.size);

  input_buffer_size_ = media::VideoFrame::AllocationSize(kInputFormat,
                                                         input_coded_size_);
  CHECK_GT(input_buffer_size_, 0UL);

  // ARM performs CPU cache management with CPU cache line granularity. We thus
  // need to ensure our buffers are CPU cache line-aligned (64 byte-aligned).
  // Otherwise newer kernels will refuse to accept them, and on older kernels
  // we'll be treating ourselves to random corruption.
  // Since we are just mmapping and passing chunks of the input file, to ensure
  // alignment, if the starting virtual addresses of the frames in it were not
  // 64 byte-aligned, we'd have to use a separate set of input buffers and copy
  // the frames into them before sending to the encoder. It would have been an
  // overkill here though, because, for now at least, we only test resolutions
  // that result in proper alignment, and it would have also interfered with
  // performance testing. So just assert that the frame size is a multiple of
  // 64 bytes. This ensures all frames start at 64-byte boundary, because
  // MemoryMappedFile should be mmapp()ed at virtual page start as well.
  ASSERT_EQ(input_buffer_size_ & 63, 0)
      << "Frame size has to be a multiple of 64 bytes";
  ASSERT_EQ(reinterpret_cast<off_t>(test_stream_.input_file.data()) & 63, 0)
      << "Mapped file should be mapped at a 64 byte boundary";

  num_required_input_buffers_ = input_count;
  ASSERT_GT(num_required_input_buffers_, 0UL);

  num_frames_in_stream_ = test_stream_.input_file.length() / input_buffer_size_;
  CHECK_GT(num_frames_in_stream_, 0UL);
  CHECK_LE(num_frames_in_stream_, kMaxFrameNum);
  CHECK_EQ(num_frames_in_stream_ * input_buffer_size_,
           test_stream_.input_file.length());

  output_buffer_size_ = output_size;
  ASSERT_GT(output_buffer_size_, 0UL);

  for (unsigned int i = 0; i < kNumOutputBuffers; ++i) {
    base::SharedMemory* shm = new base::SharedMemory();
    CHECK(shm->CreateAndMapAnonymous(output_buffer_size_));
    output_shms_.push_back(shm);
    FeedEncoderWithOutput(shm);
  }

  FeedEncoderWithInputs();
}

void VEAClient::BitstreamBufferReady(int32 bitstream_buffer_id,
                                     size_t payload_size,
                                     bool key_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_LE(payload_size, output_buffer_size_);

  IdToSHM::iterator it = output_buffers_at_client_.find(bitstream_buffer_id);
  ASSERT_NE(it, output_buffers_at_client_.end());
  base::SharedMemory* shm = it->second;
  output_buffers_at_client_.erase(it);

  if (state_ == CS_FINISHED)
    return;

  encoded_stream_size_ += payload_size;

  const uint8* stream_ptr = static_cast<const uint8*>(shm->memory());
  if (payload_size > 0)
    validator_->ProcessStreamBuffer(stream_ptr, payload_size);

  EXPECT_EQ(key_frame, seen_keyframe_in_this_buffer_);
  seen_keyframe_in_this_buffer_ = false;

  if (save_to_file_) {
    int size = base::checked_cast<int>(payload_size);
    EXPECT_EQ(base::AppendToFile(
                  base::FilePath::FromUTF8Unsafe(test_stream_.out_filename),
                  static_cast<char*>(shm->memory()),
                  size),
              size);
  }

  FeedEncoderWithOutput(shm);
}

void VEAClient::NotifyError(VideoEncodeAccelerator::Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetState(CS_ERROR);
}

void VEAClient::SetState(ClientState new_state) {
  note_->Notify(new_state);
  state_ = new_state;
}

void VEAClient::SetInitialConfiguration() {
  if (force_bitrate_) {
    CHECK_GT(test_stream_.requested_bitrate, 0UL);
    encoder_->RequestEncodingParametersChange(test_stream_.requested_bitrate,
                                              kDefaultFPS);
  }
}

void VEAClient::InputNoLongerNeededCallback(int32 input_id) {
  std::set<int32>::iterator it = inputs_at_client_.find(input_id);
  ASSERT_NE(it, inputs_at_client_.end());
  inputs_at_client_.erase(it);
  FeedEncoderWithInputs();
}

scoped_refptr<media::VideoFrame> VEAClient::PrepareInputFrame(off_t position) {
  CHECK_LE(position + input_buffer_size_, test_stream_.input_file.length());

  uint8* frame_data =
      const_cast<uint8*>(test_stream_.input_file.data() + position);

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalYuvData(
          kInputFormat,
          input_coded_size_,
          gfx::Rect(test_stream_.size),
          test_stream_.size,
          input_coded_size_.width(),
          input_coded_size_.width() / 2,
          input_coded_size_.width() / 2,
          frame_data,
          frame_data + input_coded_size_.GetArea(),
          frame_data + (input_coded_size_.GetArea() * 5 / 4),
          base::TimeDelta(),
          media::BindToCurrentLoop(
              base::Bind(&VEAClient::InputNoLongerNeededCallback,
                         base::Unretained(this),
                         next_input_id_)));

  CHECK(inputs_at_client_.insert(next_input_id_).second);
  ++next_input_id_;

  return frame;
}

void VEAClient::FeedEncoderWithInputs() {
  if (!has_encoder())
    return;

  if (state_ != CS_ENCODING)
    return;

  while (inputs_at_client_.size() <
         num_required_input_buffers_ + kNumExtraInputFrames) {
    size_t bytes_left = test_stream_.input_file.length() - pos_in_input_stream_;
    if (bytes_left < input_buffer_size_) {
      DCHECK_EQ(bytes_left, 0UL);
      FlushEncoder();
      return;
    }

    bool force_keyframe = false;
    if (keyframe_period_ && next_input_id_ % keyframe_period_ == 0) {
      keyframe_requested_at_ = next_input_id_;
      force_keyframe = true;
    }

    scoped_refptr<media::VideoFrame> video_frame =
        PrepareInputFrame(pos_in_input_stream_);
    pos_in_input_stream_ += input_buffer_size_;

    encoder_->Encode(video_frame, force_keyframe);
  }
}

void VEAClient::FeedEncoderWithOutput(base::SharedMemory* shm) {
  if (!has_encoder())
    return;

  if (state_ != CS_ENCODING && state_ != CS_FINISHING)
    return;

  base::SharedMemoryHandle dup_handle;
  CHECK(shm->ShareToProcess(base::Process::Current().handle(), &dup_handle));

  media::BitstreamBuffer bitstream_buffer(
      next_output_buffer_id_++, dup_handle, output_buffer_size_);
  CHECK(output_buffers_at_client_.insert(std::make_pair(bitstream_buffer.id(),
                                                        shm)).second);
  encoder_->UseOutputBitstreamBuffer(bitstream_buffer);
}

void VEAClient::FlushEncoder() {
  ASSERT_EQ(state_, CS_ENCODING);
  SetState(CS_FINISHING);

  // Feed the encoder with an additional set of num_required_input_buffers_
  // to flush it, using the first frame in the input stream. The resulting
  // encoded frames will be ignored.
  for (unsigned int i = 0; i < num_required_input_buffers_; ++i) {
    scoped_refptr<media::VideoFrame> frame = PrepareInputFrame(0);
    encoder_->Encode(frame, false);
  }
}

bool VEAClient::HandleEncodedFrame(bool keyframe) {
  // This would be a bug in the test, which should not ignore false
  // return value from this method.
  CHECK_LE(num_encoded_frames_, num_frames_in_stream_);

  ++num_encoded_frames_;
  if (keyframe) {
    // Got keyframe, reset keyframe detection regardless of whether we
    // got a frame in time or not.
    keyframe_requested_at_ = kMaxFrameNum;
    seen_keyframe_in_this_buffer_ = true;
  }

  // Because the keyframe behavior requirements are loose, we give
  // the encoder more freedom here. It could either deliver a keyframe
  // immediately after we requested it, which could be for a frame number
  // before the one we requested it for (if the keyframe request
  // is asynchronous, i.e. not bound to any concrete frame, and because
  // the pipeline can be deeper than one frame), at that frame, or after.
  // So the only constraints we put here is that we get a keyframe not
  // earlier than we requested one (in time), and not later than
  // kMaxKeyframeDelay frames after the frame, for which we requested
  // it, comes back encoded.
  EXPECT_LE(num_encoded_frames_, keyframe_requested_at_ + kMaxKeyframeDelay);

  if (num_encoded_frames_ == num_frames_in_stream_) {
    ChecksAtFinish();
    SetState(CS_FINISHED);
    return false;
  }

  return true;
}

void VEAClient::ChecksAtFinish() {
  unsigned int bitrate =
      encoded_stream_size_ * 8 * kDefaultFPS / num_frames_in_stream_;
  DVLOG(1) << "Final bitrate: " << bitrate
           << " num frames: " << num_frames_in_stream_;
  if (force_bitrate_) {
    EXPECT_NEAR(bitrate,
                test_stream_.requested_bitrate,
                kBitrateTolerance * test_stream_.requested_bitrate);
  }
}

// Test parameters:
// - If true, save output to file (provided an output filename was supplied).
// - Force a keyframe every n frames.
// - Force bitrate; the actual required value is provided as a property
//   of the input stream, because it depends on stream type/resolution/etc.
class VideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<Tuple3<bool, int, bool> > {};

TEST_P(VideoEncodeAcceleratorTest, TestSimpleEncode) {
  const unsigned int keyframe_period = GetParam().b;
  const bool force_bitrate = GetParam().c;

  TestStream test_stream;
  ParseAndReadTestStreamData(test_stream_data, &test_stream);

  // Disregard save_to_file if we didn't get an output filename.
  const bool save_to_file = GetParam().a && !test_stream.out_filename.empty();

  if (test_stream.requested_bitrate == 0)
    test_stream.requested_bitrate = kDefaultBitrate;

  base::Thread encoder_thread("EncoderThread");
  encoder_thread.Start();

  ClientStateNotification<ClientState> note;
  scoped_ptr<VEAClient> client(new VEAClient(test_stream,
                                             &note,
                                             save_to_file,
                                             keyframe_period,
                                             force_bitrate));

  encoder_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VEAClient::CreateEncoder, base::Unretained(client.get())));

  ASSERT_EQ(note.Wait(), CS_ENCODER_SET);
  ASSERT_EQ(note.Wait(), CS_INITIALIZED);
  ASSERT_EQ(note.Wait(), CS_ENCODING);
  ASSERT_EQ(note.Wait(), CS_FINISHING);
  ASSERT_EQ(note.Wait(), CS_FINISHED);

  encoder_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VEAClient::DestroyEncoder, base::Unretained(client.get())));

  encoder_thread.Stop();
}

INSTANTIATE_TEST_CASE_P(SimpleEncode,
                        VideoEncodeAcceleratorTest,
                        ::testing::Values(MakeTuple(true, 0, false)));

INSTANTIATE_TEST_CASE_P(ForceKeyframes,
                        VideoEncodeAcceleratorTest,
                        ::testing::Values(MakeTuple(false, 10, false)));

INSTANTIATE_TEST_CASE_P(ForceBitrate,
                        VideoEncodeAcceleratorTest,
                        ::testing::Values(MakeTuple(false, 0, true)));

// TODO(posciak): more tests:
// - async FeedEncoderWithOutput
// - out-of-order return of outputs to encoder
// - dynamic, runtime bitrate changes
// - multiple encoders
// - multiple encoders + decoders
// - mid-stream encoder_->Destroy()

}  // namespace
}  // namespace content

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  CommandLine::Init(argc, argv);

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end();
       ++it) {
    if (it->first == "test_stream_data") {
      content::test_stream_data = it->second.c_str();
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }

  base::ShadowingAtExitManager at_exit_manager;

  return RUN_ALL_TESTS();
}
