// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A new breed of mock media filters, this time using gmock!  Feel free to add
// actions if you need interesting side-effects.
//
// Don't forget you can use StrictMock<> and NiceMock<> if you want the mock
// filters to fail the test or do nothing when an unexpected method is called.
// http://code.google.com/p/googlemock/wiki/CookBook#Nice_Mocks_and_Strict_Mocks

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <string>

#include "base/callback.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_renderer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/filter_collection.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Use this template to test for object destruction by setting expectations on
// the method OnDestroy().
//
// TODO(scherkus): not sure about the naming...  perhaps contribute this back
// to gmock itself!
template<class MockClass>
class Destroyable : public MockClass {
 public:
  Destroyable() {}

  MOCK_METHOD0(OnDestroy, void());

 protected:
  virtual ~Destroyable() {
    OnDestroy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Destroyable);
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();

  // Demuxer implementation.
  MOCK_METHOD2(Initialize, void(DemuxerHost* host, const PipelineStatusCB& cb));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Seek, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD0(OnAudioRendererDisabled, void());
  MOCK_METHOD1(GetStream, scoped_refptr<DemuxerStream>(DemuxerStream::Type));
  MOCK_CONST_METHOD0(GetStartTime, base::TimeDelta());

 protected:
  virtual ~MockDemuxer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream();

  // DemuxerStream implementation.
  MOCK_METHOD0(type, Type());
  MOCK_METHOD1(Read, void(const ReadCB& read_cb));
  MOCK_METHOD0(audio_decoder_config, const AudioDecoderConfig&());
  MOCK_METHOD0(video_decoder_config, const VideoDecoderConfig&());
  MOCK_METHOD0(EnableBitstreamConverter, void());

 protected:
  virtual ~MockDemuxerStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();

  // VideoDecoder implementation.
  MOCK_METHOD3(Initialize, void(const scoped_refptr<DemuxerStream>&,
                                const PipelineStatusCB&,
                                const StatisticsCB&));
  MOCK_METHOD1(Read, void(const ReadCB&));
  MOCK_METHOD1(Reset, void(const base::Closure&));
  MOCK_METHOD1(Stop, void(const base::Closure&));
  MOCK_CONST_METHOD0(HasAlpha, bool());

 protected:
  virtual ~MockVideoDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();

  // AudioDecoder implementation.
  MOCK_METHOD3(Initialize, void(const scoped_refptr<DemuxerStream>&,
                                const PipelineStatusCB&,
                                const StatisticsCB&));
  MOCK_METHOD1(Read, void(const ReadCB&));
  MOCK_METHOD0(bits_per_channel, int(void));
  MOCK_METHOD0(channel_layout, ChannelLayout(void));
  MOCK_METHOD0(samples_per_second, int(void));
  MOCK_METHOD1(Reset, void(const base::Closure&));

 protected:
  virtual ~MockAudioDecoder();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer();
  virtual ~MockVideoRenderer();

  // VideoRenderer implementation.
  MOCK_METHOD10(Initialize, void(const scoped_refptr<DemuxerStream>& stream,
                                 const VideoDecoderList& decoders,
                                 const PipelineStatusCB& init_cb,
                                 const StatisticsCB& statistics_cb,
                                 const TimeCB& time_cb,
                                 const NaturalSizeChangedCB& size_changed_cb,
                                 const base::Closure& ended_cb,
                                 const PipelineStatusCB& error_cb,
                                 const TimeDeltaCB& get_time_cb,
                                 const TimeDeltaCB& get_duration_cb));
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Pause, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD2(Preroll, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer();
  virtual ~MockAudioRenderer();

  // AudioRenderer implementation.
  MOCK_METHOD8(Initialize, void(const scoped_refptr<DemuxerStream>& stream,
                                const PipelineStatusCB& init_cb,
                                const StatisticsCB& statistics_cb,
                                const base::Closure& underflow_cb,
                                const TimeCB& time_cb,
                                const base::Closure& ended_cb,
                                const base::Closure& disabled_cb,
                                const PipelineStatusCB& error_cb));
  MOCK_METHOD1(Play, void(const base::Closure& callback));
  MOCK_METHOD1(Pause, void(const base::Closure& callback));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(Stop, void(const base::Closure& callback));
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD2(Preroll, void(base::TimeDelta time, const PipelineStatusCB& cb));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(ResumeAfterUnderflow, void(bool buffer_more_audio));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

class MockDecryptor : public Decryptor {
 public:
  MockDecryptor();
  virtual ~MockDecryptor();

  MOCK_METHOD4(GenerateKeyRequest, bool(const std::string& key_system,
                                        const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length));
  MOCK_METHOD6(AddKey, void(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id));
  MOCK_METHOD2(CancelKeyRequest, void(const std::string& key_system,
                                      const std::string& session_id));
  MOCK_METHOD2(RegisterNewKeyCB, void(StreamType stream_type,
                                      const NewKeyCB& new_key_cb));
  MOCK_METHOD3(Decrypt, void(StreamType stream_type,
                             const scoped_refptr<DecoderBuffer>& encrypted,
                             const DecryptCB& decrypt_cb));
  MOCK_METHOD1(CancelDecrypt, void(StreamType stream_type));
  MOCK_METHOD2(InitializeAudioDecoder,
               void(const AudioDecoderConfig& config,
                    const DecoderInitCB& init_cb));
  MOCK_METHOD2(InitializeVideoDecoder,
               void(const VideoDecoderConfig& config,
                    const DecoderInitCB& init_cb));
  MOCK_METHOD2(DecryptAndDecodeAudio,
               void(const scoped_refptr<media::DecoderBuffer>& encrypted,
                    const AudioDecodeCB& audio_decode_cb));
  MOCK_METHOD2(DecryptAndDecodeVideo,
               void(const scoped_refptr<media::DecoderBuffer>& encrypted,
                    const VideoDecodeCB& video_decode_cb));
  MOCK_METHOD1(ResetDecoder, void(StreamType stream_type));
  MOCK_METHOD1(DeinitializeDecoder, void(StreamType stream_type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecryptor);
};

// Helper mock statistics callback.
class MockStatisticsCB {
 public:
  MockStatisticsCB();
  ~MockStatisticsCB();

  MOCK_METHOD1(OnStatistics, void(const media::PipelineStatistics& statistics));
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
