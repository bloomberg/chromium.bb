// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_vector.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "content/common/gpu/media/video_accelerator_unittest_helpers.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/h264_parser.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
#include "content/common/gpu/media/v4l2_video_encode_accelerator.h"
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY) && defined(USE_X11)
#include "content/common/gpu/media/vaapi_video_encode_accelerator.h"
#else
#error The VideoEncodeAcceleratorUnittest is not supported on this platform.
#endif

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
// Default initial bitrate.
const uint32 kDefaultBitrate = 2000000;
// Default ratio of requested_subsequent_bitrate to initial_bitrate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentBitrateRatio = 2.0;
// Default initial framerate.
const uint32 kDefaultFramerate = 30;
// Default ratio of requested_subsequent_framerate to initial_framerate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentFramerateRatio = 0.1;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
const double kBitrateTolerance = 0.1;
// Minimum required FPS throughput for the basic performance test.
const uint32 kMinPerfFPS = 30;
// Minimum (arbitrary) number of frames required to enforce bitrate requirements
// over. Streams shorter than this may be too short to realistically require
// an encoder to be able to converge to the requested bitrate over.
// The input stream will be looped as many times as needed in bitrate tests
// to reach at least this number of frames before calculating final bitrate.
const unsigned int kMinFramesForBitrateTests = 300;

// The syntax of multiple test streams is:
//  test-stream1;test-stream2;test-stream3
// The syntax of each test stream is:
// "in_filename:width:height:out_filename:requested_bitrate:requested_framerate
//  :requested_subsequent_bitrate:requested_subsequent_framerate"
// - |in_filename| must be an I420 (YUV planar) raw stream
//   (see http://www.fourcc.org/yuv.php#IYUV).
// - |width| and |height| are in pixels.
// - |profile| to encode into (values of media::VideoCodecProfile).
// - |out_filename| filename to save the encoded stream to (optional).
//   Output stream is saved for the simple encode test only.
// Further parameters are optional (need to provide preceding positional
// parameters if a specific subsequent parameter is required):
// - |requested_bitrate| requested bitrate in bits per second.
// - |requested_framerate| requested initial framerate.
// - |requested_subsequent_bitrate| bitrate to switch to in the middle of the
//                                  stream.
// - |requested_subsequent_framerate| framerate to switch to in the middle
//                                    of the stream.
//   Bitrate is only forced for tests that test bitrate.
const char* g_default_in_filename = "bear_320x192_40frames.yuv";
const char* g_default_in_parameters = ":320:192:1:out.h264:200000";
// Environment to store test stream data for all test cases.
class VideoEncodeAcceleratorTestEnvironment;
VideoEncodeAcceleratorTestEnvironment* g_env;

struct TestStream {
  TestStream()
      : num_frames(0),
        aligned_buffer_size(0),
        requested_bitrate(0),
        requested_framerate(0),
        requested_subsequent_bitrate(0),
        requested_subsequent_framerate(0) {}
  ~TestStream() {}

  gfx::Size visible_size;
  gfx::Size coded_size;
  unsigned int num_frames;

  // Original unaligned input file name provided as an argument to the test.
  // And the file must be an I420 (YUV planar) raw stream.
  std::string in_filename;

  // A temporary file used to prepare aligned input buffers of |in_filename|.
  // The file makes sure starting address of YUV planes are 64 byte-aligned.
  base::FilePath aligned_in_file;

  // The memory mapping of |aligned_in_file|
  base::MemoryMappedFile mapped_aligned_in_file;

  // Byte size of a frame of |aligned_in_file|.
  size_t aligned_buffer_size;

  // Byte size for each aligned plane of a frame
  std::vector<size_t> aligned_plane_size;

  std::string out_filename;
  media::VideoCodecProfile requested_profile;
  unsigned int requested_bitrate;
  unsigned int requested_framerate;
  unsigned int requested_subsequent_bitrate;
  unsigned int requested_subsequent_framerate;
};

inline static size_t Align64Bytes(size_t value) {
  return (value + 63) & ~63;
}

// Write |data| of |size| bytes at |offset| bytes into |file|.
static bool WriteFile(base::File* file,
                      const off_t offset,
                      const uint8* data,
                      size_t size) {
  size_t written_bytes = 0;
  while (written_bytes < size) {
    int bytes = file->Write(offset + written_bytes,
                            reinterpret_cast<const char*>(data + written_bytes),
                            size - written_bytes);
    if (bytes <= 0)
      return false;
    written_bytes += bytes;
  }
  return true;
}

