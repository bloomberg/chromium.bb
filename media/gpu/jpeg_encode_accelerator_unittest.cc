// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_jpeg_encode_accelerator_factory.h"
#include "media/gpu/test/video_accelerator_unittest_helpers.h"
#include "media/video/jpeg_encode_accelerator.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/jpeg_codec.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {
namespace {

// Default test image file.
const base::FilePath::CharType kDefaultYuvFilename[] =
    FILE_PATH_LITERAL("bali_640x360_P420.yuv:640x360");
// Whether to save encode results to files. Output files will be saved in the
// same directory as the input files, with the '.jpg' extension appended to
// their names. The encode result of generated images is written to the current
// folder using HxW_[black|white].jpg as output file name.
bool g_save_to_file = false;

const double kMeanDiffThreshold = 10.0;
const int kJpegDefaultQuality = 90;

// Environment to create test data for all test cases.
class JpegEncodeAcceleratorTestEnvironment;
JpegEncodeAcceleratorTestEnvironment* g_env;

struct TestImage {
  TestImage(std::vector<uint8_t> image_data,
            const gfx::Size& visible_size,
            const base::FilePath output_filename)
      : image_data(std::move(image_data)),
        visible_size(visible_size),
        output_filename(output_filename) {}

  // Test image data.
  std::vector<uint8_t> image_data;
  gfx::Size visible_size;

  // Output filename, only used when '--save_to_file' is specified.
  base::FilePath output_filename;
  size_t output_size = 0;
};

enum class ClientState {
  CREATED,
  INITIALIZED,
  ENCODE_PASS,
  ERROR,
};

class JpegEncodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  JpegEncodeAcceleratorTestEnvironment(
      const base::FilePath::CharType* yuv_filenames,
      const base::FilePath log_path,
      const int repeat)
      : repeat_(repeat), log_path_(log_path) {
    user_yuv_files_ = yuv_filenames ? yuv_filenames : kDefaultYuvFilename;
  }
  void SetUp() override;
  void TearDown() override;

  void LogToFile(const std::string& key, const std::string& value);

  // Read image from |filename| to |image_data|.
  std::unique_ptr<TestImage> ReadTestYuvImage(const base::FilePath& filename,
                                              const gfx::Size& image_size);

  // Returns a file path for a file in what name specified or media/test/data
  // directory.  If the original file path is existed, returns it first.
  base::FilePath GetOriginalOrTestDataFilePath(const std::string& name);

  // Parsed data from command line.
  std::vector<std::unique_ptr<TestImage>> image_data_user_;

  // Generated 2560x1920 white I420 image.
  std::unique_ptr<TestImage> image_data_2560x1920_white_;
  // Scarlet doesn't support 1080 width, it only suports 1088 width.
  // Generated 1280x720 white I420 image.
  std::unique_ptr<TestImage> image_data_1280x720_white_;
  // Generated 640x480 black I420 image.
  std::unique_ptr<TestImage> image_data_640x480_black_;
  // Generated 640x368 black I420 image.
  std::unique_ptr<TestImage> image_data_640x368_black_;
  // Generated 640x360 black I420 image.
  std::unique_ptr<TestImage> image_data_640x360_black_;

  // Number of times SimpleEncodeTest should repeat for an image.
  const size_t repeat_;

 private:
  // Create black or white test image with specified |size|.
  std::unique_ptr<TestImage> CreateTestYuvImage(const gfx::Size& image_size,
                                                bool is_black);

  const base::FilePath::CharType* user_yuv_files_;
  const base::FilePath log_path_;
  std::unique_ptr<base::File> log_file_;
};

