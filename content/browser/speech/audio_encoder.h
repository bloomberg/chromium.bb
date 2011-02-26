// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_
#define CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_

#include <list>
#include <string>

#include "base/basictypes.h"

namespace speech_input {

// Provides a simple interface to encode raw audio using the various speech
// codecs.
class AudioEncoder {
 public:
  enum Codec {
    CODEC_FLAC,
    CODEC_SPEEX,
  };

  static AudioEncoder* Create(Codec codec,
                              int sampling_rate,
                              int bits_per_sample);

  virtual ~AudioEncoder();

  // Encodes each frame of raw audio in |samples| to the internal buffer. Use
  // |GetEncodedData| to read the result after this call or when recording
  // completes.
  virtual void Encode(const short* samples, int num_samples) = 0;

  // Finish encoding and flush any pending encoded bits out.
  virtual void Flush() = 0;

  // Copies the encoded audio to the given string. Returns true if the output
  // is not empty.
  bool GetEncodedData(std::string* encoded_data);

  const std::string& mime_type() { return mime_type_; }

 protected:
  AudioEncoder(const std::string& mime_type);

  void AppendToBuffer(std::string* item);

 private:
  // Buffer holding the recorded audio. Owns the strings inside the list.
  typedef std::list<std::string*> AudioBufferQueue;
  AudioBufferQueue audio_buffers_;
  std::string mime_type_;
  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_
