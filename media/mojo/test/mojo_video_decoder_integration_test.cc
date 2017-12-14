// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/decode_status.h"
#include "media/base/decoder_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {

const int kMaxDecodeRequests = 4;

// A mock VideoDecoder with helpful default functionality.
// TODO(sandersd): Determine how best to merge this with MockVideoDecoder
// declared in mock_filters.h.
class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder() {
    // Treat const getters like a NiceMock.
    EXPECT_CALL(*this, GetDisplayName())
        .WillRepeatedly(Return("MockVideoDecoder"));
    EXPECT_CALL(*this, NeedsBitstreamConversion())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, CanReadWithoutStalling()).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, GetMaxDecodeRequests())
        .WillRepeatedly(Return(kMaxDecodeRequests));

    // For regular methods, only configure a default action.
    ON_CALL(*this, DoInitialize(_)).WillByDefault(RunCallback<0>(true));
    ON_CALL(*this, Decode(_, _))
        .WillByDefault(Invoke(this, &MockVideoDecoder::DoDecode));
    ON_CALL(*this, Reset(_))
        .WillByDefault(Invoke(this, &MockVideoDecoder::DoReset));
  }

  // Re-declare as public.
  ~MockVideoDecoder() override {}

  // media::VideoDecoder implementation
  MOCK_CONST_METHOD0(GetDisplayName, std::string());

  // Initialize() records values before delegating to the mock method.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override {
    config_ = config;
    output_cb_ = output_cb;
    DoInitialize(init_cb);
  }

  MOCK_METHOD2(Decode,
               void(const scoped_refptr<DecoderBuffer>& buffer,
                    const DecodeCB&));
  MOCK_METHOD1(Reset, void(const base::Closure&));
  MOCK_CONST_METHOD0(NeedsBitstreamConversion, bool());
  MOCK_CONST_METHOD0(CanReadWithoutStalling, bool());
  MOCK_CONST_METHOD0(GetMaxDecodeRequests, int());

  // Mock helpers.
  MOCK_METHOD1(DoInitialize, void(const InitCB&));
  MOCK_METHOD0(GetReleaseMailboxCB, VideoFrame::ReleaseMailboxCB());

  // Returns an output frame immediately.
  // TODO(sandersd): Extend to support tests of MojoVideoFrame frames.
  void DoDecode(const scoped_refptr<DecoderBuffer>& buffer,
                const DecodeCB& decode_cb) {
    if (!buffer->end_of_stream()) {
      gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
      mailbox_holders[0].mailbox.name[0] = 1;
      output_cb_.Run(VideoFrame::WrapNativeTextures(
          PIXEL_FORMAT_ARGB, mailbox_holders, GetReleaseMailboxCB(),
          config_.coded_size(), config_.visible_rect(), config_.natural_size(),
          buffer->timestamp()));
    }
    // |decode_cb| must not be called from the same stack.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(decode_cb, DecodeStatus::OK));
  }

  void DoReset(const base::Closure& reset_cb) {
    // |reset_cb| must not be called from the same stack.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, reset_cb);
  }

 private:
  // Destructing a std::unique_ptr<VideoDecoder>(this) is a no-op.
  // TODO(sandersd): After this, any method call is an error. Implement checks
  // for that.
  void Destroy() override { DVLOG(1) << __func__ << "(): Ignored"; }

  VideoDecoderConfig config_;
  OutputCB output_cb_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

// Proxies CreateVideoDecoder() to a callback.
class FakeMojoMediaClient : public MojoMediaClient {
 public:
  using CreateVideoDecoderCB =
      base::Callback<std::unique_ptr<VideoDecoder>(MediaLog*)>;

  FakeMojoMediaClient(CreateVideoDecoderCB create_video_decoder_cb)
      : create_video_decoder_cb_(std::move(create_video_decoder_cb)) {}

  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb) override {
    return create_video_decoder_cb_.Run(media_log);
  }

 private:
  CreateVideoDecoderCB create_video_decoder_cb_;

  DISALLOW_COPY_AND_ASSIGN(FakeMojoMediaClient);
};

}  // namespace

