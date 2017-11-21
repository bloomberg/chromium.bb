// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;
using ::testing::WithArgs;

namespace media {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

constexpr VideoCodecProfile kCodecProfiles[] = {H264PROFILE_MIN, VP8PROFILE_MIN,
                                                VP9PROFILE_MIN};
constexpr int kBitstreamId = 123;
constexpr size_t kInputSize = 256;

}  // namespace

class MockAcceleratedVideoDecoder : public AcceleratedVideoDecoder {
 public:
  MockAcceleratedVideoDecoder() = default;
  ~MockAcceleratedVideoDecoder() override = default;

  MOCK_METHOD2(SetStream, void(const uint8_t* ptr, size_t size));
  MOCK_METHOD0(Flush, bool());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD0(Decode, DecodeResult());
  MOCK_CONST_METHOD0(GetPicSize, gfx::Size());
  MOCK_CONST_METHOD0(GetRequiredNumOfPictures, size_t());
};

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper() = default;
  MOCK_METHOD4(
      CreateSurfaces,
      bool(unsigned int, const gfx::Size&, size_t, std::vector<VASurfaceID>*));
  MOCK_METHOD0(DestroySurfaces, void());

 private:
  ~MockVaapiWrapper() override = default;
};

class MockVaapiPicture : public VaapiPicture {
 public:
  MockVaapiPicture(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
                   const MakeGLContextCurrentCallback& make_context_current_cb,
                   const BindGLImageCallback& bind_image_cb,
                   int32_t picture_buffer_id,
                   const gfx::Size& size,
                   uint32_t texture_id,
                   uint32_t client_texture_id)
      : VaapiPicture(vaapi_wrapper,
                     make_context_current_cb,
                     bind_image_cb,
                     picture_buffer_id,
                     size,
                     texture_id,
                     client_texture_id) {}
  ~MockVaapiPicture() override = default;

  // VaapiPicture implementation.
  bool Allocate(gfx::BufferFormat format) override { return true; }
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) override {
    return true;
  }
  bool DownloadFromSurface(
      const scoped_refptr<VASurface>& va_surface) override {
    return true;
  }
  bool AllowOverlay() const override { return false; }
};

class VaapiVideoDecodeAcceleratorTest : public TestWithParam<VideoCodecProfile>,
                                        public VideoDecodeAccelerator::Client {
 public:
  VaapiVideoDecodeAcceleratorTest()
      : vda_(base::Bind([] { return true; }),
             base::Bind([](uint32_t client_texture_id,
                           uint32_t texture_target,
                           const scoped_refptr<gl::GLImage>& image,
                           bool can_bind_to_sampler) { return true; })),
        decoder_thread_("VaapiVideoDecodeAcceleratorTestThread"),
        mock_decoder_(new MockAcceleratedVideoDecoder),
        mock_vaapi_wrapper_(new MockVaapiWrapper()),
        weak_ptr_factory_(this) {
    decoder_thread_.Start();

    // Don't want to go through a vda_->Initialize() because it binds too many
    // items of the environment. Instead, just start the decoder thread.
    vda_.decoder_thread_task_runner_ = decoder_thread_.task_runner();

    // Plug in our |mock_decoder_| and ourselves as the |client_|.
    vda_.decoder_.reset(mock_decoder_);
    vda_.client_ = weak_ptr_factory_.GetWeakPtr();
    vda_.vaapi_wrapper_ = mock_vaapi_wrapper_;

    vda_.create_vaapi_picture_callback_ =
        base::Bind(&VaapiVideoDecodeAcceleratorTest::CreateVaapiPicture,
                   base::Unretained(this));

    vda_.state_ = VaapiVideoDecodeAccelerator::kIdle;
  }
  ~VaapiVideoDecodeAcceleratorTest() {}

  void SetUp() override {
    in_shm_.reset(new base::SharedMemory);
    ASSERT_TRUE(in_shm_->CreateAndMapAnonymous(kInputSize));
  }

  void SetVdaStateToUnitialized() {
    vda_.state_ = VaapiVideoDecodeAccelerator::kUninitialized;
  }

  void QueueInputBuffer(const BitstreamBuffer& bitstream_buffer) {
    vda_.QueueInputBuffer(bitstream_buffer);
  }

  void AssignPictureBuffers(const std::vector<PictureBuffer>& picture_buffers) {
    vda_.AssignPictureBuffers(picture_buffers);
  }

  // Reset epilogue, needed to get |vda_| worker thread out of its Wait().
  void ResetSequence() {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*mock_decoder_, Reset());
    EXPECT_CALL(*this, NotifyResetDone()).WillOnce(RunClosure(quit_closure));
    vda_.Reset();
    run_loop.Run();
  }

  // TODO(mcasas): Use a Mock VaapiPicture factory, https://crbug.com/784507.
  MOCK_METHOD2(MockCreateVaapiPicture, void(VaapiWrapper*, const gfx::Size&));
  std::unique_ptr<VaapiPicture> CreateVaapiPicture(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      uint32_t texture_id,
      uint32_t client_texture_id) {
    MockCreateVaapiPicture(vaapi_wrapper.get(), size);
    return base::MakeUnique<MockVaapiPicture>(
        vaapi_wrapper, make_context_current_cb, bind_image_cb,
        picture_buffer_id, size, texture_id, client_texture_id);
  }

  // VideoDecodeAccelerator::Client methods.
  MOCK_METHOD1(NotifyInitializationComplete, void(bool));
  MOCK_METHOD5(
      ProvidePictureBuffers,
      void(uint32_t, VideoPixelFormat, uint32_t, const gfx::Size&, uint32_t));
  MOCK_METHOD1(DismissPictureBuffer, void(int32_t));
  MOCK_METHOD1(PictureReady, void(const Picture&));
  MOCK_METHOD1(NotifyEndOfBitstreamBuffer, void(int32_t));
  MOCK_METHOD0(NotifyFlushDone, void());
  MOCK_METHOD0(NotifyResetDone, void());
  MOCK_METHOD1(NotifyError, void(VideoDecodeAccelerator::Error));

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // The class under test and a worker thread for it.
  VaapiVideoDecodeAccelerator vda_;
  base::Thread decoder_thread_;

  // Ownership passed to |vda_|, but we retain a pointer to it for MOCK checks.
  MockAcceleratedVideoDecoder* mock_decoder_;

  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;

  std::unique_ptr<base::SharedMemory> in_shm_;

 private:
  base::WeakPtrFactory<VaapiVideoDecodeAcceleratorTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoDecodeAcceleratorTest);
};

