// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_UNIFIED_H_
#define MEDIA_AUDIO_PULSE_PULSE_UNIFIED_H_

#include <pulse/pulseaudio.h>

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_fifo.h"

namespace media {

class AudioManagerBase;
class SeekableBuffer;

class PulseAudioUnifiedStream : public AudioOutputStream {
 public:
  PulseAudioUnifiedStream(const AudioParameters& params,
                          AudioManagerBase* manager);

  virtual ~PulseAudioUnifiedStream();

  // Implementation of PulseAudioUnifiedStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

 private:
  // Called by PulseAudio when |pa_stream_| change state.  If an unexpected
  // failure state change happens and |source_callback_| is set
  // this method will forward the error via OnError().
  static void StreamNotifyCallback(pa_stream* s, void* user_data);

  // Called by PulseAudio recording stream when it has data.
  static void ReadCallback(pa_stream* s, size_t length, void* user_data);

  // Helpers for ReadCallback() to read and write data.
  void WriteData(size_t requested_bytes);
  void ReadData();

  // Close() helper function to free internal structs.
  void Reset();

  // AudioParameters from the constructor.
  const AudioParameters params_;

  // Audio manager that created us.  Used to report that we've closed.
  AudioManagerBase* manager_;

  // PulseAudio API structs.
  pa_context* pa_context_;
  pa_threaded_mainloop* pa_mainloop_;
  pa_stream* input_stream_;
  pa_stream* output_stream_;

  // Float representation of volume from 0.0 to 1.0.
  float volume_;

  // Callback to audio data source.  Must only be modified while holding a lock
  // on |pa_mainloop_| via pa_threaded_mainloop_lock().
  AudioSourceCallback* source_callback_;

  scoped_ptr<AudioBus> input_bus_;
  scoped_ptr<AudioBus> output_bus_;

  // Used for input to output buffering.
  scoped_ptr<media::SeekableBuffer> fifo_;

  // Temporary storage for recorded data. It gets a packet of data from
  // |fifo_| and deliver the data to OnMoreIOData() callback.
  scoped_array<uint8> input_data_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioUnifiedStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_UNIFIED_H_
