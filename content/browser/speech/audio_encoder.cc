// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/audio_encoder.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "third_party/flac/flac.h"
#include "third_party/speex/speex.h"

using std::string;

namespace {

//-------------------------------- FLACEncoder ---------------------------------

const char* const kContentTypeFLAC = "audio/x-flac; rate=";
const int kFLACCompressionLevel = 0;  // 0 for speed

class FLACEncoder : public speech_input::AudioEncoder {
 public:
  FLACEncoder(int sampling_rate, int bits_per_sample);
  virtual ~FLACEncoder();
  virtual void Encode(const short* samples, int num_samples);
  virtual void Flush();

 private:
  static FLAC__StreamEncoderWriteStatus WriteCallback(
      const FLAC__StreamEncoder* encoder,
      const FLAC__byte buffer[],
      size_t bytes,
      unsigned samples,
      unsigned current_frame,
      void* client_data);

  FLAC__StreamEncoder* encoder_;
  bool is_encoder_initialized_;

  DISALLOW_COPY_AND_ASSIGN(FLACEncoder);
};

FLAC__StreamEncoderWriteStatus FLACEncoder::WriteCallback(
    const FLAC__StreamEncoder* encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void* client_data) {
  FLACEncoder* me = static_cast<FLACEncoder*>(client_data);
  DCHECK(me->encoder_ == encoder);
  me->AppendToBuffer(new string(reinterpret_cast<const char*>(buffer), bytes));
  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLACEncoder::FLACEncoder(int sampling_rate, int bits_per_sample)
    : AudioEncoder(std::string(kContentTypeFLAC) +
                   base::IntToString(sampling_rate)),
      encoder_(FLAC__stream_encoder_new()),
      is_encoder_initialized_(false) {
  FLAC__stream_encoder_set_channels(encoder_, 1);
  FLAC__stream_encoder_set_bits_per_sample(encoder_, bits_per_sample);
  FLAC__stream_encoder_set_sample_rate(encoder_, sampling_rate);
  FLAC__stream_encoder_set_compression_level(encoder_, kFLACCompressionLevel);

  // Initializing the encoder will cause sync bytes to be written to
  // its output stream, so we wait until the first call to this method
  // before doing so.
}

FLACEncoder::~FLACEncoder() {
  FLAC__stream_encoder_delete(encoder_);
}

void FLACEncoder::Encode(const short* samples, int num_samples) {
  if (!is_encoder_initialized_) {
    const FLAC__StreamEncoderInitStatus encoder_status =
        FLAC__stream_encoder_init_stream(encoder_, WriteCallback, NULL, NULL,
                                         NULL, this);
    DCHECK(encoder_status == FLAC__STREAM_ENCODER_INIT_STATUS_OK);
    is_encoder_initialized_ = true;
  }

  // FLAC encoder wants samples as int32s.
  scoped_ptr<FLAC__int32> flac_samples(new FLAC__int32[num_samples]);
  FLAC__int32* flac_samples_ptr = flac_samples.get();
  for (int i = 0; i < num_samples; ++i)
    flac_samples_ptr[i] = samples[i];

  FLAC__stream_encoder_process(encoder_, &flac_samples_ptr, num_samples);
}

void FLACEncoder::Flush() {
  FLAC__stream_encoder_finish(encoder_);
}

//-------------------------------- SpeexEncoder --------------------------------

const char* const kContentTypeSpeex = "audio/x-speex-with-header-byte; rate=";
const int kSpeexEncodingQuality = 8;
const int kMaxSpeexFrameLength = 110;  // (44kbps rate sampled at 32kHz).

// Since the frame length gets written out as a byte in the encoded packet,
// make sure it is within the byte range.
COMPILE_ASSERT(kMaxSpeexFrameLength <= 0xFF, invalidLength);

class SpeexEncoder : public speech_input::AudioEncoder {
 public:
  explicit SpeexEncoder(int sampling_rate);
  virtual ~SpeexEncoder();
  virtual void Encode(const short* samples, int num_samples);
  virtual void Flush() {}

 private:
  void* encoder_state_;
  SpeexBits bits_;
  int samples_per_frame_;
  char encoded_frame_data_[kMaxSpeexFrameLength + 1];  // +1 for the frame size.
  DISALLOW_COPY_AND_ASSIGN(SpeexEncoder);
};

SpeexEncoder::SpeexEncoder(int sampling_rate)
    : AudioEncoder(std::string(kContentTypeSpeex) +
                   base::IntToString(sampling_rate)) {
   // speex_bits_init() does not initialize all of the |bits_| struct.
   memset(&bits_, 0, sizeof(bits_));
   speex_bits_init(&bits_);
   encoder_state_ = speex_encoder_init(&speex_wb_mode);
   DCHECK(encoder_state_);
   speex_encoder_ctl(encoder_state_, SPEEX_GET_FRAME_SIZE, &samples_per_frame_);
   DCHECK(samples_per_frame_ > 0);
   int quality = kSpeexEncodingQuality;
   speex_encoder_ctl(encoder_state_, SPEEX_SET_QUALITY, &quality);
   int vbr = 1;
   speex_encoder_ctl(encoder_state_, SPEEX_SET_VBR, &vbr);
   memset(encoded_frame_data_, 0, sizeof(encoded_frame_data_));
}

SpeexEncoder::~SpeexEncoder() {
  speex_bits_destroy(&bits_);
  speex_encoder_destroy(encoder_state_);
}

void SpeexEncoder::Encode(const short* samples, int num_samples) {
  // Drop incomplete frames, typically those which come in when recording stops.
  num_samples -= (num_samples % samples_per_frame_);
  for (int i = 0; i < num_samples; i += samples_per_frame_) {
    speex_bits_reset(&bits_);
    speex_encode_int(encoder_state_, const_cast<spx_int16_t*>(samples + i),
                     &bits_);

    // Encode the frame and place the size of the frame as the first byte. This
    // is the packet format for MIME type x-speex-with-header-byte.
    int frame_length = speex_bits_write(&bits_, encoded_frame_data_ + 1,
                                        kMaxSpeexFrameLength);
    encoded_frame_data_[0] = static_cast<char>(frame_length);
    AppendToBuffer(new string(encoded_frame_data_, frame_length + 1));
  }
}

}  // namespace

namespace speech_input {

AudioEncoder* AudioEncoder::Create(Codec codec,
                                   int sampling_rate,
                                   int bits_per_sample) {
  if (codec == CODEC_FLAC)
    return new FLACEncoder(sampling_rate, bits_per_sample);
  return new SpeexEncoder(sampling_rate);
}

AudioEncoder::AudioEncoder(const std::string& mime_type)
    : mime_type_(mime_type) {
}

AudioEncoder::~AudioEncoder() {
  STLDeleteElements(&audio_buffers_);
}

bool AudioEncoder::GetEncodedDataAndClear(std::string* encoded_data) {
  if (!audio_buffers_.size())
    return false;

  int audio_buffer_length = 0;
  for (AudioBufferQueue::iterator it = audio_buffers_.begin();
       it != audio_buffers_.end(); ++it) {
    audio_buffer_length += (*it)->length();
  }
  encoded_data->reserve(audio_buffer_length);
  for (AudioBufferQueue::iterator it = audio_buffers_.begin();
       it != audio_buffers_.end(); ++it) {
    encoded_data->append(*(*it));
  }

  STLDeleteElements(&audio_buffers_);

  return true;
}

void AudioEncoder::AppendToBuffer(std::string* item) {
  audio_buffers_.push_back(item);
}

}  // namespace speech_input