class MojoVideoDecoderIntegrationTest : public ::testing::Test {
 public:
  MojoVideoDecoderIntegrationTest()
      : mojo_media_client_(
            base::Bind(&MojoVideoDecoderIntegrationTest::CreateVideoDecoder,
                       base::Unretained(this))) {
    mojom::VideoDecoderPtr remote_video_decoder;
    binding_ = mojo::MakeStrongBinding(
        std::make_unique<MojoVideoDecoderService>(&mojo_media_client_,
                                                  &mojo_cdm_service_context_),
        mojo::MakeRequest(&remote_video_decoder));
    client_ = std::make_unique<MojoVideoDecoder>(
        base::ThreadTaskRunnerHandle::Get(), nullptr, &client_media_log_,
        std::move(remote_video_decoder), RequestOverlayInfoCB());
  }

  void TearDown() override {
    if (client_) {
      client_.reset();
      RunUntilIdle();
    }
  }

 protected:
  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  void SetWriterCapacity(uint32_t capacity) {
    client_->set_writer_capacity_for_testing(capacity);
  }

  bool Initialize() {
    bool result = false;

    EXPECT_CALL(*decoder_, DoInitialize(_));

    StrictMock<base::MockCallback<VideoDecoder::InitCB>> init_cb;
    EXPECT_CALL(init_cb, Run(_)).WillOnce(SaveArg<0>(&result));

    client_->Initialize(TestVideoConfig::Normal(), false, nullptr,
                        init_cb.Get(), output_cb_.Get());
    RunUntilIdle();

    return result;
  }

  DecodeStatus Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      VideoFrame::ReleaseMailboxCB release_cb =
                          VideoFrame::ReleaseMailboxCB()) {
    DecodeStatus result = DecodeStatus::DECODE_ERROR;

    if (!buffer->end_of_stream()) {
      EXPECT_CALL(*decoder_, GetReleaseMailboxCB())
          .WillOnce(Return(release_cb));
    }
    EXPECT_CALL(*decoder_, Decode(_, _));

    StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
    EXPECT_CALL(decode_cb, Run(_)).WillOnce(SaveArg<0>(&result));

    client_->Decode(buffer, decode_cb.Get());
    RunUntilIdle();

    return result;
  }

  scoped_refptr<DecoderBuffer> CreateKeyframe(int64_t timestamp_ms) {
    // Use 32 bytes to simulated chunked write (with capacity 10; see below).
    std::vector<uint8_t> data(32, 0);

    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(data.data(), data.size());

    buffer->set_timestamp(base::TimeDelta::FromMilliseconds(timestamp_ms));
    buffer->set_duration(base::TimeDelta::FromMilliseconds(10));
    buffer->set_is_key_frame(true);

    return buffer;
  }

  // Callback that |client_| will deliver VideoFrames to.
  StrictMock<base::MockCallback<VideoDecoder::OutputCB>> output_cb_;

  // MojoVideoDecoder (client) under test.
  std::unique_ptr<MojoVideoDecoder> client_;

  // MediaLog that |client_| will deliver log events to.
  StrictMock<MockMediaLog> client_media_log_;

  // VideoDecoder (impl used by service) under test.
  std::unique_ptr<MockVideoDecoder> decoder_ =
      std::make_unique<StrictMock<MockVideoDecoder>>();

  // MediaLog that the service has provided to |decoder_|. This should be
  // proxied to |client_media_log_|.
  MediaLog* decoder_media_log_ = nullptr;

 private:
  // Passes |decoder_| to the service.
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(MediaLog* media_log) {
    DCHECK(!decoder_media_log_);
    decoder_media_log_ = media_log;
    // Since MockVideoDecoder::Destroy() is a no-op, this doesn't actually
    // transfer ownership.
    return std::unique_ptr<VideoDecoder>(decoder_.get());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  MojoCdmServiceContext mojo_cdm_service_context_;

  // Provides |decoder_| to the service.
  FakeMojoMediaClient mojo_media_client_;

  // Owns the service, bound to |client_|.
  mojo::StrongBindingPtr<mojom::VideoDecoder> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoDecoderIntegrationTest);
};