void JpegEncodeAcceleratorTestEnvironment::SetUp() {
  // Since base::test::ScopedTaskEnvironment will call
  // TestTimeouts::action_max_timeout(), TestTimeouts::Initialize() needs to be
  // called in advance.
  TestTimeouts::Initialize();

  if (!log_path_.empty()) {
    log_file_.reset(new base::File(
        log_path_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
    LOG_ASSERT(log_file_->IsValid());
  }

  image_data_2560x1920_white_ =
      CreateTestYuvImage(gfx::Size(2560, 1920), false);
  image_data_1280x720_white_ = CreateTestYuvImage(gfx::Size(1280, 720), false);
  image_data_640x480_black_ = CreateTestYuvImage(gfx::Size(640, 480), true);
  image_data_640x368_black_ = CreateTestYuvImage(gfx::Size(640, 368), true);
  image_data_640x360_black_ = CreateTestYuvImage(gfx::Size(640, 360), true);

  // |user_yuv_files_| may include many files and use ';' as delimiter.
  std::vector<base::FilePath::StringType> files =
      base::SplitString(user_yuv_files_, base::FilePath::StringType(1, ';'),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& file : files) {
    std::vector<base::FilePath::StringType> filename_and_size =
        base::SplitString(file, base::FilePath::StringType(1, ':'),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(2u, filename_and_size.size());
    base::FilePath::StringType filename(filename_and_size[0]);

    std::vector<base::FilePath::StringType> image_resolution =
        base::SplitString(filename_and_size[1],
                          base::FilePath::StringType(1, 'x'),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(2u, image_resolution.size());
    int width = 0, height = 0;
    ASSERT_TRUE(base::StringToInt(image_resolution[0], &width));
    ASSERT_TRUE(base::StringToInt(image_resolution[1], &height));

    gfx::Size image_size(width, height);
    ASSERT_TRUE(!image_size.IsEmpty());

    base::FilePath input_file = GetOriginalOrTestDataFilePath(filename);
    auto image_data = ReadTestYuvImage(input_file, image_size);
    image_data_user_.push_back(std::move(image_data));
  }
}

void JpegEncodeAcceleratorTestEnvironment::TearDown() {
  log_file_.reset();
}

void JpegEncodeAcceleratorTestEnvironment::LogToFile(const std::string& key,
                                                     const std::string& value) {
  std::string s = base::StringPrintf("%s: %s\n", key.c_str(), value.c_str());
  LOG(INFO) << s;
  if (log_file_) {
    log_file_->WriteAtCurrentPos(s.data(), static_cast<int>(s.length()));
  }
}

std::unique_ptr<TestImage>
JpegEncodeAcceleratorTestEnvironment::CreateTestYuvImage(
    const gfx::Size& image_size,
    bool is_black) {
  const size_t num_pixels = image_size.width() * image_size.height();
  std::vector<uint8_t> image_data(num_pixels * 3 / 2);

  // Fill in Y values.
  std::fill(image_data.begin(), image_data.begin() + num_pixels,
            is_black ? 0 : 255);
  // Fill in U and V values.
  std::fill(image_data.begin() + num_pixels, image_data.end(), 128);

  base::FilePath output_filename(std::to_string(image_size.width()) + "x" +
                                 std::to_string(image_size.height()) +
                                 (is_black ? "_black.jpg" : "_white.jpg"));
  return std::make_unique<TestImage>(std::move(image_data), image_size,
                                     output_filename);
}

std::unique_ptr<TestImage>
JpegEncodeAcceleratorTestEnvironment::ReadTestYuvImage(
    const base::FilePath& input_file,
    const gfx::Size& image_size) {
  int64_t file_size = 0;
  LOG_ASSERT(GetFileSize(input_file, &file_size));
  std::vector<uint8_t> image_data(file_size);
  LOG_ASSERT(ReadFile(input_file, reinterpret_cast<char*>(image_data.data()),
                      file_size) == file_size);

  base::FilePath output_filename = input_file.AddExtension(".jpg");
  return std::make_unique<TestImage>(std::move(image_data), image_size,
                                     output_filename);
}

base::FilePath
JpegEncodeAcceleratorTestEnvironment::GetOriginalOrTestDataFilePath(
    const std::string& name) {
  base::FilePath file_path = base::FilePath(name);
  if (!PathExists(file_path)) {
    file_path = GetTestDataFilePath(name);
  }
  VLOG(3) << "Using file path " << file_path.value();
  return file_path;
}

class JpegClient : public JpegEncodeAccelerator::Client {
 public:
  JpegClient(const std::vector<TestImage*>& test_aligned_images,
             const std::vector<TestImage*>& test_images,
             media::test::ClientStateNotification<ClientState>* note);
  ~JpegClient() override;
  void CreateJpegEncoder();
  void DestroyJpegEncoder();
  void StartEncode(int32_t bitstream_buffer_id);

  // JpegEncodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t buffer_id, size_t encoded_picture_size) override;
  void NotifyError(int32_t buffer_id,
                   JpegEncodeAccelerator::Status status) override;

 private:
  // Get the related test image file.
  TestImage* GetTestImage(int32_t bitstream_buffer_id);
  void PrepareMemory(int32_t bitstream_buffer_id);
  void SetState(ClientState new_state);
  void SaveToFile(TestImage* test_image, size_t hw_size, size_t sw_size);
  bool CompareHardwareAndSoftwareResults(int width,
                                         int height,
                                         size_t hw_encoded_size,
                                         size_t sw_encoded_size);

  // Calculate mean absolute difference of hardware and software encode results
  // for verifying the similarity.
  double GetMeanAbsoluteDifference(uint8_t* hw_yuv_result,
                                   uint8_t* sw_yuv_result,
                                   size_t yuv_size);

  // Generate software encode result and populate it into |sw_out_shm_|.
  bool GetSoftwareEncodeResult(int width,
                               int height,
                               size_t* sw_encoded_size,
                               base::TimeDelta* sw_encode_time);

  // JpegClient doesn't own |test_aligned_images_|.
  // The resolutions of these images are all aligned. HW Accelerator must
  // support them.
  const std::vector<TestImage*>& test_aligned_images_;

  // JpegClient doesn't own |test_images_|.
  // The resolutions of these images may be unaligned.
  const std::vector<TestImage*>& test_images_;

  // A map that stores HW encoding start timestamp for each output buffer id.
  std::map<int, base::TimeTicks> buffer_id_to_start_time_;

  std::unique_ptr<JpegEncodeAccelerator> encoder_;
  ClientState state_;

  // Used to notify another thread about the state. JpegClient does not own
  // this.
  media::test::ClientStateNotification<ClientState>* note_;

  // Output buffer prepared for JpegEncodeAccelerator.
  std::unique_ptr<BitstreamBuffer> encoded_buffer_;

  // Mapped memory of input file.
  std::unique_ptr<base::SharedMemory> in_shm_;
  // Mapped memory of output buffer from hardware encoder.
  std::unique_ptr<base::SharedMemory> hw_out_shm_;
  // Mapped memory of output buffer from software encoder.
  std::unique_ptr<base::SharedMemory> sw_out_shm_;

  DISALLOW_COPY_AND_ASSIGN(JpegClient);
};

JpegClient::JpegClient(const std::vector<TestImage*>& test_aligned_images,
                       const std::vector<TestImage*>& test_images,
                       media::test::ClientStateNotification<ClientState>* note)
    : test_aligned_images_(test_aligned_images),
      test_images_(test_images),
      state_(ClientState::CREATED),
      note_(note) {}

JpegClient::~JpegClient() {}

void JpegClient::CreateJpegEncoder() {
  auto jea_factories =
      GpuJpegEncodeAcceleratorFactory::GetAcceleratorFactories();
  if (jea_factories.size() == 0) {
    LOG(ERROR) << "JpegEncodeAccelerator is not supported on this platform.";
    SetState(ClientState::ERROR);
    return;
  }

  for (const auto& create_jea_func : jea_factories) {
    encoder_ = create_jea_func.Run(base::ThreadTaskRunnerHandle::Get());
    if (encoder_)
      break;
  }

  if (!encoder_) {
    LOG(ERROR) << "Failed to create JpegEncodeAccelerator.";
    SetState(ClientState::ERROR);
    return;
  }

  JpegEncodeAccelerator::Status status = encoder_->Initialize(this);
  if (status != JpegEncodeAccelerator::ENCODE_OK) {
    LOG(ERROR) << "JpegEncodeAccelerator::Initialize() failed: " << status;
    SetState(ClientState::ERROR);
    return;
  }
  SetState(ClientState::INITIALIZED);
}

void JpegClient::DestroyJpegEncoder() {
  encoder_.reset();
}

void JpegClient::VideoFrameReady(int32_t buffer_id, size_t hw_encoded_size) {
  base::TimeTicks hw_encode_end = base::TimeTicks::Now();
  base::TimeDelta elapsed_hw =
      hw_encode_end - buffer_id_to_start_time_[buffer_id];

  TestImage* test_image;
  if (buffer_id < static_cast<int32_t>(test_aligned_images_.size())) {
    test_image = test_aligned_images_[buffer_id];
  } else {
    test_image = test_images_[buffer_id - test_aligned_images_.size()];
  }

  size_t sw_encoded_size = 0;
  base::TimeDelta elapsed_sw;
  LOG_ASSERT(GetSoftwareEncodeResult(test_image->visible_size.width(),
                                     test_image->visible_size.height(),
                                     &sw_encoded_size, &elapsed_sw));

  g_env->LogToFile("hw_encode_time",
                   base::NumberToString(elapsed_hw.InMicroseconds()));
  g_env->LogToFile("sw_encode_time",
                   base::NumberToString(elapsed_sw.InMicroseconds()));

  if (g_save_to_file) {
    SaveToFile(test_image, hw_encoded_size, sw_encoded_size);
  }

  if (!CompareHardwareAndSoftwareResults(test_image->visible_size.width(),
                                         test_image->visible_size.height(),
                                         hw_encoded_size, sw_encoded_size)) {
    SetState(ClientState::ERROR);
  } else {
    SetState(ClientState::ENCODE_PASS);
  }

  encoded_buffer_.reset(nullptr);
}

bool JpegClient::GetSoftwareEncodeResult(int width,
                                         int height,
                                         size_t* sw_encoded_size,
                                         base::TimeDelta* sw_encode_time) {
  base::TimeTicks sw_encode_start = base::TimeTicks::Now();
  int y_stride = width;
  int u_stride = width / 2;
  int v_stride = u_stride;
  uint8_t* yuv_src = static_cast<uint8_t*>(in_shm_->memory());
  const int kBytesPerPixel = 4;
  std::vector<uint8_t> rgba_buffer(width * height * kBytesPerPixel);
  std::vector<uint8_t> encoded;
  libyuv::I420ToABGR(yuv_src, y_stride, yuv_src + y_stride * height, u_stride,
                     yuv_src + y_stride * height + u_stride * height / 2,
                     v_stride, rgba_buffer.data(), width * kBytesPerPixel,
                     width, height);

  SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType);
  SkPixmap src(info, &rgba_buffer[0], width * kBytesPerPixel);
  if (!gfx::JPEGCodec::Encode(src, kJpegDefaultQuality, &encoded)) {
    return false;
  }

  memcpy(sw_out_shm_->memory(), encoded.data(), encoded.size());
  *sw_encoded_size = encoded.size();
  *sw_encode_time = base::TimeTicks::Now() - sw_encode_start;
  return true;
}

bool JpegClient::CompareHardwareAndSoftwareResults(int width,
                                                   int height,
                                                   size_t hw_encoded_size,
                                                   size_t sw_encoded_size) {
  size_t yuv_size = width * height * 3 / 2;
  uint8_t* hw_yuv_result = new uint8_t[yuv_size];
  int y_stride = width;
  int u_stride = width / 2;
  int v_stride = u_stride;
  if (libyuv::ConvertToI420(
          static_cast<const uint8_t*>(hw_out_shm_->memory()), hw_encoded_size,
          hw_yuv_result, y_stride, hw_yuv_result + y_stride * height, u_stride,
          hw_yuv_result + y_stride * height + u_stride * height / 2, v_stride,
          0, 0, width, height, width, height, libyuv::kRotate0,
          libyuv::FOURCC_MJPG)) {
    LOG(ERROR) << "Convert HW encoded result to YUV failed";
  }

  uint8_t* sw_yuv_result = new uint8_t[yuv_size];
  if (libyuv::ConvertToI420(
          static_cast<const uint8_t*>(sw_out_shm_->memory()), sw_encoded_size,
          sw_yuv_result, y_stride, sw_yuv_result + y_stride * height, u_stride,
          sw_yuv_result + y_stride * height + u_stride * height / 2, v_stride,
          0, 0, width, height, width, height, libyuv::kRotate0,
          libyuv::FOURCC_MJPG)) {
    LOG(ERROR) << "Convert SW encoded result to YUV failed";
  }

  double difference =
      GetMeanAbsoluteDifference(hw_yuv_result, sw_yuv_result, yuv_size);
  delete[] hw_yuv_result;
  delete[] sw_yuv_result;

  if (difference > kMeanDiffThreshold) {
    LOG(ERROR) << "HW and SW encode results are not similar enough. diff = "
               << difference;
    return false;
  } else {
    return true;
  }
}

double JpegClient::GetMeanAbsoluteDifference(uint8_t* hw_yuv_result,
                                             uint8_t* sw_yuv_result,
                                             size_t yuv_size) {
  double total_difference = 0;
  for (size_t i = 0; i < yuv_size; i++)
    total_difference += std::abs(hw_yuv_result[i] - sw_yuv_result[i]);
  return total_difference / yuv_size;
}

void JpegClient::NotifyError(int32_t buffer_id,
                             JpegEncodeAccelerator::Status status) {
  LOG(ERROR) << "Notifying of error " << status << " for output buffer id "
             << buffer_id;
  SetState(ClientState::ERROR);
  encoded_buffer_.reset(nullptr);
}

TestImage* JpegClient::GetTestImage(int32_t bitstream_buffer_id) {
  DCHECK_LT(static_cast<size_t>(bitstream_buffer_id),
            test_aligned_images_.size() + test_images_.size());
  TestImage* image_file;
  if (bitstream_buffer_id < static_cast<int32_t>(test_aligned_images_.size())) {
    image_file = test_aligned_images_[bitstream_buffer_id];
  } else {
    image_file =
        test_images_[bitstream_buffer_id - test_aligned_images_.size()];
  }

  return image_file;
}

void JpegClient::PrepareMemory(int32_t bitstream_buffer_id) {
  TestImage* test_image = GetTestImage(bitstream_buffer_id);

  size_t input_size = test_image->image_data.size();
  if (!in_shm_.get() || input_size > in_shm_->mapped_size()) {
    in_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(in_shm_->CreateAndMapAnonymous(input_size));
  }
  memcpy(in_shm_->memory(), test_image->image_data.data(), input_size);

  if (!hw_out_shm_.get() ||
      test_image->output_size > hw_out_shm_->mapped_size()) {
    hw_out_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(hw_out_shm_->CreateAndMapAnonymous(test_image->output_size));
  }
  memset(hw_out_shm_->memory(), 0, test_image->output_size);

  if (!sw_out_shm_.get() ||
      test_image->output_size > sw_out_shm_->mapped_size()) {
    sw_out_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(sw_out_shm_->CreateAndMapAnonymous(test_image->output_size));
  }
  memset(sw_out_shm_->memory(), 0, test_image->output_size);
}

void JpegClient::SetState(ClientState new_state) {
  DVLOG(2) << "Changing state "
           << static_cast<std::underlying_type<ClientState>::type>(state_)
           << "->"
           << static_cast<std::underlying_type<ClientState>::type>(new_state);
  note_->Notify(new_state);
  state_ = new_state;
}

void JpegClient::SaveToFile(TestImage* test_image,
                            size_t hw_size,
                            size_t sw_size) {
  DCHECK_NE(nullptr, test_image);

  base::FilePath out_filename_hw = test_image->output_filename;
  LOG(INFO) << "Writing HW encode results to "
            << out_filename_hw.MaybeAsASCII();
  ASSERT_EQ(
      static_cast<int>(hw_size),
      base::WriteFile(out_filename_hw,
                      static_cast<char*>(hw_out_shm_->memory()), hw_size));

  base::FilePath out_filename_sw = out_filename_hw.InsertBeforeExtension("_sw");
  LOG(INFO) << "Writing SW encode results to "
            << out_filename_sw.MaybeAsASCII();
  ASSERT_EQ(
      static_cast<int>(sw_size),
      base::WriteFile(out_filename_sw,
                      static_cast<char*>(sw_out_shm_->memory()), sw_size));
}

void JpegClient::StartEncode(int32_t bitstream_buffer_id) {
  TestImage* test_image = GetTestImage(bitstream_buffer_id);

  test_image->output_size =
      encoder_->GetMaxCodedBufferSize(test_image->visible_size);
  PrepareMemory(bitstream_buffer_id);

  base::SharedMemoryHandle dup_handle;
  dup_handle = base::SharedMemory::DuplicateHandle(hw_out_shm_->handle());
  encoded_buffer_ = std::make_unique<BitstreamBuffer>(
      bitstream_buffer_id, dup_handle, test_image->output_size);
  scoped_refptr<VideoFrame> input_frame_ = VideoFrame::WrapExternalSharedMemory(
      PIXEL_FORMAT_I420, test_image->visible_size,
      gfx::Rect(test_image->visible_size), test_image->visible_size,
      static_cast<uint8_t*>(in_shm_->memory()), test_image->image_data.size(),
      in_shm_->handle(), 0, base::TimeDelta());

  LOG_ASSERT(input_frame_.get());

  buffer_id_to_start_time_[bitstream_buffer_id] = base::TimeTicks::Now();
  encoder_->Encode(input_frame_, kJpegDefaultQuality, nullptr,
                   *encoded_buffer_);
}

class JpegEncodeAcceleratorTest : public ::testing::Test {
 protected:
  JpegEncodeAcceleratorTest() {}

  void TestEncode(size_t num_concurrent_encoders);

  // This is needed to allow the usage of methods in post_task.h in
  // JpegEncodeAccelerator implementations.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // The elements of |test_aligned_images_| and |test_images_| are
  // owned by JpegEncodeAcceleratorTestEnvironment.
  std::vector<TestImage*> test_aligned_images_;
  std::vector<TestImage*> test_images_;

 protected:
  DISALLOW_COPY_AND_ASSIGN(JpegEncodeAcceleratorTest);
};

void JpegEncodeAcceleratorTest::TestEncode(size_t num_concurrent_encoders) {
  base::Thread encoder_thread("EncoderThread");
  ASSERT_TRUE(encoder_thread.Start());

  std::vector<
      std::unique_ptr<media::test::ClientStateNotification<ClientState>>>
      notes;
  std::vector<std::unique_ptr<JpegClient>> clients;

  for (size_t i = 0; i < num_concurrent_encoders; i++) {
    notes.push_back(
        std::make_unique<media::test::ClientStateNotification<ClientState>>());
    clients.push_back(std::make_unique<JpegClient>(
        test_aligned_images_, test_images_, notes.back().get()));
    encoder_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&JpegClient::CreateJpegEncoder,
                                  base::Unretained(clients.back().get())));
    ASSERT_EQ(notes[i]->Wait(), ClientState::INITIALIZED);
  }

  for (size_t index = 0; index < test_aligned_images_.size(); index++) {
    VLOG(3) << index
            << ",width:" << test_aligned_images_[index]->visible_size.width();
    VLOG(3) << index
            << ",height:" << test_aligned_images_[index]->visible_size.height();
    for (size_t i = 0; i < num_concurrent_encoders; i++) {
      encoder_thread.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&JpegClient::StartEncode,
                                    base::Unretained(clients[i].get()), index));
    }
    for (size_t i = 0; i < num_concurrent_encoders; i++) {
      ASSERT_EQ(notes[i]->Wait(), ClientState::ENCODE_PASS);
    }
  }

  for (size_t index = 0; index < test_images_.size(); index++) {
    int buffer_id = index + test_aligned_images_.size();
    VLOG(3) << buffer_id
            << ",width:" << test_images_[index]->visible_size.width();
    VLOG(3) << buffer_id
            << ",height:" << test_images_[index]->visible_size.height();
    for (size_t i = 0; i < num_concurrent_encoders; i++) {
      encoder_thread.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&JpegClient::StartEncode,
                         base::Unretained(clients[i].get()), buffer_id));
    }

    for (size_t i = 0; i < num_concurrent_encoders; i++) {
// For unaligned images, V4L2 may not be able to encode them.
#if BUILDFLAG(USE_V4L2_CODEC) && defined(ARCH_CPU_ARM_FAMILY)
      ClientState status = notes[i]->Wait();
      ASSERT_TRUE(status == ClientState::ENCODE_PASS ||
                  status == ClientState::ERROR);
#else
      ASSERT_EQ(notes[i]->Wait(), ClientState::ENCODE_PASS);
#endif
    }
  }

  for (size_t i = 0; i < num_concurrent_encoders; i++) {
    encoder_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&JpegClient::DestroyJpegEncoder,
                                  base::Unretained(clients[i].get())));
  }
  encoder_thread.Stop();
  VLOG(1) << "Exit TestEncode";
}

