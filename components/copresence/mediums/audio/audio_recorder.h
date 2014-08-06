// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_RECORDER_
#define COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_RECORDER_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_converter.h"

namespace base {
class MessageLoop;
}

namespace media {
class AudioBus;
}

namespace copresence {

// The AudioRecorder class will record audio until told to stop.
class AudioRecorder : public media::AudioInputStream::AudioInputCallback,
                      public media::AudioConverter::InputCallback {
 public:
  typedef base::Callback<void(const std::string&)> DecodeSamplesCallback;

  explicit AudioRecorder(const DecodeSamplesCallback& decode_callback);

  // Initializes the object. Do not use this object before calling this method.
  void Initialize();

  void Record();
  void Stop();

  // Cleans up and deletes this object. Do not use object after this call.
  void Finalize();

  // Takes ownership of the stream.
  void set_input_stream_for_testing(
      media::AudioInputStream* input_stream_for_testing) {
    input_stream_for_testing_.reset(input_stream_for_testing);
  }

  // Takes ownership of the params.
  void set_params_for_testing(media::AudioParameters* params_for_testing) {
    params_for_testing_.reset(params_for_testing);
  }

 private:
  friend class AudioRecorderTest;
  FRIEND_TEST_ALL_PREFIXES(AudioRecorderTest, BasicRecordAndStop);
  FRIEND_TEST_ALL_PREFIXES(AudioRecorderTest, OutOfOrderRecordAndStopMultiple);

  virtual ~AudioRecorder();

  // Methods to do our various operations; all of these need to be run on the
  // audio thread.
  void InitializeOnAudioThread();
  void RecordOnAudioThread();
  void StopOnAudioThread();
  void StopAndCloseOnAudioThread();
  void FinalizeOnAudioThread();

  // AudioInputStream::AudioInputCallback overrides:
  // Called by the audio recorder when a full packet of audio data is
  // available. This is called from a special audio thread and the
  // implementation should return as soon as possible.
  virtual void OnData(media::AudioInputStream* stream,
                      const media::AudioBus* source,
                      uint32 hardware_delay_bytes,
                      double volume) OVERRIDE;
  virtual void OnError(media::AudioInputStream* stream) OVERRIDE;

  // AudioConverter::InputCallback overrides:
  virtual double ProvideInput(media::AudioBus* dest,
                              base::TimeDelta buffer_delay) OVERRIDE;

  // Flushes the audio loop, making sure that any queued operations are
  // performed.
  void FlushAudioLoopForTesting();

  media::AudioInputStream* stream_;
  bool is_recording_;
  DecodeSamplesCallback decode_callback_;

  // ProvideInput will use this buffer as its source.
  const media::AudioBus* temp_conversion_buffer_;

  // Outside of the ctor/Initialize method, only access the next variables on
  // the recording thread.
  scoped_ptr<media::AudioBus> buffer_;
  int total_buffer_frames_;
  int buffer_frame_index_;

  scoped_ptr<media::AudioConverter> converter_;

  scoped_ptr<media::AudioInputStream> input_stream_for_testing_;
  scoped_ptr<media::AudioParameters> params_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(AudioRecorder);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_RECORDER_
