// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sounds/sounds_manager.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/audio/audio_manager.h"
#include "media/audio/sounds/audio_stream_handler.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

SoundsManager* g_instance = NULL;

// SoundsManagerImpl ---------------------------------------------------

class SoundsManagerImpl : public SoundsManager {
 public:
  SoundsManagerImpl();
  virtual ~SoundsManagerImpl();

  // SoundsManager implementation:
  virtual bool Initialize(
      const std::vector<base::StringPiece>& resources) OVERRIDE;
  virtual bool Play(Sound sound) OVERRIDE;
  virtual base::TimeDelta GetDuration(Sound sound) OVERRIDE;

 private:
  std::vector<linked_ptr<AudioStreamHandler> > handlers_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(SoundsManagerImpl);
};

SoundsManagerImpl::SoundsManagerImpl()
    : handlers_(SOUND_COUNT),
      message_loop_(AudioManager::Get()->GetMessageLoop()) {
}

SoundsManagerImpl::~SoundsManagerImpl() {
  DCHECK(CalledOnValidThread());
}

bool SoundsManagerImpl::Initialize(
    const std::vector<base::StringPiece>& resources) {
  if (resources.size() != static_cast<size_t>(SOUND_COUNT)) {
    LOG(ERROR) << "Incorrect num of sounds.";
    return false;
  }
  for (size_t i = 0; i < resources.size(); ++i) {
    handlers_[i].reset(new AudioStreamHandler(resources[i]));
    if (!handlers_[i]->IsInitialized()) {
      LOG(WARNING) << "Can't initialize AudioStreamHandler for sound "
                   << i << ".";
      return false;
    }
  }
  return true;
}

bool SoundsManagerImpl::Play(Sound sound) {
  DCHECK(CalledOnValidThread());
  DCHECK(sound < SOUND_COUNT);
  if (!handlers_[sound].get() || !handlers_[sound]->IsInitialized())
    return false;
  return handlers_[sound]->Play();
}

base::TimeDelta SoundsManagerImpl::GetDuration(Sound sound) {
  DCHECK(CalledOnValidThread());
  if (sound >= SOUND_COUNT ||
      !handlers_[sound].get() ||
      !handlers_[sound]->IsInitialized()) {
    return base::TimeDelta();
  }
  const WavAudioHandler& wav_audio = handlers_[sound]->wav_audio_handler();
  const int64 size = wav_audio.size();
  const int64 rate = wav_audio.byte_rate();
  return base::TimeDelta::FromMicroseconds(size * 1000000 / rate);
}

// SoundsManagerStub ---------------------------------------------------

class SoundsManagerStub : public SoundsManager {
 public:
  SoundsManagerStub();
  virtual ~SoundsManagerStub();

  // SoundsManager implementation:
  virtual bool Initialize(
      const std::vector<base::StringPiece>& resources) OVERRIDE;
  virtual bool Play(Sound sound) OVERRIDE;
  virtual base::TimeDelta GetDuration(Sound sound) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SoundsManagerStub);
};

SoundsManagerStub::SoundsManagerStub() {
}

SoundsManagerStub::~SoundsManagerStub() {
  DCHECK(CalledOnValidThread());
}

bool SoundsManagerStub::Initialize(
    const std::vector<base::StringPiece>& /* resources */) {
  DCHECK(CalledOnValidThread());
  return false;
}

bool SoundsManagerStub::Play(Sound /* sound */) {
  DCHECK(CalledOnValidThread());
  return false;
}

base::TimeDelta SoundsManagerStub::GetDuration(Sound /* sound */) {
  DCHECK(CalledOnValidThread());
  return base::TimeDelta();
}

}  // namespace

SoundsManager::SoundsManager() {
}

SoundsManager::~SoundsManager() {
  DCHECK(CalledOnValidThread());
}

// static
void SoundsManager::Create() {
  CHECK(!g_instance) << "SoundsManager::Create() is called twice";
  const bool enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kDisableSystemSoundsManager);
  if (enabled)
    g_instance = new SoundsManagerImpl();
  else
    g_instance = new SoundsManagerStub();
}

// static
void SoundsManager::Shutdown() {
  CHECK(g_instance) << "SoundsManager::Shutdown() is called "
                    << "without previous call to Create()";
  delete g_instance;
  g_instance = NULL;
}

// static
SoundsManager* SoundsManager::Get() {
  CHECK(g_instance) << "SoundsManager::Get() is called before Create()";
  return g_instance;
}

}  // namespace media
