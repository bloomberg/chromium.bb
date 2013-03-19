// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SILENCE_DETECTOR_H_
#define MEDIA_AUDIO_AUDIO_SILENCE_DETECTOR_H_

#include "base/atomic_ref_count.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "media/base/media_export.h"

// An audio silence detector.  It is periodically provided an AudioBus by the
// native audio thread, where simple logic determines whether the audio samples
// in each channel in the buffer represent silence.  If a long-enough period of
// contiguous silence is observed in all channels, a notification callback is
// run on the thread that constructed AudioSilenceDetector.
//
// Note that extreme care has been taken to make the
// AudioSilenceDetector::Scan() method safe to be called on the native audio
// thread.  The code acquires no locks, nor engages in any operation that could
// result in an undetermined/unbounded amount of run-time.  Comments in
// audio_silence_detector.cc elaborate further on the silence detection
// algorithm.

namespace base {
class TimeDelta;
}

namespace media {

class AudioBus;

class MEDIA_EXPORT AudioSilenceDetector {
 public:
  typedef base::Callback<void(bool)> AudibleCallback;

  // Tunable parameters: |questionable_silence_period| is the amount of time
  // where audio must remain silent before triggerring a callback.
  // |indistinguishable_silence_threshold| is the value range below which audio
  // is considered silent, in full-scale units.
  //
  // TODO(miu): |indistinguishable_silence_threshold| should be specified in
  // dbFS units rather than full-scale.  We need a dbFS data type for
  // media/audio first.
  AudioSilenceDetector(int sample_rate,
                       const base::TimeDelta& questionable_silence_period,
                       float indistinguishable_silence_threshold);

  ~AudioSilenceDetector();

  // Start detecting silence, notifying via the given callback.
  void Start(const AudibleCallback& notify_is_audible);

  // Stop detecting silence.  If |notify_ending_in_silence| is true, a final
  // notify_is_audible(false) call will be made here.
  void Stop(bool notify_ending_in_silence);

  // Scan more |frames| of audio data from |buffer|.  This is usually called
  // within the native audio thread's "more data" callback.
  void Scan(const AudioBus* buffer, int frames);

 private:
  // Called by |poll_timer_| at regular intervals to determine whether to invoke
  // the callback due to a silence state change.
  void MaybeInvokeAudibleCallback();

  // Returns true if the first |num_frames| frames in all channels in |buffer|
  // probably contain silence.  A simple heuristic is used to quickly examine a
  // subset of the samples in each channel, hence the name of this method.
  // "Silence" means that the range of the sample values examined does not
  // exceed |silence_threshold_|.
  bool ProbablyContainsSilence(const AudioBus* buffer, int num_frames);

  // Time between polls for changes in state.
  const base::TimeDelta polling_period_;

  // Number of frames of contiguous silence to be observed before setting
  // |observing_silence_| to false.
  const int frames_before_observing_silence_;

  // Threshold below which audio should be considered indistinguishably silent.
  const float silence_threshold_;

  // Number of frames of contiguous silence observed thus far on the native
  // audio thread.
  int frames_silent_so_far_;

  // Boolean state (0 or 1) set by the native audio thread.  This is polled
  // regularly by the thread that invokes the callback.
  base::AtomicRefCount observing_silence_;

  // Callback for notifying of a detected transition to silence or non-silence.
  AudibleCallback notify_is_audible_;

  // Last reported audible state, used for de-duping callback invocations.
  bool was_audible_;

  // Fires regularly, calling MaybeInvokeAudibleCallback().
  base::RepeatingTimer<AudioSilenceDetector> poll_timer_;

  // Constructor, destructor and most methods must be called on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioSilenceDetector);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SILENCE_DETECTOR_H_
