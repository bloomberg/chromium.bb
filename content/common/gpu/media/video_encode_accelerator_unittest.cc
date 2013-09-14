// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_vector.h"
#include "base/process/process.h"
#include "base/safe_numerics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "content/common/gpu/media/exynos_video_encode_accelerator.h"
#include "content/common/gpu/media/h264_parser.h"
#include "content/common/gpu/media/video_accelerator_unittest_helpers.h"
#include "media/base/bind_to_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::VideoEncodeAccelerator;

namespace content {
namespace {

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
// - |out_filename| filename to save the encoded stream to.
//   Output stream is only saved in the simple encode test.
// - |requested_bitrate| requested bitrate in bits per second (optional).
//   Bitrate is only forced for tests that test bitrate.
const base::FilePath::CharType* test_stream_data =
    FILE_PATH_LITERAL("sync_192p_20frames.yuv:320:192:out.h264:100000");

struct TestStream {
  explicit TestStream(base::FilePath::StringType filename)
      : requested_bitrate(0) {}
  ~TestStream() {}

  gfx::Size size;
  base::MemoryMappedFile input_file;
  std::string out_filename;
  unsigned int requested_bitrate;
};

static void ParseAndReadTestStreamData(base::FilePath::StringType data,
                                       TestStream* test_stream) {
  std::vector<base::FilePath::StringType> fields;
  base::SplitString(data, ':', &fields);
  CHECK_GE(fields.size(), 4U) << data;
  CHECK_LE(fields.size(), 5U) << data;

  base::FilePath::StringType filename = fields[0];
  int width, height;
  CHECK(base::StringToInt(fields[1], &width));
  CHECK(base::StringToInt(fields[2], &height));
  test_stream->size = gfx::Size(width, height);
  CHECK(!test_stream->size.IsEmpty());
  test_stream->out_filename = fields[3];
  if (!fields[4].empty())
    CHECK(base::StringToUint(fields[4], &test_stream->requested_bitrate));

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
  void NotifyInitializeDone() OVERRIDE;
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
  // Perform any checks required at the end of the stream, called after
  // receiving the last frame from the encoder.
  void ChecksAtFinish();

  ClientState state_;
  scoped_ptr<VideoEncodeAccelerator> encoder_;

  const TestStream& test_stream_;
  ClientStateNotification<ClientState>* note_;

  // Ids assigned to VideoFrames (start at 1 for easy comparison with
  // num_encoded_slices_).
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

  // Calculated number of frames in the stream.
  unsigned int num_frames_in_stream_;
  // Number of encoded slices we got from encoder thus far.
  unsigned int num_encoded_slices_;

  // Set to true when encoder provides us with the corresponding NALU type.
  bool seen_sps_;
  bool seen_pps_;
  bool seen_idr_;

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

  content::H264Parser h264_parser_;

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
      num_encoded_slices_(0),
      seen_sps_(false),
      seen_pps_(false),
      seen_idr_(false),
      save_to_file_(save_to_file),
      keyframe_period_(keyframe_period),
      keyframe_requested_at_(kMaxFrameNum),
      force_bitrate_(force_bitrate),
      encoded_stream_size_(0) {
  if (keyframe_period_)
    CHECK_LT(kMaxKeyframeDelay, keyframe_period_);

  if (save_to_file_) {
    CHECK(!test_stream_.out_filename.empty());
    base::FilePath out_filename(test_stream_.out_filename);
    // This creates or truncates out_filename.
    // Without it, AppendToFile() will not work.
    EXPECT_EQ(0, file_util::WriteFile(out_filename, NULL, 0));
  }

  thread_checker_.DetachFromThread();
}

VEAClient::~VEAClient() {
  CHECK(!has_encoder());
}

void VEAClient::CreateEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!has_encoder());

  encoder_.reset(new ExynosVideoEncodeAccelerator(this));

  SetState(CS_ENCODER_SET);
  encoder_->Initialize(media::VideoFrame::I420,
                       test_stream_.size,
                       media::H264PROFILE_MAIN,
                       kDefaultBitrate);
}

void VEAClient::DestroyEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;
  encoder_.release()->Destroy();
}

void VEAClient::NotifyInitializeDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetInitialConfiguration();
  SetState(CS_INITIALIZED);
}

