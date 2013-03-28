// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_

#include <map>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/fake_audio_consumer.h"
#include "media/base/audio_converter.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class LoopbackAudioConverter;
class VirtualAudioOutputStream;

// VirtualAudioInputStream converts and mixes audio from attached
// VirtualAudioOutputStreams into a single stream. It will continuously render
// audio until this VirtualAudioInputStream is stopped and closed.
class MEDIA_EXPORT VirtualAudioInputStream : public AudioInputStream {
 public:
  // Callback invoked just after VirtualAudioInputStream is closed.
  typedef base::Callback<void(VirtualAudioInputStream* vais)>
      AfterCloseCallback;

  // Construct a target for audio loopback which mixes multiple data streams
  // into a single stream having the given |params|.
  VirtualAudioInputStream(
      const AudioParameters& params,
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const AfterCloseCallback& after_close_cb);

  virtual ~VirtualAudioInputStream();

  // AudioInputStream:
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;
  virtual void SetAutomaticGainControl(bool enabled) OVERRIDE;
  virtual bool GetAutomaticGainControl() OVERRIDE;

  // Attaches a VirtualAudioOutputStream to be used as input. This
  // VirtualAudioInputStream must outlive all attached streams, so any attached
  // stream must be closed (which causes a detach) before
  // VirtualAudioInputStream is destroyed.
  virtual void AddOutputStream(VirtualAudioOutputStream* stream,
                               const AudioParameters& output_params);

  // Detaches a VirtualAudioOutputStream and removes it as input.
  virtual void RemoveOutputStream(VirtualAudioOutputStream* stream,
                                  const AudioParameters& output_params);

 private:
  friend class VirtualAudioInputStreamTest;

  typedef std::map<AudioParameters, LoopbackAudioConverter*> AudioConvertersMap;

  // When Start() is called on this class, we continuously schedule this
  // callback to render audio using any attached VirtualAudioOutputStreams until
  // Stop() is called.
  void ReadAudio(AudioBus* audio_bus);

  const scoped_refptr<base::MessageLoopProxy> message_loop_;

  AfterCloseCallback after_close_cb_;

  AudioInputCallback* callback_;

  // Non-const for testing.
  scoped_ptr<uint8[]> buffer_;
  AudioParameters params_;

  // AudioConverters associated with the attached VirtualAudioOutputStreams,
  // partitioned by common AudioParameters.
  AudioConvertersMap converters_;

  // AudioConverter that takes all the audio converters and mixes them into one
  // final audio stream.
  AudioConverter mixer_;

  // Number of currently attached VirtualAudioOutputStreams.
  int num_attached_output_streams_;

  // Handles callback timing for consumption of audio data.
  FakeAudioConsumer fake_consumer_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_VIRTUAL_AUDIO_INPUT_STREAM_H_