TEST_F(JpegEncodeAcceleratorTest, SimpleEncode) {
  for (size_t i = 0; i < g_env->repeat_; i++) {
    for (auto& image : g_env->image_data_user_) {
      test_images_.push_back(image.get());
    }
  }
  TestEncode(1);
}

TEST_F(JpegEncodeAcceleratorTest, MultipleEncoders) {
  for (auto& image : g_env->image_data_user_) {
    test_images_.push_back(image.get());
  }
  TestEncode(3);
}

TEST_F(JpegEncodeAcceleratorTest, ResolutionChange) {
  test_images_.push_back(g_env->image_data_640x368_black_.get());
  test_images_.push_back(g_env->image_data_640x360_black_.get());
  test_aligned_images_.push_back(g_env->image_data_1280x720_white_.get());
  TestEncode(1);
}

TEST_F(JpegEncodeAcceleratorTest, AlignedSizes) {
  test_aligned_images_.push_back(g_env->image_data_2560x1920_white_.get());
  test_aligned_images_.push_back(g_env->image_data_1280x720_white_.get());
  test_aligned_images_.push_back(g_env->image_data_640x480_black_.get());
  TestEncode(1);
}

TEST_F(JpegEncodeAcceleratorTest, CodedSizeAlignment) {
  test_images_.push_back(g_env->image_data_640x360_black_.get());
  TestEncode(1);
}

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);
  mojo::core::Init();
  base::ShadowingAtExitManager at_exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  const base::FilePath::CharType* yuv_filenames = nullptr;
  base::FilePath log_path;
  size_t repeat = 1;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    // yuv_filenames can include one or many files and use ';' as delimiter.
    // For each file, it should follow the format "[filename]:[width]x[height]".
    // For example, "lake.yuv:4160x3120".
    if (it->first == "yuv_filenames") {
      yuv_filenames = it->second.c_str();
      continue;
    }
    if (it->first == "output_log") {
      log_path = base::FilePath(
          base::FilePath::StringType(it->second.begin(), it->second.end()));
      continue;
    }
    if (it->first == "repeat") {
      if (!base::StringToSizeT(it->second, &repeat)) {
        LOG(INFO) << "Can't parse parameter |repeat|: " << it->second;
        repeat = 1;
      }
      continue;
    }
    if (it->first == "save_to_file") {
      media::g_save_to_file = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "h" || it->first == "help")
      continue;
    LOG(ERROR) << "Unexpected switch: " << it->first << ":" << it->second;
    return -EINVAL;
  }
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  media::g_env = reinterpret_cast<media::JpegEncodeAcceleratorTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new media::JpegEncodeAcceleratorTestEnvironment(yuv_filenames,
                                                          log_path, repeat)));

  return RUN_ALL_TESTS();
}