// ARM performs CPU cache management with CPU cache line granularity. We thus
// need to ensure our buffers are CPU cache line-aligned (64 byte-aligned).
// Otherwise newer kernels will refuse to accept them, and on older kernels
// we'll be treating ourselves to random corruption.
// Since we are just mapping and passing chunks of the input file directly to
// the VEA as input frames to avoid copying large chunks of raw data on each
// frame and thus affecting performance measurements, we have to prepare a
// temporary file with all planes aligned to 64-byte boundaries beforehand.
static void CreateAlignedInputStreamFile(const gfx::Size& coded_size,
                                         TestStream* test_stream) {
  // Test case may have many encoders and memory should be prepared once.
  if (test_stream->coded_size == coded_size &&
      test_stream->mapped_aligned_in_file.IsValid())
    return;

  // All encoders in multiple encoder test reuse the same test_stream, make
  // sure they requested the same coded_size
  ASSERT_TRUE(!test_stream->mapped_aligned_in_file.IsValid() ||
              coded_size == test_stream->coded_size);
  test_stream->coded_size = coded_size;

  size_t num_planes = media::VideoFrame::NumPlanes(kInputFormat);
  std::vector<size_t> padding_sizes(num_planes);
  std::vector<size_t> coded_bpl(num_planes);
  std::vector<size_t> visible_bpl(num_planes);
  std::vector<size_t> visible_plane_rows(num_planes);

  // Calculate padding in bytes to be added after each plane required to keep
  // starting addresses of all planes at a 64 byte boudnary. This padding will
  // be added after each plane when copying to the temporary file.
  // At the same time we also need to take into account coded_size requested by
  // the VEA; each row of visible_bpl bytes in the original file needs to be
  // copied into a row of coded_bpl bytes in the aligned file.
  for (size_t i = 0; i < num_planes; i++) {
    size_t size =
        media::VideoFrame::PlaneAllocationSize(kInputFormat, i, coded_size);
    test_stream->aligned_plane_size.push_back(Align64Bytes(size));
    test_stream->aligned_buffer_size += test_stream->aligned_plane_size.back();

    coded_bpl[i] =
        media::VideoFrame::RowBytes(i, kInputFormat, coded_size.width());
    visible_bpl[i] = media::VideoFrame::RowBytes(
        i, kInputFormat, test_stream->visible_size.width());
    visible_plane_rows[i] = media::VideoFrame::Rows(
        i, kInputFormat, test_stream->visible_size.height());
    size_t padding_rows =
        media::VideoFrame::Rows(i, kInputFormat, coded_size.height()) -
        visible_plane_rows[i];
    padding_sizes[i] = padding_rows * coded_bpl[i] + Align64Bytes(size) - size;
  }

  base::MemoryMappedFile src_file;
  CHECK(src_file.Initialize(base::FilePath(test_stream->in_filename)));
  CHECK(base::CreateTemporaryFile(&test_stream->aligned_in_file));

  size_t visible_buffer_size = media::VideoFrame::AllocationSize(
      kInputFormat, test_stream->visible_size);
  CHECK_EQ(src_file.length() % visible_buffer_size, 0U)
      << "Stream byte size is not a product of calculated frame byte size";

  test_stream->num_frames = src_file.length() / visible_buffer_size;
  uint32 flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                 base::File::FLAG_READ;

  // Create a temporary file with coded_size length.
  base::File dest_file(test_stream->aligned_in_file, flags);
  CHECK_GT(test_stream->aligned_buffer_size, 0UL);
  dest_file.SetLength(test_stream->aligned_buffer_size *
                      test_stream->num_frames);

  const uint8* src = src_file.data();
  off_t dest_offset = 0;
  for (size_t frame = 0; frame < test_stream->num_frames; frame++) {
    for (size_t i = 0; i < num_planes; i++) {
      // Assert that each plane of frame starts at 64 byte boundary.
      ASSERT_EQ(dest_offset & 63, 0)
          << "Planes of frame should be mapped at a 64 byte boundary";
      for (size_t j = 0; j < visible_plane_rows[i]; j++) {
        CHECK(WriteFile(&dest_file, dest_offset, src, visible_bpl[i]));
        src += visible_bpl[i];
        dest_offset += coded_bpl[i];
      }
      dest_offset += padding_sizes[i];
    }
  }
  CHECK(test_stream->mapped_aligned_in_file.Initialize(dest_file.Pass()));
  // Assert that memory mapped of file starts at 64 byte boundary. So each
  // plane of frames also start at 64 byte boundary.
  ASSERT_EQ(
      reinterpret_cast<off_t>(test_stream->mapped_aligned_in_file.data()) & 63,
      0)
      << "File should be mapped at a 64 byte boundary";

  CHECK_EQ(test_stream->mapped_aligned_in_file.length() %
               test_stream->aligned_buffer_size,
           0U)
      << "Stream byte size is not a product of calculated frame byte size";
  CHECK_GT(test_stream->num_frames, 0UL);
  CHECK_LE(test_stream->num_frames, kMaxFrameNum);
}