static size_t I420ByteSize(const gfx::Size& d) {
  CHECK((d.width() % 2 == 0) && (d.height() % 2 == 0));
  return d.width() * d.height() * 3 / 2;
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

  num_required_input_buffers_ = input_count;
  ASSERT_GT(num_required_input_buffers_, 0UL);

  input_buffer_size_ = I420ByteSize(input_coded_size_);
  CHECK_GT(input_buffer_size_, 0UL);

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
  EXPECT_GT(payload_size, 0UL);

  IdToSHM::iterator it = output_buffers_at_client_.find(bitstream_buffer_id);
  ASSERT_NE(it, output_buffers_at_client_.end());
  base::SharedMemory* shm = it->second;
  output_buffers_at_client_.erase(it);

  if (state_ == CS_FINISHED)
    return;

  encoded_stream_size_ += payload_size;

  h264_parser_.SetStream(static_cast<uint8*>(shm->memory()), payload_size);

  bool seen_idr_in_this_buffer = false;

  while (1) {
    content::H264NALU nalu;
    content::H264Parser::Result result;

    result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == content::H264Parser::kEOStream)
      break;

    ASSERT_EQ(result, content::H264Parser::kOk);

    switch (nalu.nal_unit_type) {
      case content::H264NALU::kIDRSlice:
        ASSERT_TRUE(seen_sps_);
        ASSERT_TRUE(seen_pps_);
        seen_idr_ = seen_idr_in_this_buffer = true;
        // Got keyframe, reset keyframe detection regardless of whether we
        // got a frame in time or not.
        keyframe_requested_at_ = kMaxFrameNum;
        // fallthrough
      case content::H264NALU::kNonIDRSlice:
        ASSERT_TRUE(seen_idr_);
        ++num_encoded_slices_;

        // Because the keyframe behavior requirements are loose, we give
        // the encoder more freedom here. It could either deliver a keyframe
        // immediately after we requested it, which could be for a frame number
        // before the one we requested it for (if the keyframe request
        // is asynchronous, i.e. not bound to any concrete frame, and because
        // the pipeline can be deeper that one frame), at that frame, or after.
        // So the only constraints we put here is that we get a keyframe not
        // earlier than we requested one (in time), and not later than
        // kMaxKeyframeDelay frames after the frame for which we requested
        // it comes back as encoded slice.
        EXPECT_LE(num_encoded_slices_,
                  keyframe_requested_at_ + kMaxKeyframeDelay);
        break;
      case content::H264NALU::kSPS:
        seen_sps_ = true;
        break;
      case content::H264NALU::kPPS:
        ASSERT_TRUE(seen_sps_);
        seen_pps_ = true;
        break;
      default:
        break;
    }

    if (num_encoded_slices_ == num_frames_in_stream_) {
      ASSERT_EQ(state_, CS_FINISHING);
      ChecksAtFinish();
      SetState(CS_FINISHED);
      break;
    }
  }

  EXPECT_EQ(key_frame, seen_idr_in_this_buffer);

  if (save_to_file_) {
    int size = base::checked_numeric_cast<int>(payload_size);
    EXPECT_EQ(file_util::AppendToFile(
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

    uint8* frame_data = const_cast<uint8*>(test_stream_.input_file.data() +
                                           pos_in_input_stream_);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapExternalYuvData(
            media::VideoFrame::I420,
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
    pos_in_input_stream_ += input_buffer_size_;

    bool force_keyframe = false;
    if (keyframe_period_ && next_input_id_ % keyframe_period_ == 0) {
      keyframe_requested_at_ = next_input_id_;
      force_keyframe = true;
    }
    encoder_->Encode(video_frame, force_keyframe);
    ++next_input_id_;
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
  CHECK(output_buffers_at_client_.insert(
      std::make_pair(bitstream_buffer.id(), shm)).second);
  encoder_->UseOutputBitstreamBuffer(bitstream_buffer);
}

void VEAClient::FlushEncoder() {
  ASSERT_EQ(state_, CS_ENCODING);
  SetState(CS_FINISHING);

  // Feed encoder with a set of black frames to flush it.
  for (unsigned int i = 0; i < num_required_input_buffers_; ++i) {
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(input_coded_size_);
    CHECK(inputs_at_client_.insert(next_input_id_).second);
    ++next_input_id_;
    encoder_->Encode(frame, false);
  }
}

void VEAClient::ChecksAtFinish() {
  if (force_bitrate_) {
    EXPECT_NEAR(encoded_stream_size_ * 8 * kDefaultFPS / num_frames_in_stream_,
                test_stream_.requested_bitrate,
                kBitrateTolerance * test_stream_.requested_bitrate);
  }
}

// Test parameters:
// - If true, save output to file.
// - Force keyframe every n frames.
// - Force bitrate; the actual required value is provided as a property
//   of the input stream, because it depends on stream type/resolution/etc.
class VideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<Tuple3<bool, int, bool> > {};

TEST_P(VideoEncodeAcceleratorTest, TestSimpleEncode) {
  const bool save_to_file = GetParam().a;
  const unsigned int keyframe_period = GetParam().b;
  bool force_bitrate = GetParam().c;

  TestStream test_stream(test_stream_data);
  ParseAndReadTestStreamData(test_stream_data, &test_stream);
  if (test_stream.requested_bitrate == 0)
    force_bitrate = false;

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
  settings.dcheck_state =
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;
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
