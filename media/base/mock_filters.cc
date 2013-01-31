// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

MockDemuxer::MockDemuxer() {}

MockDemuxer::~MockDemuxer() {}

MockDemuxerStream::MockDemuxerStream() {}

MockDemuxerStream::~MockDemuxerStream() {}

MockVideoDecoder::MockVideoDecoder() {
  EXPECT_CALL(*this, HasAlpha()).WillRepeatedly(Return(false));
}

MockVideoDecoder::~MockVideoDecoder() {}

MockAudioDecoder::MockAudioDecoder() {}

MockAudioDecoder::~MockAudioDecoder() {}

MockVideoRenderer::MockVideoRenderer() {}

MockVideoRenderer::~MockVideoRenderer() {}

MockAudioRenderer::MockAudioRenderer() {}

MockAudioRenderer::~MockAudioRenderer() {}

MockDecryptor::MockDecryptor() {}

MockDecryptor::~MockDecryptor() {}

void MockDecryptor::InitializeAudioDecoder(
    scoped_ptr<AudioDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  InitializeAudioDecoderMock(*config, init_cb);
}

void MockDecryptor::InitializeVideoDecoder(
    scoped_ptr<VideoDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  InitializeVideoDecoderMock(*config, init_cb);
}

MockStatisticsCB::MockStatisticsCB() {}

MockStatisticsCB::~MockStatisticsCB() {}

}  // namespace media
