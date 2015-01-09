// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A fake implementation of AudioInputStream, useful for testing purpose.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/sounds/wav_audio_handler.h"
#include "media/base/audio_converter.h"

namespace media {

class AudioBus;
class AudioManagerBase;

// This class can either generate a beep sound or play audio from a file.
class MEDIA_EXPORT FakeAudioInputStream
    : public AudioInputStream, AudioConverter::InputCallback {
 public:
  static AudioInputStream* MakeFakeStream(
      AudioManagerBase* manager, const AudioParameters& params);

  bool Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;

  // Generate one beep sound. This method is called by
  // FakeVideoCaptureDevice to test audio/video synchronization.
  // This is a static method because FakeVideoCaptureDevice is
  // disconnected from an audio device. This means only one instance of
  // this class gets to respond, which is okay because we assume there's
  // only one stream for this testing purpose.
  // TODO(hclam): Make this non-static. To do this we'll need to fix
  // crbug.com/159053 such that video capture device is aware of audio
  // input stream.
  static void BeepOnce();

 private:
  FakeAudioInputStream(AudioManagerBase* manager,
                       const AudioParameters& params);
  ~FakeAudioInputStream() override;

  void DoCallback();

  // Opens this stream reading from a |wav_filename| rather than beeping.
  void OpenInFileMode(const base::FilePath& wav_filename);

  // Returns true if the device is playing from a file; false if we're beeping.
  bool PlayingFromFile();

  void PlayFile();
  void PlayBeep();

  AudioManagerBase* audio_manager_;
  AudioInputCallback* callback_;
  scoped_ptr<uint8[]> buffer_;
  int buffer_size_;
  AudioParameters params_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::TimeTicks last_callback_time_;
  base::TimeDelta callback_interval_;
  base::TimeDelta interval_from_last_beep_;
  int beep_duration_in_buffers_;
  int beep_generated_in_buffers_;
  int beep_period_in_frames_;
  scoped_ptr<media::AudioBus> audio_bus_;
  scoped_ptr<uint8[]> wav_file_data_;
  scoped_ptr<media::WavAudioHandler> wav_audio_handler_;
  scoped_ptr<media::AudioConverter> file_audio_converter_;
  int wav_file_read_pos_;

  // Allows us to run tasks on the FakeAudioInputStream instance which are
  // bound by its lifetime.
  base::WeakPtrFactory<FakeAudioInputStream> weak_factory_;

  // If running in file mode, this provides audio data from wav_audio_handler_.
  double ProvideInput(AudioBus* audio_bus,
                      base::TimeDelta buffer_delay) override;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_INPUT_STREAM_H_