// Parse |data| into its constituent parts, set the various output fields
// accordingly, read in video stream, and store them to |test_streams|.
static void ParseAndReadTestStreamData(const base::FilePath::StringType& data,
                                       ScopedVector<TestStream>* test_streams) {
  // Split the string to individual test stream data.
  std::vector<base::FilePath::StringType> test_streams_data;
  base::SplitString(data, ';', &test_streams_data);
  CHECK_GE(test_streams_data.size(), 1U) << data;

  // Parse each test stream data and read the input file.
  for (size_t index = 0; index < test_streams_data.size(); ++index) {
    std::vector<base::FilePath::StringType> fields;
    base::SplitString(test_streams_data[index], ':', &fields);
    CHECK_GE(fields.size(), 4U) << data;
    CHECK_LE(fields.size(), 9U) << data;
    TestStream* test_stream = new TestStream();

    test_stream->in_filename = fields[0];
    int width, height;
    CHECK(base::StringToInt(fields[1], &width));
    CHECK(base::StringToInt(fields[2], &height));
    test_stream->visible_size = gfx::Size(width, height);
    CHECK(!test_stream->visible_size.IsEmpty());
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

    if (fields.size() >= 7 && !fields[6].empty())
      CHECK(base::StringToUint(fields[6], &test_stream->requested_framerate));

    if (fields.size() >= 8 && !fields[7].empty()) {
      CHECK(base::StringToUint(fields[7],
                               &test_stream->requested_subsequent_bitrate));
    }

    if (fields.size() >= 9 && !fields[8].empty()) {
      CHECK(base::StringToUint(fields[8],
                               &test_stream->requested_subsequent_framerate));
    }
    test_streams->push_back(test_stream);
  }
}

enum ClientState {
  CS_CREATED,
  CS_ENCODER_SET,
  CS_INITIALIZED,
  CS_ENCODING,
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

  virtual void ProcessStreamBuffer(const uint8* stream, size_t size) OVERRIDE;

 private:
  // Set to true when encoder provides us with the corresponding NALU type.
  bool seen_sps_;
  bool seen_pps_;
  bool seen_idr_;

  media::H264Parser h264_parser_;
};

