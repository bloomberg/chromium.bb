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
  virtual bool Initialize(SoundKey key,
                          const base::StringPiece& data) OVERRIDE;
  virtual bool Play(SoundKey key) OVERRIDE;
  virtual base::TimeDelta GetDuration(SoundKey key) OVERRIDE;

 private:
  base::hash_map<SoundKey, linked_ptr<AudioStreamHandler> > handlers_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(SoundsManagerImpl);
};

SoundsManagerImpl::SoundsManagerImpl()
    : message_loop_(AudioManager::Get()->GetMessageLoop()) {}

SoundsManagerImpl::~SoundsManagerImpl() { DCHECK(CalledOnValidThread()); }

bool SoundsManagerImpl::Initialize(SoundKey key,
                                   const base::StringPiece& data) {
  if (handlers_.find(key) != handlers_.end() && handlers_[key]->IsInitialized())
    return true;
  linked_ptr<AudioStreamHandler> handler(new AudioStreamHandler(data));
  if (!handler->IsInitialized()) {
    LOG(WARNING) << "Can't initialize AudioStreamHandler for key=" << key;
    return false;
  }
  handlers_[key] = handler;
  return true;
}

bool SoundsManagerImpl::Play(SoundKey key) {
  DCHECK(CalledOnValidThread());
  if (handlers_.find(key) == handlers_.end() ||
      !handlers_[key]->IsInitialized()) {
    return false;
  }
  return handlers_[key]->Play();
}

base::TimeDelta SoundsManagerImpl::GetDuration(SoundKey key) {
  DCHECK(CalledOnValidThread());
  if (handlers_.find(key) == handlers_.end() ||
      !handlers_[key]->IsInitialized()) {
    return base::TimeDelta();
  }
  const WavAudioHandler& wav_audio = handlers_[key]->wav_audio_handler();
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
  virtual bool Initialize(SoundKey key,
                          const base::StringPiece& data) OVERRIDE;
  virtual bool Play(SoundKey key) OVERRIDE;
  virtual base::TimeDelta GetDuration(SoundKey key) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SoundsManagerStub);
};

SoundsManagerStub::SoundsManagerStub() {}

SoundsManagerStub::~SoundsManagerStub() { DCHECK(CalledOnValidThread()); }

bool SoundsManagerStub::Initialize(SoundKey /* key */,
                                   const base::StringPiece& /* data */) {
  DCHECK(CalledOnValidThread());
  return false;
}

bool SoundsManagerStub::Play(SoundKey /* key */) {
  DCHECK(CalledOnValidThread());
  return false;
}

base::TimeDelta SoundsManagerStub::GetDuration(SoundKey /* key */) {
  DCHECK(CalledOnValidThread());
  return base::TimeDelta();
}

}  // namespace

SoundsManager::SoundsManager() {}

SoundsManager::~SoundsManager() { DCHECK(CalledOnValidThread()); }

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
