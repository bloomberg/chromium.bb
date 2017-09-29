// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "media/base/decryptor.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/mojo/clients/mojo_decryptor.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/services/mojo_decryptor_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::InSequence;
using testing::IsNull;
using testing::NotNull;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace media {

class MojoDecryptorTest : public ::testing::Test {
 public:
  MojoDecryptorTest() {
    decryptor_.reset(new StrictMock<MockDecryptor>());

    mojom::DecryptorPtr remote_decryptor;
    mojo_decryptor_service_.reset(new MojoDecryptorService(
        decryptor_.get(), mojo::MakeRequest(&remote_decryptor),
        base::Bind(&MojoDecryptorTest::OnConnectionClosed,
                   base::Unretained(this))));

    mojo_decryptor_.reset(new MojoDecryptor(std::move(remote_decryptor)));
  }

  ~MojoDecryptorTest() override {}

  void DestroyClient() {
    EXPECT_CALL(*this, OnConnectionClosed());
    mojo_decryptor_.reset();
  }

  void DestroyService() {
    // MojoDecryptor has no way to notify callers that the connection is closed.
    // TODO(jrummell): Determine if notification is needed.
    mojo_decryptor_service_.reset();
  }

  void ReturnSimpleVideoFrame(const scoped_refptr<DecoderBuffer>& encrypted,
                              const Decryptor::VideoDecodeCB& video_decode_cb) {
    // We don't care about the encrypted data, just create a simple VideoFrame.
    scoped_refptr<VideoFrame> frame(
        MojoSharedBufferVideoFrame::CreateDefaultI420(
            gfx::Size(100, 100), base::TimeDelta::FromSeconds(100)));
    frame->AddDestructionObserver(base::Bind(
        &MojoDecryptorTest::OnFrameDestroyed, base::Unretained(this)));

    // Currently freeing buffers only works for MojoSharedMemory, so make
    // sure |frame| is of that type.
    EXPECT_EQ(VideoFrame::STORAGE_MOJO_SHARED_BUFFER, frame->storage_type());
    video_decode_cb.Run(Decryptor::kSuccess, std::move(frame));
  }

  void ReturnEOSVideoFrame(const scoped_refptr<DecoderBuffer>& encrypted,
                           const Decryptor::VideoDecodeCB& video_decode_cb) {
    // Simply create and return an End-Of-Stream VideoFrame.
    video_decode_cb.Run(Decryptor::kSuccess, VideoFrame::CreateEOSFrame());
  }

  MOCK_METHOD2(VideoDecoded,
               void(Decryptor::Status status,
                    const scoped_refptr<VideoFrame>& frame));
  MOCK_METHOD0(OnConnectionClosed, void());
  MOCK_METHOD0(OnFrameDestroyed, void());

 protected:
  // Fixture members.
  base::TestMessageLoop message_loop_;

  // The MojoDecryptor that we are testing.
  std::unique_ptr<MojoDecryptor> mojo_decryptor_;

  // The matching MojoDecryptorService for |mojo_decryptor_|.
  std::unique_ptr<MojoDecryptorService> mojo_decryptor_service_;

  // The actual Decryptor object used by |mojo_decryptor_service_|.
  std::unique_ptr<StrictMock<MockDecryptor>> decryptor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoDecryptorTest);
};

TEST_F(MojoDecryptorTest, VideoDecodeFreesBuffer) {
  // Call DecryptAndDecodeVideo(). Once the callback VideoDecoded() completes,
  // the frame will be destroyed, and the buffer will be released.
  {
    InSequence seq;
    EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()));
    EXPECT_CALL(*this, OnFrameDestroyed());
  }
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnSimpleVideoFrame));

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, VideoDecodeFreesMultipleBuffers) {
  // Call DecryptAndDecodeVideo() multiple times.
  const int TIMES = 5;
  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()))
      .Times(TIMES);
  EXPECT_CALL(*this, OnFrameDestroyed()).Times(TIMES);
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(Invoke(this, &MojoDecryptorTest::ReturnSimpleVideoFrame));

  for (int i = 0; i < TIMES; ++i) {
    scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
    mojo_decryptor_->DecryptAndDecodeVideo(
        std::move(buffer),
        base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, VideoDecodeHoldThenFreeBuffers) {
  // Call DecryptAndDecodeVideo() twice. Hang on to the buffers returned,
  // and free them later.
  scoped_refptr<VideoFrame> saved_frame1;
  scoped_refptr<VideoFrame> saved_frame2;

  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()))
      .WillOnce(SaveArg<1>(&saved_frame1))
      .WillOnce(SaveArg<1>(&saved_frame2));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(Invoke(this, &MojoDecryptorTest::ReturnSimpleVideoFrame));

  for (int i = 0; i < 2; ++i) {
    scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
    mojo_decryptor_->DecryptAndDecodeVideo(
        std::move(buffer),
        base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Free the first frame.
  EXPECT_CALL(*this, OnFrameDestroyed());
  saved_frame1 = nullptr;
  base::RunLoop().RunUntilIdle();

  // Repeat for the second frame.
  EXPECT_CALL(*this, OnFrameDestroyed());
  saved_frame2 = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, EOSBuffer) {
  // Call DecryptAndDecodeVideo(), but return an EOS frame (which has no frame
  // data, so no memory needs to be freed).
  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnEOSVideoFrame));

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, DestroyClient) {
  DestroyClient();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, DestroyService) {
  DestroyService();
  base::RunLoop().RunUntilIdle();

  // Attempts to use the client should fail.
  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kError, IsNull()));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _)).Times(0);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