void H264Validator::ProcessStreamBuffer(const uint8* stream, size_t size) {
  h264_parser_.SetStream(stream, size);

  while (1) {
    media::H264NALU nalu;
    media::H264Parser::Result result;

    result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == media::H264Parser::kEOStream)
      break;

    ASSERT_EQ(media::H264Parser::kOk, result);

    bool keyframe = false;

    switch (nalu.nal_unit_type) {
      case media::H264NALU::kIDRSlice:
        ASSERT_TRUE(seen_sps_);
        ASSERT_TRUE(seen_pps_);
        seen_idr_ = true;
        // fallthrough
      case media::H264NALU::kNonIDRSlice: {
        ASSERT_TRUE(seen_idr_);

        media::H264SliceHeader shdr;
        ASSERT_EQ(media::H264Parser::kOk,
                  h264_parser_.ParseSliceHeader(nalu, &shdr));
        keyframe = shdr.IsISlice() || shdr.IsSISlice();

        if (!frame_cb_.Run(keyframe))
          return;
        break;
      }

      case media::H264NALU::kSPS: {
        int sps_id;
        ASSERT_EQ(media::H264Parser::kOk, h264_parser_.ParseSPS(&sps_id));
        seen_sps_ = true;
        break;
      }

      case media::H264NALU::kPPS: {
        ASSERT_TRUE(seen_sps_);
        int pps_id;
        ASSERT_EQ(media::H264Parser::kOk, h264_parser_.ParsePPS(&pps_id));
        seen_pps_ = true;
        break;
      }

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

  virtual void ProcessStreamBuffer(const uint8* stream, size_t size) OVERRIDE;

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
  VEAClient(TestStream* test_stream,
            ClientStateNotification<ClientState>* note,
            bool save_to_file,
            unsigned int keyframe_period,
            bool force_bitrate,
            bool test_perf,
            bool mid_stream_bitrate_switch,
            bool mid_stream_framerate_switch);
  virtual ~VEAClient();
  void CreateEncoder();
  void DestroyEncoder();

  // Return the number of encoded frames per second.
  double frames_per_second();

  // VideoDecodeAccelerator::Client implementation.
  virtual void RequireBitstreamBuffers(unsigned int input_count,
                                       const gfx::Size& input_coded_size,
                                       size_t output_buffer_size) OVERRIDE;
  virtual void BitstreamBufferReady(int32 bitstream_buffer_id,
                                    size_t payload_size,
                                    bool key_frame) OVERRIDE;
  virtual void NotifyError(VideoEncodeAccelerator::Error error) OVERRIDE;

 private:
  bool has_encoder() { return encoder_.get(); }

  void SetState(ClientState new_state);

  // Set current stream parameters to given |bitrate| at |framerate|.
  void SetStreamParameters(unsigned int bitrate, unsigned int framerate);

  // Called when encoder is done with a VideoFrame.
  void InputNoLongerNeededCallback(int32 input_id);

  // Ensure encoder has at least as many inputs as it asked for
  // via RequireBitstreamBuffers().
  void FeedEncoderWithInputs();

  // Provide the encoder with a new output buffer.
  void FeedEncoderWithOutput(base::SharedMemory* shm);

  // Called on finding a complete frame (with |keyframe| set to true for
  // keyframes) in the stream, to perform codec-independent, per-frame checks
  // and accounting. Returns false once we have collected all frames we needed.
  bool HandleEncodedFrame(bool keyframe);

  // Verify that stream bitrate has been close to current_requested_bitrate_,
  // assuming current_framerate_ since the last time VerifyStreamProperties()
  // was called. Fail the test if |force_bitrate_| is true and the bitrate
  // is not within kBitrateTolerance.
  void VerifyStreamProperties();

  // Test codec performance, failing the test if we are currently running
  // the performance test.
  void VerifyPerf();

  // Prepare and return a frame wrapping the data at |position| bytes in
  // the input stream, ready to be sent to encoder.
  scoped_refptr<media::VideoFrame> PrepareInputFrame(off_t position);

  // Update the parameters according to |mid_stream_bitrate_switch| and
  // |mid_stream_framerate_switch|.
  void UpdateTestStreamData(bool mid_stream_bitrate_switch,
                            bool mid_stream_framerate_switch);

  ClientState state_;
  scoped_ptr<VideoEncodeAccelerator> encoder_;

  TestStream* test_stream_;
  // Used to notify another thread about the state. VEAClient does not own this.
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
  gfx::Size input_coded_size_;
  // Requested by encoder.
  unsigned int num_required_input_buffers_;
  size_t output_buffer_size_;

  // Number of frames to encode. This may differ from the number of frames in
  // stream if we need more frames for bitrate tests.
  unsigned int num_frames_to_encode_;

  // Number of encoded frames we've got from the encoder thus far.
  unsigned int num_encoded_frames_;

  // Frames since last bitrate verification.
  unsigned int num_frames_since_last_check_;

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

  // Current requested bitrate.
  unsigned int current_requested_bitrate_;

  // Current expected framerate.
  unsigned int current_framerate_;

  // Byte size of the encoded stream (for bitrate calculation) since last
  // time we checked bitrate.
  size_t encoded_stream_size_since_last_check_;

  // If true, verify performance at the end of the test.
  bool test_perf_;

  scoped_ptr<StreamValidator> validator_;

  // The time when the encoding started.
  base::TimeTicks encode_start_time_;

  // The time when the last encoded frame is ready.
  base::TimeTicks last_frame_ready_time_;

  // All methods of this class should be run on the same thread.
  base::ThreadChecker thread_checker_;

  // Requested bitrate in bits per second.
  unsigned int requested_bitrate_;

  // Requested initial framerate.
  unsigned int requested_framerate_;

  // Bitrate to switch to in the middle of the stream.
  unsigned int requested_subsequent_bitrate_;

  // Framerate to switch to in the middle of the stream.
  unsigned int requested_subsequent_framerate_;
};

VEAClient::VEAClient(TestStream* test_stream,
                     ClientStateNotification<ClientState>* note,
                     bool save_to_file,
                     unsigned int keyframe_period,
                     bool force_bitrate,
                     bool test_perf,
                     bool mid_stream_bitrate_switch,
                     bool mid_stream_framerate_switch)
    : state_(CS_CREATED),
      test_stream_(test_stream),
      note_(note),
      next_input_id_(1),
      next_output_buffer_id_(0),
      pos_in_input_stream_(0),
      num_required_input_buffers_(0),
      output_buffer_size_(0),
      num_frames_to_encode_(0),
      num_encoded_frames_(0),
      num_frames_since_last_check_(0),
      seen_keyframe_in_this_buffer_(false),
      save_to_file_(save_to_file),
      keyframe_period_(keyframe_period),
      keyframe_requested_at_(kMaxFrameNum),
      force_bitrate_(force_bitrate),
      current_requested_bitrate_(0),
      current_framerate_(0),
      encoded_stream_size_since_last_check_(0),
      test_perf_(test_perf),
      requested_bitrate_(0),
      requested_framerate_(0),
      requested_subsequent_bitrate_(0),
      requested_subsequent_framerate_(0) {
  if (keyframe_period_)
    CHECK_LT(kMaxKeyframeDelay, keyframe_period_);

  validator_ = StreamValidator::Create(
      test_stream_->requested_profile,
      base::Bind(&VEAClient::HandleEncodedFrame, base::Unretained(this)));

  CHECK(validator_.get());

  if (save_to_file_) {
    CHECK(!test_stream_->out_filename.empty());
    base::FilePath out_filename(test_stream_->out_filename);
    // This creates or truncates out_filename.
    // Without it, AppendToFile() will not work.
    EXPECT_EQ(0, base::WriteFile(out_filename, NULL, 0));
  }

  // Initialize the parameters of the test streams.
  UpdateTestStreamData(mid_stream_bitrate_switch, mid_stream_framerate_switch);

  thread_checker_.DetachFromThread();
}

VEAClient::~VEAClient() { CHECK(!has_encoder()); }

void VEAClient::CreateEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!has_encoder());

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
  scoped_ptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kEncoder);
  encoder_.reset(new V4L2VideoEncodeAccelerator(device.Pass()));
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY) && defined(USE_X11)
  encoder_.reset(new VaapiVideoEncodeAccelerator(gfx::GetXDisplay()));