// This test checks that QueueInputBuffer() fails when state is kUnitialized.
TEST_P(VaapiVideoDecodeAcceleratorTest, QueueInputBufferAndError) {
  SetVdaStateToUnitialized();

  base::SharedMemoryHandle handle;
  handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(kBitstreamId, handle, kInputSize);

  EXPECT_CALL(*this,
              NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE));
  QueueInputBuffer(bitstream_buffer);
}

// Verifies that Decode() returning kDecodeError ends up pinging NotifyError().
TEST_P(VaapiVideoDecodeAcceleratorTest, QueueInputBufferAndDecodeError) {
  base::SharedMemoryHandle handle;
  handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(kBitstreamId, handle, kInputSize);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*mock_decoder_, SetStream(_, kInputSize));
  EXPECT_CALL(*mock_decoder_, Decode())
      .WillOnce(Return(AcceleratedVideoDecoder::kDecodeError));
  EXPECT_CALL(*this, NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE))
      .WillOnce(RunClosure(quit_closure));

  QueueInputBuffer(bitstream_buffer);
  run_loop.Run();
}

// Tests usual startup sequence: a BitstreamBuffer is enqueued for decode,
// |vda_| asks for PictureBuffers, that we provide, and then the same Decode()
// is tried again.
TEST_P(VaapiVideoDecodeAcceleratorTest,
       QueueInputBufferAndAssignPictureBuffersAndDecode) {
  // Try and QueueInputBuffer(), |vda_| will ping us to ProvidePictureBuffers().
  const uint32_t kNumPictures = 2;
  const gfx::Size kPictureSize(64, 48);
  {
    base::SharedMemoryHandle handle;
    handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
    BitstreamBuffer bitstream_buffer(kBitstreamId, handle, kInputSize);

    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*mock_decoder_, SetStream(_, kInputSize));
    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kAllocateNewSurfaces));

    EXPECT_CALL(*mock_decoder_, GetRequiredNumOfPictures())
        .WillOnce(Return(kNumPictures));
    EXPECT_CALL(*mock_decoder_, GetPicSize()).WillOnce(Return(kPictureSize));
    EXPECT_CALL(*mock_vaapi_wrapper_, DestroySurfaces());

    EXPECT_CALL(*this,
                ProvidePictureBuffers(kNumPictures, _, 1, kPictureSize, _))
        .WillOnce(RunClosure(quit_closure));

    QueueInputBuffer(bitstream_buffer);
    run_loop.Run();
  }
  // AssignPictureBuffers() accordingly and expect another go at Decode().
  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();

    const std::vector<PictureBuffer> kPictureBuffers(
        {{2, kPictureSize}, {3, kPictureSize}});
    EXPECT_EQ(kPictureBuffers.size(), kNumPictures);

    EXPECT_CALL(*mock_vaapi_wrapper_,
                CreateSurfaces(_, kPictureSize, kNumPictures, _))
        .WillOnce(DoAll(
            WithArgs<3>(Invoke([](std::vector<VASurfaceID>* va_surface_ids) {
              va_surface_ids->resize(kNumPictures);
            })),
            Return(true)));
    EXPECT_CALL(*this,
                MockCreateVaapiPicture(mock_vaapi_wrapper_.get(), kPictureSize))
        .Times(2);

    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kRanOutOfStreamData));
    EXPECT_CALL(*this, NotifyEndOfBitstreamBuffer(kBitstreamId))
        .WillOnce(RunClosure(quit_closure));

    AssignPictureBuffers(kPictureBuffers);
    run_loop.Run();
  }

  ResetSequence();
}

// Verifies that Decode() replying kRanOutOfStreamData (to signal it's finished)
// rolls to a NotifyEndOfBitstreamBuffer().
TEST_P(VaapiVideoDecodeAcceleratorTest, QueueInputBufferAndDecodeFinished) {
  base::SharedMemoryHandle handle;
  handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(kBitstreamId, handle, kInputSize);

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*mock_decoder_, SetStream(_, kInputSize));
    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kRanOutOfStreamData));
    EXPECT_CALL(*this, NotifyEndOfBitstreamBuffer(kBitstreamId))
        .WillOnce(RunClosure(quit_closure));

    QueueInputBuffer(bitstream_buffer);
    run_loop.Run();
  }

  ResetSequence();
}

INSTANTIATE_TEST_CASE_P(/* No prefix. */,
                        VaapiVideoDecodeAcceleratorTest,
                        ValuesIn(kCodecProfiles));

}  // namespace media
