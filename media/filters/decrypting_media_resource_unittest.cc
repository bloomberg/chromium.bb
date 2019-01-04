// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/decrypting_media_resource.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

class DecryptingMediaResourceTest : public testing::Test {
 public:
  DecryptingMediaResourceTest() {
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    EXPECT_CALL(decryptor_, CanAlwaysDecrypt()).WillRepeatedly(Return(true));
    EXPECT_CALL(decryptor_, CancelDecrypt(_)).Times(AnyNumber());
    EXPECT_CALL(decryptor_, RegisterNewKeyCB(_, _)).Times(AnyNumber());
    EXPECT_CALL(demuxer_, GetAllStreams())
        .WillRepeatedly(
            Invoke(this, &DecryptingMediaResourceTest::GetAllStreams));

    decrypting_media_resource_ = std::make_unique<DecryptingMediaResource>(
        &demuxer_, &cdm_context_, &null_media_log_,
        scoped_task_environment_.GetMainThreadTaskRunner());
  }

  ~DecryptingMediaResourceTest() {
    // Ensure that the DecryptingMediaResource is destructed before other
    // objects that it internally references but does not own.
    decrypting_media_resource_.reset();
  }

  bool HasEncryptedStream() {
    for (auto* stream : decrypting_media_resource_->GetAllStreams()) {
      if ((stream->type() == DemuxerStream::AUDIO &&
           stream->audio_decoder_config().is_encrypted()) ||
          (stream->type() == DemuxerStream::VIDEO &&
           stream->video_decoder_config().is_encrypted()))
        return true;
    }

    return false;
  }

  void AddStream(DemuxerStream::Type type, bool encrypted) {
    streams_.push_back(CreateMockDemuxerStream(type, encrypted));
  }

  std::vector<DemuxerStream*> GetAllStreams() {
    std::vector<DemuxerStream*> streams;

    for (auto& stream : streams_) {
      streams.push_back(stream.get());
    }

    return streams;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::MockCallback<DecryptingMediaResource::InitCB>
      decrypting_media_resource_init_cb_;
  NullMediaLog null_media_log_;
  StrictMock<MockDecryptor> decryptor_;
  StrictMock<MockDemuxer> demuxer_;
  StrictMock<MockCdmContext> cdm_context_;
  std::unique_ptr<DecryptingMediaResource> decrypting_media_resource_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;
};

TEST_F(DecryptingMediaResourceTest,
       CreatesDecryptingDemuxerStreamForClearStreams) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ false);

  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get());
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(
      decrypting_media_resource_->DecryptingDemuxerStreamCountForTesting(), 2);
  EXPECT_FALSE(HasEncryptedStream());
}

TEST_F(DecryptingMediaResourceTest,
       CreatesDecryptingDemuxerStreamForEncryptedStreams) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get());
  scoped_task_environment_.RunUntilIdle();

  // When using an AesDecryptor we preemptively wrap our streams with a
  // DecryptingDemuxerStream, regardless of encryption. With this in mind, we
  // should have three DecryptingDemuxerStreams.
  EXPECT_EQ(
      decrypting_media_resource_->DecryptingDemuxerStreamCountForTesting(), 2);

  // All of the streams that we get from our DecryptingMediaResource, NOT the
  // internal MediaResource implementation, should be clear.
  EXPECT_FALSE(HasEncryptedStream());
}

TEST_F(DecryptingMediaResourceTest,
       CreatesDecryptingDemuxerStreamForMixedStreams) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get());
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(
      decrypting_media_resource_->DecryptingDemuxerStreamCountForTesting(), 2);
  EXPECT_FALSE(HasEncryptedStream());
}

TEST_F(DecryptingMediaResourceTest,
       OneDecryptingDemuxerStreamFailsInitialization) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  // The first DecryptingDemuxerStream will fail to initialize, causing the
  // callback to be run with a value of false. The second
  // DecryptingDemuxerStream will succeed but never invoke the callback.
  EXPECT_CALL(cdm_context_, GetDecryptor())
      .WillOnce(Return(nullptr))
      .WillRepeatedly(Return(&decryptor_));
  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(false));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get());
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(DecryptingMediaResourceTest,
       BothDecryptingDemuxerStreamsFailInitialization) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  // Both DecryptingDemuxerStreams will fail to initialize but the callback
  // should still only be invoked a single time.
  EXPECT_CALL(cdm_context_, GetDecryptor()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(false));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get());
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace media