#endif

  SetState(CS_ENCODER_SET);

  DVLOG(1) << "Profile: " << test_stream_->requested_profile
           << ", initial bitrate: " << requested_bitrate_;
  if (!encoder_->Initialize(kInputFormat,
                            test_stream_->visible_size,
                            test_stream_->requested_profile,
                            requested_bitrate_,
                            this)) {
    DLOG(ERROR) << "VideoEncodeAccelerator::Initialize() failed";
    SetState(CS_ERROR);
    return;
  }

  SetStreamParameters(requested_bitrate_, requested_framerate_);
  SetState(CS_INITIALIZED);
}

void VEAClient::DestroyEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;
  encoder_.reset();
}

void VEAClient::UpdateTestStreamData(bool mid_stream_bitrate_switch,
                                     bool mid_stream_framerate_switch) {
  // Use defaults for bitrate/framerate if they are not provided.
  if (test_stream_->requested_bitrate == 0)
    requested_bitrate_ = kDefaultBitrate;
  else
    requested_bitrate_ = test_stream_->requested_bitrate;

  if (test_stream_->requested_framerate == 0)
    requested_framerate_ = kDefaultFramerate;
  else
    requested_framerate_ = test_stream_->requested_framerate;

  // If bitrate/framerate switch is requested, use the subsequent values if
  // provided, or, if not, calculate them from their initial values using
  // the default ratios.
  // Otherwise, if a switch is not requested, keep the initial values.
  if (mid_stream_bitrate_switch) {
    if (test_stream_->requested_subsequent_bitrate == 0)
      requested_subsequent_bitrate_ =
          requested_bitrate_ * kDefaultSubsequentBitrateRatio;
    else
      requested_subsequent_bitrate_ =
          test_stream_->requested_subsequent_bitrate;
  } else {
    requested_subsequent_bitrate_ = requested_bitrate_;
  }
  if (requested_subsequent_bitrate_ == 0)
    requested_subsequent_bitrate_ = 1;

  if (mid_stream_framerate_switch) {
    if (test_stream_->requested_subsequent_framerate == 0)
      requested_subsequent_framerate_ =
          requested_framerate_ * kDefaultSubsequentFramerateRatio;
    else
      requested_subsequent_framerate_ =
          test_stream_->requested_subsequent_framerate;
  } else {
    requested_subsequent_framerate_ = requested_framerate_;
  }
  if (requested_subsequent_framerate_ == 0)
    requested_subsequent_framerate_ = 1;
}

