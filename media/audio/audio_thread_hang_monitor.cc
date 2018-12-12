// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_thread_hang_monitor.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"

namespace media {

namespace {

// Maximum number of failed pings to the audio thread allowed. A UMA will be
// recorded once this count is reached. We require at least three failed pings
// before recording to ensure unobservable power events aren't mistakenly
// caught (e.g., the system suspends before a OnSuspend() event can be fired).
constexpr int kMaxFailedPingsCount = 3;

constexpr base::TimeDelta kHungDeadline = base::TimeDelta::FromMinutes(1);

}  // namespace

AudioThreadHangMonitor::SharedAtomicFlag::SharedAtomicFlag() {}
AudioThreadHangMonitor::SharedAtomicFlag::~SharedAtomicFlag() {}

// static
AudioThreadHangMonitor::Ptr AudioThreadHangMonitor::Create(
    bool dump_on_hang,
    const base::TickClock* clock,
    scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner,
    scoped_refptr<base::SequencedTaskRunner> monitor_task_runner) {
  if (!monitor_task_runner)
    monitor_task_runner = base::CreateSequencedTaskRunnerWithTraits({});

  auto monitor =
      Ptr(new AudioThreadHangMonitor(dump_on_hang, clock,
                                     std::move(audio_thread_task_runner)),
          base::OnTaskRunnerDeleter(monitor_task_runner));

  // |monitor| is destroyed on |monitor_task_runner|, so Unretained is safe.
  monitor_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&AudioThreadHangMonitor::StartTimer,
                                base::Unretained(monitor.get())));
  return monitor;
}

AudioThreadHangMonitor::~AudioThreadHangMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_);
}

bool AudioThreadHangMonitor::IsAudioThreadHung() const {
  return audio_thread_status_ == ThreadStatus::kHung;
}

AudioThreadHangMonitor::AudioThreadHangMonitor(
    bool dump_on_hang,
    const base::TickClock* clock,
    scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner)
    : clock_(clock),
      alive_flag_(base::MakeRefCounted<SharedAtomicFlag>()),
      audio_task_runner_(std::move(audio_thread_task_runner)),
      dump_on_hang_(dump_on_hang),
      timer_(clock_) {
  DETACH_FROM_SEQUENCE(monitor_sequence_);
}

void AudioThreadHangMonitor::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_);

  // Set the flag to true so that the first run doesn't detect a hang.
  alive_flag_->flag_ = true;

  last_check_time_ = clock_->NowTicks();

  LogHistogramThreadStatus();

  // |this| owns |timer_|, so Unretained is safe.
  timer_.Start(
      FROM_HERE, kHungDeadline,
      base::BindRepeating(&AudioThreadHangMonitor::CheckIfAudioThreadIsAlive,
                          base::Unretained(this)));
}

bool AudioThreadHangMonitor::NeverLoggedThreadHung() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_);
  return audio_thread_status_ == ThreadStatus::kStarted;
}

bool AudioThreadHangMonitor::NeverLoggedThreadRecoveredAfterHung() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_);
  return audio_thread_status_ == ThreadStatus::kHung;
}

void AudioThreadHangMonitor::CheckIfAudioThreadIsAlive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_);

  const base::TimeDelta time_since_last_check =
      clock_->NowTicks() - last_check_time_;

  // An unexpected |time_since_last_check| may indicate that the system has been
  // in sleep mode, in which case the audio thread may have had insufficient
  // time to respond to the ping. In such a case, skip the check for now.
  if (time_since_last_check > kHungDeadline + base::TimeDelta::FromSeconds(1))
    return;

  const bool audio_thread_responded_to_last_ping = alive_flag_->flag_;
  if (audio_thread_responded_to_last_ping) {
    recent_ping_state_ = std::max(recent_ping_state_, 0) + 1;

    // Update the thread status if it was previously hung. Will only log
    // "recovered" once for the lifetime of this object.
    if (NeverLoggedThreadRecoveredAfterHung() &&
        recent_ping_state_ >= kMaxFailedPingsCount) {
      // Require just as many successful pings to recover from failure.
      audio_thread_status_ = ThreadStatus::kRecovered;
      LogHistogramThreadStatus();
    }
  } else {
    recent_ping_state_ = std::min(recent_ping_state_, 0) - 1;

    // Update the thread status if it was previously live and has never been
    // considered hung before. Will only log "hung" once for the lifetime of
    // this object.
    if (-recent_ping_state_ >= kMaxFailedPingsCount &&
        NeverLoggedThreadHung()) {
      audio_thread_status_ = ThreadStatus::kHung;
      LogHistogramThreadStatus();

      if (dump_on_hang_) {
        base::debug::DumpWithoutCrashing();
      }
    }
  }

  alive_flag_->flag_ = false;
  last_check_time_ = clock_->NowTicks();
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<SharedAtomicFlag> flag) { flag->flag_ = true; },
          alive_flag_));
}

void AudioThreadHangMonitor::LogHistogramThreadStatus() {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioThreadStatus",
                            audio_thread_status_.load());
}

}  // namespace media
