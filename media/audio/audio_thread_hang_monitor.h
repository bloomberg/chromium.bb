// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_THREAD_HANG_MONITOR_H_
#define MEDIA_AUDIO_AUDIO_THREAD_HANG_MONITOR_H_

#include "media/audio/audio_manager.h"

#include <atomic>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/media_export.h"
#include "media/base/media_switches.h"

namespace base {
class TickClock;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// This class detects if the audio manager thread is hung. It logs a histogram,
// and can optionally (if |dump_on_hang| is set) upload a crash dump when a hang
// is detected. It runs on a task runner from the task scheduler. It works by
// posting a task to the audio thread every minute and checking that it was
// executed. If three consecutive such pings are missed, the thread is
// considered hung.
class MEDIA_EXPORT AudioThreadHangMonitor final {
 public:
  using Ptr =
      std::unique_ptr<AudioThreadHangMonitor, base::OnTaskRunnerDeleter>;

  // These values are histogrammed over time; do not change their ordinal
  // values.
  enum class ThreadStatus {
    // kNone = 0, obsolete.
    kStarted = 1,
    kHung,
    kRecovered,
    kMaxValue = kRecovered
  };

  // |monitor_task_runner| may be set explicitly by tests only. Other callers
  // should use the default.
  static Ptr Create(
      bool dump_on_hang,
      const base::TickClock* clock,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner,
      scoped_refptr<base::SequencedTaskRunner> monitor_task_runner = nullptr);

  ~AudioThreadHangMonitor();

  // Thread-safe.
  bool IsAudioThreadHung() const;

 private:
  class SharedAtomicFlag final
      : public base::RefCountedThreadSafe<SharedAtomicFlag> {
   public:
    SharedAtomicFlag();

    std::atomic_bool flag_ = {false};

   private:
    friend class base::RefCountedThreadSafe<SharedAtomicFlag>;
    ~SharedAtomicFlag();
  };

  AudioThreadHangMonitor(
      bool dump_on_hang,
      const base::TickClock* clock,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner);

  void StartTimer();

  bool NeverLoggedThreadHung() const;
  bool NeverLoggedThreadRecoveredAfterHung() const;

  // This function is run by the |timer_|. It checks if the audio thread has
  // shown signs of life since the last time it was called (by checking the
  // |alive_flag_|) and updates the value of |successful_pings_| and
  // |failed_pings_| as appropriate. It also changes the thread status and logs
  // its value to a histogram.
  void CheckIfAudioThreadIsAlive();

  // LogHistogramThreadStatus logs |thread_status_| to a histogram.
  void LogHistogramThreadStatus();

  const base::TickClock* const clock_;

  // This flag is set to false on the monitor sequence and then set to true on
  // the audio thread to indicate that the audio thread is alive.
  const scoped_refptr<SharedAtomicFlag> alive_flag_;

  // |audio_task_runner_| is the task runner of the audio thread.
  const scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // If |dump_on_hang_| is set, a crash dump will be collected the first time
  // the thread is detected as hung (note that no actual crashing is involved).
  const bool dump_on_hang_;

  std::atomic<ThreadStatus> audio_thread_status_ = {ThreadStatus::kStarted};

  // All fields below are accessed on |monitor_sequence|.
  SEQUENCE_CHECKER(monitor_sequence_);

  // Timer to check |alive_flag_| regularly.
  base::RepeatingTimer timer_;

  // This variable is used to check to detect suspend/resume cycles.
  // If a long time has passed since the timer was last fired, it is likely due
  // to the machine being suspended. In such a case, we want to avoid falsely
  // detecting the audio thread as hung.
  base::TimeTicks last_check_time_ = base::TimeTicks();

  // |recent_ping_state_| tracks the recent life signs from the audio thread. If
  // the most recent ping was successful, the number indicates the number of
  // successive successful pings. If the most recent ping was failed, the number
  // is the negative of the number of successive failed pings.
  int recent_ping_state_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AudioThreadHangMonitor);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_THREAD_HANG_MONITOR_H_
