// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

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
        weak_ptr_factory_(this) {
    decoder_thread_.Start();

    // Don't want to go through a vda_->Initialize() because it binds too many
    // items of the environment. Instead, just start the decoder thread.
    vda_.decoder_thread_task_runner_ = decoder_thread_.task_runner();

    // Plug in our |mock_decoder_| and ourselves as the |client_|.
    vda_.decoder_.reset(mock_decoder_);
    vda_.client_ = weak_ptr_factory_.GetWeakPtr();

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

  void Reset() { vda_.Reset(); }

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

  EXPECT_CALL(*this, NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE))
      .Times(1);
  QueueInputBuffer(bitstream_buffer);
}

// Verifies that Decode() returning kDecodeError ends up pinging NotifyError().
TEST_P(VaapiVideoDecodeAcceleratorTest, QueueInputBufferAndDecodeError) {
  base::SharedMemoryHandle handle;
  handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(kBitstreamId, handle, kInputSize);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*mock_decoder_, SetStream(_, kInputSize)).Times(1);
  EXPECT_CALL(*mock_decoder_, Decode())
      .Times(1)
      .WillOnce(Return(AcceleratedVideoDecoder::kDecodeError));
  EXPECT_CALL(*this, NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  QueueInputBuffer(bitstream_buffer);
  run_loop.Run();
}

// Verifies that Decode() returning kAllocateNewSurfaces is followed up OK.
TEST_P(VaapiVideoDecodeAcceleratorTest,
       QueueInputBufferAndDecodeNeedsNewSurfaces) {
  base::SharedMemoryHandle handle;
  handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(kBitstreamId, handle, kInputSize);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*mock_decoder_, SetStream(_, kInputSize)).Times(1);
  EXPECT_CALL(*mock_decoder_, Decode())
      .Times(1)
      .WillOnce(Return(AcceleratedVideoDecoder::kAllocateNewSurfaces));

  EXPECT_CALL(*mock_decoder_, GetRequiredNumOfPictures()).Times(1);
  EXPECT_CALL(*mock_decoder_, GetPicSize())
      .Times(1)
      .WillOnce(DoAll(RunClosure(quit_closure), Return(gfx::Size(64, 48))));

  QueueInputBuffer(bitstream_buffer);
  run_loop.Run();
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
    EXPECT_CALL(*mock_decoder_, SetStream(_, kInputSize)).Times(1);
    EXPECT_CALL(*mock_decoder_, Decode())
        .Times(1)
        .WillOnce(Return(AcceleratedVideoDecoder::kRanOutOfStreamData));
    EXPECT_CALL(*this, NotifyEndOfBitstreamBuffer(kBitstreamId))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    QueueInputBuffer(bitstream_buffer);
    run_loop.Run();
  }
  // This epilogue is needed to get |vda_| worker thread out of its Wait().
  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*mock_decoder_, Reset()).Times(1);
    EXPECT_CALL(*this, NotifyResetDone())
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    Reset();
    run_loop.Run();
  }
}

INSTANTIATE_TEST_CASE_P(/* No prefix. */,
                        VaapiVideoDecodeAcceleratorTest,
                        ValuesIn(kCodecProfiles));

}  // namespace media
