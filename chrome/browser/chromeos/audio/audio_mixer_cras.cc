// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/audio/audio_mixer_cras.h"

#include <cmath>
#include <cras_client.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Default volume as a percentage in the range [0.0, 100.0].
const double kDefaultVolumePercent = 75.0;

// Number of seconds that we'll sleep between each connection attempt.
const int kConnectionRetrySleepSec = 1;

}  // namespace

AudioMixerCras::AudioMixerCras()
    : client_(NULL),
      client_connected_(false),
      volume_percent_(kDefaultVolumePercent),
      is_muted_(false),
      is_mute_locked_(false),
      is_capture_muted_(false),
      is_capture_mute_locked_(false),
      apply_is_pending_(true) {
}

AudioMixerCras::~AudioMixerCras() {
  if (!thread_.get())
    return;
  DCHECK(MessageLoop::current() != thread_->message_loop());

  base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
  thread_->Stop();
  thread_.reset();

  cras_client_destroy(client_);
}

void AudioMixerCras::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!thread_.get()) << "Init() called twice";

  thread_.reset(new base::Thread("AudioMixerCras"));
  CHECK(thread_->Start());
  thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&AudioMixerCras::Connect, base::Unretained(this)));
}

double AudioMixerCras::GetVolumePercent() {
  base::AutoLock lock(lock_);
  return volume_percent_;
}

void AudioMixerCras::SetVolumePercent(double percent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (isnan(percent))
    percent = 0.0;
  percent = std::max(std::min(percent, 100.0), 0.0);

  base::AutoLock lock(lock_);
  volume_percent_ = percent;
  if (client_connected_ && !apply_is_pending_)
    thread_->message_loop()->PostTask(FROM_HERE,
        base::Bind(&AudioMixerCras::ApplyState, base::Unretained(this)));
}

bool AudioMixerCras::IsMuted() {
  base::AutoLock lock(lock_);
  return is_muted_;
}

void AudioMixerCras::SetMuted(bool mute) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  if (is_mute_locked_) {
    NOTREACHED() << "Mute has been locked!";
    return;
  }
  is_muted_ = mute;
  ApplyStateIfNeeded();
}

bool AudioMixerCras::IsMuteLocked() {
  base::AutoLock lock(lock_);
  return is_mute_locked_;
}

void AudioMixerCras::SetMuteLocked(bool locked) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  is_mute_locked_ = locked;
  ApplyStateIfNeeded();
}

bool AudioMixerCras::IsCaptureMuted() {
  base::AutoLock lock(lock_);
  return is_capture_muted_;
}

void AudioMixerCras::SetCaptureMuted(bool mute) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  if (is_capture_mute_locked_) {
    NOTREACHED() << "Capture mute has been locked!";
    return;
  }
  is_capture_muted_ = mute;
  ApplyStateIfNeeded();
}

bool AudioMixerCras::IsCaptureMuteLocked() {
  base::AutoLock lock(lock_);
  return is_capture_mute_locked_;
}

void AudioMixerCras::SetCaptureMuteLocked(bool locked) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  is_capture_mute_locked_ = locked;
  ApplyStateIfNeeded();
}

void AudioMixerCras::Connect() {
  DCHECK(MessageLoop::current() == thread_->message_loop());

  // Create the client structure.
  if (client_ == NULL && cras_client_create(&client_) < 0) {
    LOG(DFATAL) << "cras_client_create() failed";
    return; // TODO(dgreid) change interface so this can return an error.
  }

  if (cras_client_connect(client_) != 0) {
    thread_->message_loop()->PostDelayedTask(FROM_HERE,
        base::Bind(&AudioMixerCras::Connect, base::Unretained(this)),
        base::TimeDelta::FromSeconds(kConnectionRetrySleepSec));
    return;
  }
  client_connected_ = true;

  ApplyState();
}

void AudioMixerCras::ApplyState() {
  DCHECK(MessageLoop::current() == thread_->message_loop());
  if (!client_connected_)
    return;

  bool should_mute = false;
  bool should_lock_mute = false;
  bool should_mute_capture = false;
  bool should_lock_capture_mute = false;
  size_t new_volume = 0;
  {
    base::AutoLock lock(lock_);
    should_mute = is_muted_;
    should_lock_mute = is_mute_locked_;
    should_mute_capture = is_capture_muted_;
    should_lock_capture_mute = is_capture_mute_locked_;
    new_volume = floor(volume_percent_ + 0.5);
    apply_is_pending_ = false;
  }

  // If muting mute before setting volume, if un-muting set volume first.
  if (should_mute) {
    cras_client_set_system_mute(client_, should_mute);
    cras_client_set_system_volume(client_, new_volume);
  } else {
    cras_client_set_system_volume(client_, new_volume);
    cras_client_set_system_mute(client_, should_mute);
  }
  cras_client_set_system_mute_locked(client_, should_lock_mute);
  cras_client_set_system_capture_mute(client_, should_mute_capture);
  cras_client_set_system_capture_mute_locked(client_, should_lock_capture_mute);
}

void AudioMixerCras::ApplyStateIfNeeded() {
  lock_.AssertAcquired();
  if (client_connected_ && !apply_is_pending_) {
    thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AudioMixerCras::ApplyState, base::Unretained(this)));
  }
}

}  // namespace chromeos
