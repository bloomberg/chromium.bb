// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing AudioDevice.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. Render thread.
//    This object is created on the render thread.
// 2. Pipeline thread
//    OnInitialize() is called here with the audio format.
//    Play/Pause/Seek also happens here.
// 3. Audio thread created by the AudioDevice.
//    Render() is called here where audio data is decoded into raw PCM data.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/audio_device.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/filters/audio_renderer_base.h"

class AudioMessageFilter;

class CONTENT_EXPORT AudioRendererImpl
    : public media::AudioRendererBase,
      public AudioDevice::RenderCallback {
 public:
  // Methods called on Render thread ------------------------------------------
  AudioRendererImpl();
  virtual ~AudioRendererImpl();

  // Methods called on pipeline thread ----------------------------------------
  // media::Filter implementation.
  virtual void SetPlaybackRate(float rate) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time,
                    const media::FilterStatusCB& cb) OVERRIDE;
  virtual void Play(const base::Closure& callback) OVERRIDE;

  // media::AudioRenderer implementation.
  virtual void SetVolume(float volume) OVERRIDE;

 protected:
  // Methods called on pipeline thread ----------------------------------------
  // These methods are called from AudioRendererBase.
  virtual bool OnInitialize(int bits_per_channel,
                            ChannelLayout channel_layout,
                            int sample_rate) OVERRIDE;
  virtual void OnStop() OVERRIDE;

 private:
  // For access to constructor and IO thread methods.
  friend class AudioRendererImplTest;
  friend class DelegateCaller;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, Stop);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest,
                           DestroyedMessageLoop_ConsumeAudioSamples);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, UpdateEarliestEndTime);
  // Helper methods.
  // Convert number of bytes to duration of time using information about the
  // number of channels, sample rate and sample bits.
  base::TimeDelta ConvertToDuration(int bytes);

  // Methods called on pipeline thread ----------------------------------------
  void DoPlay();
  void DoPause();
  void DoSeek();

  // AudioDevice::RenderCallback implementation.
  virtual size_t Render(const std::vector<float*>& audio_data,
                        size_t number_of_frames,
                        size_t audio_delay_milliseconds) OVERRIDE;

  // Accessors used by tests.
  base::Time earliest_end_time() const {
    return earliest_end_time_;
  }

  void set_earliest_end_time(const base::Time& earliest_end_time) {
    earliest_end_time_ = earliest_end_time;
  }

  uint32 bytes_per_second() const {
    return bytes_per_second_;
  }

  // Estimate earliest time when current buffer can stop playing.
  void UpdateEarliestEndTime(int bytes_filled,
                             base::TimeDelta request_delay,
                             base::Time time_now);

  // Used to calculate audio delay given bytes.
  uint32 bytes_per_second_;

  // A flag that indicates this filter is called to stop.
  bool stopped_;

  // audio_device_ is the sink (destination) for rendered audio.
  scoped_refptr<AudioDevice> audio_device_;

  // We're supposed to know amount of audio data OS or hardware buffered, but
  // that is not always so -- on my Linux box
  // AudioBuffersState::hardware_delay_bytes never reaches 0.
  //
  // As a result we cannot use it to find when stream ends. If we just ignore
  // buffered data we will notify host that stream ended before it is actually
  // did so, I've seen it done ~140ms too early when playing ~150ms file.
  //
  // Instead of trying to invent OS-specific solution for each and every OS we
  // are supporting, use simple workaround: every time we fill the buffer we
  // remember when it should stop playing, and do not assume that buffer is
  // empty till that time. Workaround is not bulletproof, as we don't exactly
  // know when that particular data would start playing, but it is much better
  // than nothing.
  base::Time earliest_end_time_;

  AudioParameters audio_parameters_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