double VEAClient::frames_per_second() {
  base::TimeDelta duration = last_frame_ready_time_ - encode_start_time_;
  return num_encoded_frames_ / duration.InSecondsF();
}

void VEAClient::RequireBitstreamBuffers(unsigned int input_count,
                                        const gfx::Size& input_coded_size,
                                        size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_EQ(state_, CS_INITIALIZED);
  SetState(CS_ENCODING);

  CreateAlignedInputStreamFile(input_coded_size, test_stream_);

  // We may need to loop over the stream more than once if more frames than
  // provided is required for bitrate tests.
  if (force_bitrate_ && test_stream_->num_frames < kMinFramesForBitrateTests) {
    DVLOG(1) << "Stream too short for bitrate test ("
             << test_stream_->num_frames << " frames), will loop it to reach "
             << kMinFramesForBitrateTests << " frames";
    num_frames_to_encode_ = kMinFramesForBitrateTests;
  } else {
    num_frames_to_encode_ = test_stream_->num_frames;
  }

  input_coded_size_ = input_coded_size;
  num_required_input_buffers_ = input_count;
  ASSERT_GT(num_required_input_buffers_, 0UL);

  output_buffer_size_ = output_size;
  ASSERT_GT(output_buffer_size_, 0UL);

  for (unsigned int i = 0; i < kNumOutputBuffers; ++i) {
    base::SharedMemory* shm = new base::SharedMemory();
    CHECK(shm->CreateAndMapAnonymous(output_buffer_size_));
    output_shms_.push_back(shm);
    FeedEncoderWithOutput(shm);
  }

  encode_start_time_ = base::TimeTicks::Now();
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

  encoded_stream_size_since_last_check_ += payload_size;

  const uint8* stream_ptr = static_cast<const uint8*>(shm->memory());
  if (payload_size > 0)
    validator_->ProcessStreamBuffer(stream_ptr, payload_size);

  EXPECT_EQ(key_frame, seen_keyframe_in_this_buffer_);
  seen_keyframe_in_this_buffer_ = false;

  if (save_to_file_) {
    int size = base::checked_cast<int>(payload_size);
    EXPECT_EQ(base::AppendToFile(
                  base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
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
  DVLOG(4) << "Changing state " << state_ << "->" << new_state;
  note_->Notify(new_state);
  state_ = new_state;
}

void VEAClient::SetStreamParameters(unsigned int bitrate,
                                    unsigned int framerate) {
  current_requested_bitrate_ = bitrate;
  current_framerate_ = framerate;
  CHECK_GT(current_requested_bitrate_, 0UL);
  CHECK_GT(current_framerate_, 0UL);
  encoder_->RequestEncodingParametersChange(current_requested_bitrate_,
                                            current_framerate_);
  DVLOG(1) << "Switched parameters to " << current_requested_bitrate_
           << " bps @ " << current_framerate_ << " FPS";
}

void VEAClient::InputNoLongerNeededCallback(int32 input_id) {
  std::set<int32>::iterator it = inputs_at_client_.find(input_id);
  ASSERT_NE(it, inputs_at_client_.end());
  inputs_at_client_.erase(it);
  FeedEncoderWithInputs();
}

scoped_refptr<media::VideoFrame> VEAClient::PrepareInputFrame(off_t position) {
  CHECK_LE(position + test_stream_->aligned_buffer_size,
           test_stream_->mapped_aligned_in_file.length());

  uint8* frame_data_y = const_cast<uint8*>(
      test_stream_->mapped_aligned_in_file.data() + position);
  uint8* frame_data_u = frame_data_y + test_stream_->aligned_plane_size[0];
  uint8* frame_data_v = frame_data_u + test_stream_->aligned_plane_size[1];

  CHECK_GT(current_framerate_, 0U);
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalYuvData(
          kInputFormat,
          input_coded_size_,
          gfx::Rect(test_stream_->visible_size),
          test_stream_->visible_size,
          input_coded_size_.width(),
          input_coded_size_.width() / 2,
          input_coded_size_.width() / 2,
          frame_data_y,
          frame_data_u,
          frame_data_v,
          base::TimeDelta().FromMilliseconds(
              next_input_id_ * base::Time::kMillisecondsPerSecond /
              current_framerate_),
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
    size_t bytes_left =
        test_stream_->mapped_aligned_in_file.length() - pos_in_input_stream_;
    if (bytes_left < test_stream_->aligned_buffer_size) {
      DCHECK_EQ(bytes_left, 0UL);
      // Rewind if at the end of stream and we are still encoding.
      // This is to flush the encoder with additional frames from the beginning
      // of the stream, or if the stream is shorter that the number of frames
      // we require for bitrate tests.
      pos_in_input_stream_ = 0;
      continue;
    }

    bool force_keyframe = false;
    if (keyframe_period_ && next_input_id_ % keyframe_period_ == 0) {
      keyframe_requested_at_ = next_input_id_;
      force_keyframe = true;
    }

    scoped_refptr<media::VideoFrame> video_frame =
        PrepareInputFrame(pos_in_input_stream_);
    pos_in_input_stream_ += test_stream_->aligned_buffer_size;

    encoder_->Encode(video_frame, force_keyframe);
  }
}

void VEAClient::FeedEncoderWithOutput(base::SharedMemory* shm) {
  if (!has_encoder())
    return;

  if (state_ != CS_ENCODING)
    return;

  base::SharedMemoryHandle dup_handle;
  CHECK(shm->ShareToProcess(base::Process::Current().handle(), &dup_handle));

  media::BitstreamBuffer bitstream_buffer(
      next_output_buffer_id_++, dup_handle, output_buffer_size_);
  CHECK(output_buffers_at_client_.insert(std::make_pair(bitstream_buffer.id(),
                                                        shm)).second);
  encoder_->UseOutputBitstreamBuffer(bitstream_buffer);
}

bool VEAClient::HandleEncodedFrame(bool keyframe) {
  // This would be a bug in the test, which should not ignore false
  // return value from this method.
  CHECK_LE(num_encoded_frames_, num_frames_to_encode_);

  ++num_encoded_frames_;
  ++num_frames_since_last_check_;

  last_frame_ready_time_ = base::TimeTicks::Now();
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

  if (num_encoded_frames_ == num_frames_to_encode_ / 2) {
    VerifyStreamProperties();
    if (requested_subsequent_bitrate_ != current_requested_bitrate_ ||
        requested_subsequent_framerate_ != current_framerate_) {
      SetStreamParameters(requested_subsequent_bitrate_,
                          requested_subsequent_framerate_);
    }
  } else if (num_encoded_frames_ == num_frames_to_encode_) {
    VerifyPerf();
    VerifyStreamProperties();
    SetState(CS_FINISHED);
    return false;
  }

  return true;
}

void VEAClient::VerifyPerf() {
  double measured_fps = frames_per_second();
  LOG(INFO) << "Measured encoder FPS: " << measured_fps;
  if (test_perf_)
    EXPECT_GE(measured_fps, kMinPerfFPS);
}

void VEAClient::VerifyStreamProperties() {
  CHECK_GT(num_frames_since_last_check_, 0UL);
  CHECK_GT(encoded_stream_size_since_last_check_, 0UL);
  unsigned int bitrate = encoded_stream_size_since_last_check_ * 8 *
                         current_framerate_ / num_frames_since_last_check_;
  DVLOG(1) << "Current chunk's bitrate: " << bitrate
           << " (expected: " << current_requested_bitrate_
           << " @ " << current_framerate_ << " FPS,"
           << " num frames in chunk: " << num_frames_since_last_check_;

  num_frames_since_last_check_ = 0;
  encoded_stream_size_since_last_check_ = 0;

  if (force_bitrate_) {
    EXPECT_NEAR(bitrate,
                current_requested_bitrate_,
                kBitrateTolerance * current_requested_bitrate_);
  }
}

// Setup test stream data and delete temporary aligned files at the beginning
// and end of unittest. We only need to setup once for all test cases.
class VideoEncodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  VideoEncodeAcceleratorTestEnvironment(
      scoped_ptr<base::FilePath::StringType> data) {
    test_stream_data_ = data.Pass();
  }

  virtual void SetUp() {
    ParseAndReadTestStreamData(*test_stream_data_, &test_streams_);
  }

  virtual void TearDown() {
    for (size_t i = 0; i < test_streams_.size(); i++) {
      base::DeleteFile(test_streams_[i]->aligned_in_file, false);
    }
  }

  ScopedVector<TestStream> test_streams_;

 private:
  scoped_ptr<base::FilePath::StringType> test_stream_data_;
};

// Test parameters:
// - Number of concurrent encoders.
// - If true, save output to file (provided an output filename was supplied).
// - Force a keyframe every n frames.
// - Force bitrate; the actual required value is provided as a property
//   of the input stream, because it depends on stream type/resolution/etc.
// - If true, measure performance.
// - If true, switch bitrate mid-stream.
// - If true, switch framerate mid-stream.
class VideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<
          Tuple7<int, bool, int, bool, bool, bool, bool> > {};

TEST_P(VideoEncodeAcceleratorTest, TestSimpleEncode) {
  const size_t num_concurrent_encoders = GetParam().a;
  const bool save_to_file = GetParam().b;
  const unsigned int keyframe_period = GetParam().c;
  const bool force_bitrate = GetParam().d;
  const bool test_perf = GetParam().e;
  const bool mid_stream_bitrate_switch = GetParam().f;
  const bool mid_stream_framerate_switch = GetParam().g;

  ScopedVector<ClientStateNotification<ClientState> > notes;
  ScopedVector<VEAClient> clients;
  base::Thread encoder_thread("EncoderThread");
  ASSERT_TRUE(encoder_thread.Start());

  // Create all encoders.
  for (size_t i = 0; i < num_concurrent_encoders; i++) {
    size_t test_stream_index = i % g_env->test_streams_.size();
    // Disregard save_to_file if we didn't get an output filename.
    bool encoder_save_to_file =
        (save_to_file &&
         !g_env->test_streams_[test_stream_index]->out_filename.empty());

    notes.push_back(new ClientStateNotification<ClientState>());
    clients.push_back(new VEAClient(g_env->test_streams_[test_stream_index],
                                    notes.back(),
                                    encoder_save_to_file,
                                    keyframe_period,
                                    force_bitrate,
                                    test_perf,
                                    mid_stream_bitrate_switch,
                                    mid_stream_framerate_switch));

    encoder_thread.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&VEAClient::CreateEncoder,
                   base::Unretained(clients.back())));
  }

  // All encoders must pass through states in this order.
  enum ClientState state_transitions[] = {CS_ENCODER_SET, CS_INITIALIZED,
                                          CS_ENCODING, CS_FINISHED};

  // Wait for all encoders to go through all states and finish.
  // Do this by waiting for all encoders to advance to state n before checking
  // state n+1, to verify that they are able to operate concurrently.
  // It also simulates the real-world usage better, as the main thread, on which
  // encoders are created/destroyed, is a single GPU Process ChildThread.
  // Moreover, we can't have proper multithreading on X11, so this could cause
  // hard to debug issues there, if there were multiple "ChildThreads".
  for (size_t state_no = 0; state_no < arraysize(state_transitions); ++state_no)
    for (size_t i = 0; i < num_concurrent_encoders; i++)
      ASSERT_EQ(notes[i]->Wait(), state_transitions[state_no]);

  for (size_t i = 0; i < num_concurrent_encoders; ++i) {
    encoder_thread.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&VEAClient::DestroyEncoder, base::Unretained(clients[i])));
  }

  // This ensures all tasks have finished.
  encoder_thread.Stop();
}