TEST_F(MojoVideoDecoderIntegrationTest, CreateAndDestroy) {}

TEST_F(MojoVideoDecoderIntegrationTest, Initialize) {
  ASSERT_TRUE(Initialize());
  EXPECT_EQ(client_->GetDisplayName(), "MojoVideoDecoder");
  EXPECT_EQ(client_->NeedsBitstreamConversion(), false);
  EXPECT_EQ(client_->CanReadWithoutStalling(), true);
  EXPECT_EQ(client_->GetMaxDecodeRequests(), kMaxDecodeRequests);
}

TEST_F(MojoVideoDecoderIntegrationTest, Decode) {
  ASSERT_TRUE(Initialize());

  EXPECT_CALL(output_cb_, Run(_));
  ASSERT_EQ(Decode(CreateKeyframe(0)), DecodeStatus::OK);
  Mock::VerifyAndClearExpectations(&output_cb_);

  ASSERT_EQ(Decode(DecoderBuffer::CreateEOSBuffer()), DecodeStatus::OK);
}

TEST_F(MojoVideoDecoderIntegrationTest, Release) {
  ASSERT_TRUE(Initialize());

  StrictMock<base::MockCallback<VideoFrame::ReleaseMailboxCB>> release_cb;
  scoped_refptr<VideoFrame> frame;

  EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
  ASSERT_EQ(Decode(CreateKeyframe(0), release_cb.Get()), DecodeStatus::OK);
  Mock::VerifyAndClearExpectations(&output_cb_);

  EXPECT_CALL(release_cb, Run(_));
  frame = nullptr;
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, ReleaseAfterShutdown) {
  ASSERT_TRUE(Initialize());

  StrictMock<base::MockCallback<VideoFrame::ReleaseMailboxCB>> release_cb;
  scoped_refptr<VideoFrame> frame;

  EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
  ASSERT_EQ(Decode(CreateKeyframe(0), release_cb.Get()), DecodeStatus::OK);
  Mock::VerifyAndClearExpectations(&output_cb_);

  client_.reset();
  RunUntilIdle();

  EXPECT_CALL(release_cb, Run(_));
  frame = nullptr;
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, ResetDuringDecode) {
  ASSERT_TRUE(Initialize());

  VideoFrame::ReleaseMailboxCB release_cb = VideoFrame::ReleaseMailboxCB();
  StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
  StrictMock<base::MockCallback<base::Closure>> reset_cb;

  EXPECT_CALL(*decoder_, GetReleaseMailboxCB())
      .WillRepeatedly(Return(release_cb));
  EXPECT_CALL(output_cb_, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Decode(_, _)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Reset(_)).Times(1);

  InSequence s;  // Make sure all callbacks are fired in order.
  EXPECT_CALL(decode_cb, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(reset_cb, Run());

  int64_t timestamp_ms = 0;
  for (int j = 0; j < kMaxDecodeRequests; ++j) {
    client_->Decode(CreateKeyframe(timestamp_ms++), decode_cb.Get());
  }

  client_->Reset(reset_cb.Get());

  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, ResetDuringDecode_ChunkedWrite) {
  // Use a small writer capacity to force chunked write.
  SetWriterCapacity(10);

  ASSERT_TRUE(Initialize());

  VideoFrame::ReleaseMailboxCB release_cb = VideoFrame::ReleaseMailboxCB();
  StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
  StrictMock<base::MockCallback<base::Closure>> reset_cb;

  EXPECT_CALL(*decoder_, GetReleaseMailboxCB())
      .WillRepeatedly(Return(release_cb));
  EXPECT_CALL(output_cb_, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Decode(_, _)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Reset(_)).Times(1);

  InSequence s;  // Make sure all callbacks are fired in order.
  EXPECT_CALL(decode_cb, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(reset_cb, Run());

  int64_t timestamp_ms = 0;
  for (int j = 0; j < kMaxDecodeRequests; ++j) {
    client_->Decode(CreateKeyframe(timestamp_ms++), decode_cb.Get());
  }

  client_->Reset(reset_cb.Get());

  RunUntilIdle();
}

}  // namespace media
