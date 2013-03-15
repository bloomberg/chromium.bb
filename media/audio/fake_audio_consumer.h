// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_CONSUMER_H_
#define MEDIA_AUDIO_FAKE_AUDIO_CONSUMER_H_

#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/audio/audio_parameters.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class AudioBus;

// A fake audio consumer.  Using a provided message loop, FakeAudioConsumer will
// simulate a real time consumer of audio data.
class MEDIA_EXPORT FakeAudioConsumer {
 public:
  // |message_loop| is the loop on which the ReadCB provided to Start() will be
  // executed on.  |params| is used to determine the frequency of callbacks.
  FakeAudioConsumer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                    const AudioParameters& params);
  ~FakeAudioConsumer();

  // Start executing |read_cb| at a regular interval.  Must be called on the
  // message loop provided during construction.  Stop() must be called before
  // destroying FakeAudioConsumer.
  typedef base::Callback<void(AudioBus* audio_bus)> ReadCB;
  void Start(const ReadCB& read_cb);

  // Stop executing the ReadCB provided to Start().  Cancels any outstanding
  // callbacks.  Safe to call multiple times.  Must be called on the message
  // loop provided during construction.
  void Stop();

 private:
  // Task that regularly calls |read_cb_| according to the playback rate as
  // determined by the audio parameters given during construction.  Runs on
  // |message_loop_|.
  void DoRead();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ReadCB read_cb_;
  scoped_ptr<AudioBus> audio_bus_;
  base::TimeDelta buffer_duration_;
  base::Time next_read_time_;

  // Used to post delayed tasks to the AudioThread that we can cancel.
  base::CancelableClosure read_task_cb_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioConsumer);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_CONSUMER_H_