INSTANTIATE_TEST_CASE_P(
    SimpleEncode,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, true, 0, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    EncoderPerf,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, false, 0, false, true, false, false)));

INSTANTIATE_TEST_CASE_P(
    ForceKeyframes,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, false, 10, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    ForceBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, false, 0, true, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    MidStreamParamSwitchBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, false, 0, true, false, true, false)));

INSTANTIATE_TEST_CASE_P(
    MidStreamParamSwitchFPS,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, false, 0, true, false, false, true)));

INSTANTIATE_TEST_CASE_P(
    MidStreamParamSwitchBitrateAndFPS,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(1, false, 0, true, false, true, true)));

INSTANTIATE_TEST_CASE_P(
    MultipleEncoders,
    VideoEncodeAcceleratorTest,
    ::testing::Values(MakeTuple(3, false, 0, false, false, false, false),
                      MakeTuple(3, false, 0, true, false, true, true)));

// TODO(posciak): more tests:
// - async FeedEncoderWithOutput
// - out-of-order return of outputs to encoder
// - multiple encoders + decoders
// - mid-stream encoder_->Destroy()

}  // namespace
}  // namespace content

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  base::CommandLine::Init(argc, argv);

  base::ShadowingAtExitManager at_exit_manager;
  scoped_ptr<base::FilePath::StringType> test_stream_data(
      new base::FilePath::StringType(
          media::GetTestDataFilePath(content::g_default_in_filename).value() +
          content::g_default_in_parameters));

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end();
       ++it) {
    if (it->first == "test_stream_data") {
      test_stream_data->assign(it->second.c_str());
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }

  content::g_env =
      reinterpret_cast<content::VideoEncodeAcceleratorTestEnvironment*>(
          testing::AddGlobalTestEnvironment(
              new content::VideoEncodeAcceleratorTestEnvironment(
                  test_stream_data.Pass())));

  return RUN_ALL_TESTS();
}
