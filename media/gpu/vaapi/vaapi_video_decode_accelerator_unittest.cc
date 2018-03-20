// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi/vaapi_picture_factory.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
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

  MOCK_METHOD3(SetStream, void(int32_t id, const uint8_t* ptr, size_t size));
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
                   uint32_t client_texture_id,
                   uint32_t texture_target)
      : VaapiPicture(vaapi_wrapper,
                     make_context_current_cb,
                     bind_image_cb,
                     picture_buffer_id,
                     size,
                     texture_id,
                     client_texture_id,
                     texture_target) {}
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

class MockVaapiPictureFactory : public VaapiPictureFactory {
 public:
  MockVaapiPictureFactory() = default;
  ~MockVaapiPictureFactory() override = default;

  MOCK_METHOD2(MockCreateVaapiPicture, void(VaapiWrapper*, const gfx::Size&));
  std::unique_ptr<VaapiPicture> Create(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      uint32_t texture_id,
      uint32_t client_texture_id,
      uint32_t texture_target) override {
    MockCreateVaapiPicture(vaapi_wrapper.get(), size);
    return std::make_unique<MockVaapiPicture>(
        vaapi_wrapper, make_context_current_cb, bind_image_cb,
        picture_buffer_id, size, texture_id, client_texture_id, texture_target);
  }
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
        mock_vaapi_picture_factory_(new MockVaapiPictureFactory()),
        mock_vaapi_wrapper_(new MockVaapiWrapper()),
        weak_ptr_factory_(this) {
    decoder_thread_.Start();

    // Don't want to go through a vda_->Initialize() because it binds too many
    // items of the environment. Instead, just start the decoder thread.
    vda_.decoder_thread_task_runner_ = decoder_thread_.task_runner();

    // Plug in all the mocks and ourselves as the |client_|.
    vda_.decoder_.reset(mock_decoder_);
    vda_.client_ = weak_ptr_factory_.GetWeakPtr();
    vda_.vaapi_wrapper_ = mock_vaapi_wrapper_;
    vda_.vaapi_picture_factory_.reset(mock_vaapi_picture_factory_);

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
  MockVaapiPictureFactory* mock_vaapi_picture_factory_;

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
  EXPECT_CALL(*mock_decoder_, SetStream(_, _, kInputSize));
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
    EXPECT_CALL(*mock_decoder_, SetStream(_, _, kInputSize));
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

    const uint32_t tex_target =
        mock_vaapi_picture_factory_->GetGLTextureTarget();

    // These client and service texture ids are arbitrarily chosen.
    const std::vector<PictureBuffer> kPictureBuffers(
        {{2, kPictureSize, PictureBuffer::TextureIds{0},
          PictureBuffer::TextureIds{1}, tex_target, PIXEL_FORMAT_XRGB},
         {3, kPictureSize, PictureBuffer::TextureIds{2},
          PictureBuffer::TextureIds{3}, tex_target, PIXEL_FORMAT_XRGB}});
    EXPECT_EQ(kPictureBuffers.size(), kNumPictures);

    EXPECT_CALL(*mock_vaapi_wrapper_,
                CreateSurfaces(_, kPictureSize, kNumPictures, _))
        .WillOnce(DoAll(
            WithArgs<3>(Invoke([](std::vector<VASurfaceID>* va_surface_ids) {
              va_surface_ids->resize(kNumPictures);
            })),
            Return(true)));
    EXPECT_CALL(*mock_vaapi_picture_factory_,
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
    EXPECT_CALL(*mock_decoder_, SetStream(_, _, kInputSize));
    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kRanOutOfStreamData));
    EXPECT_CALL(*this, NotifyEndOfBitstreamBuffer(kBitstreamId))
        .WillOnce(RunClosure(quit_closure));

    QueueInputBuffer(bitstream_buffer);
    run_loop.Run();
  }

  ResetSequence();
}

// Verify that it is possible to select DRM(egl) and TFP(glx) at runtime.
TEST_P(VaapiVideoDecodeAcceleratorTest, SupportedPlatforms) {
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationNone,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationNone));
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationDrm,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationEGLGLES2));

#if defined(USE_X11)
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationX11,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationDesktopGL));
#endif
}

INSTANTIATE_TEST_CASE_P(/* No prefix. */,
                        VaapiVideoDecodeAcceleratorTest,
                        ValuesIn(kCodecProfiles));

}  // namespace media
