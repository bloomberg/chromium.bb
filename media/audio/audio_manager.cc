// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor.h"
#include "build/build_config.h"
#include "media/audio/fake_audio_log_factory.h"

namespace media {
namespace {
AudioManager* g_last_created = NULL;

// Helper class for managing global AudioManager data and hang timers. If the
// audio thread is unresponsive for more than a minute we want to crash the
// process so we can catch offenders quickly in the field.
class AudioManagerHelper : public base::PowerObserver {
 public:
  AudioManagerHelper()
      : max_hung_task_time_(base::TimeDelta::FromMinutes(1)),
        hang_detection_enabled_(true) {}
  ~AudioManagerHelper() override {}

  void StartHangTimer(
      const scoped_refptr<base::SingleThreadTaskRunner>& monitor_task_runner) {
    CHECK(!monitor_task_runner_);
    monitor_task_runner_ = monitor_task_runner;
    base::PowerMonitor::Get()->AddObserver(this);
    UpdateLastAudioThreadTimeTick();
    CrashOnAudioThreadHang();
  }

  // Disable hang detection when the system goes into the suspend state.
  void OnSuspend() override {
    base::AutoLock lock(hang_lock_);
    hang_detection_enabled_ = false;
  }

  // Reenable hang detection once the system comes out of the suspend state.
  void OnResume() override {
    base::AutoLock lock(hang_lock_);
    hang_detection_enabled_ = true;
    last_audio_thread_timer_tick_ = base::TimeTicks::Now();
  }

  // Runs on |monitor_task_runner| typically, but may be started on any thread.
  void CrashOnAudioThreadHang() {
    base::AutoLock lock(hang_lock_);

    // Don't attempt to verify the tick time if the system is in the process of
    // suspending or resuming.
    if (hang_detection_enabled_) {
      CHECK(base::TimeTicks::Now() - last_audio_thread_timer_tick_ <=
            max_hung_task_time_);
    }

    monitor_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&AudioManagerHelper::CrashOnAudioThreadHang,
                              base::Unretained(this)),
        max_hung_task_time_);
  }

  // Runs on the audio thread typically, but may be started on any thread.
  void UpdateLastAudioThreadTimeTick() {
    base::AutoLock lock(hang_lock_);
    last_audio_thread_timer_tick_ = base::TimeTicks::Now();
    g_last_created->GetTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                   base::Unretained(this)),
        max_hung_task_time_ / 2);
  }

  AudioLogFactory* fake_log_factory() { return &fake_log_factory_; }

 private:
  FakeAudioLogFactory fake_log_factory_;

  const base::TimeDelta max_hung_task_time_;
  scoped_refptr<base::SingleThreadTaskRunner> monitor_task_runner_;

  base::Lock hang_lock_;
  bool hang_detection_enabled_;
  base::TimeTicks last_audio_thread_timer_tick_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerHelper);
};

static base::LazyInstance<AudioManagerHelper>::Leaky g_helper =
    LAZY_INSTANCE_INITIALIZER;
}

// Forward declaration of the platform specific AudioManager factory function.
AudioManager* CreateAudioManager(AudioLogFactory* audio_log_factory);

AudioManager::AudioManager() {}

AudioManager::~AudioManager() {
  CHECK(!g_last_created || g_last_created == this);
  g_last_created = NULL;
}

// static
AudioManager* AudioManager::Create(AudioLogFactory* audio_log_factory) {
  CHECK(!g_last_created);
  g_last_created = CreateAudioManager(audio_log_factory);
  return g_last_created;
}

// static
AudioManager* AudioManager::CreateWithHangTimer(
    AudioLogFactory* audio_log_factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& monitor_task_runner) {
  AudioManager* manager = Create(audio_log_factory);
  // On OSX the audio thread is the UI thread, for which a hang monitor is not
  // necessary.
#if !defined(OS_MACOSX)
  g_helper.Pointer()->StartHangTimer(monitor_task_runner);
#endif
  return manager;
}

// static
AudioManager* AudioManager::CreateForTesting() {
  return Create(g_helper.Pointer()->fake_log_factory());
}

// static
AudioManager* AudioManager::Get() {
  return g_last_created;
}

}  // namespace media
