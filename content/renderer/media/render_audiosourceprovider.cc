// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_audiosourceprovider.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/renderer/media/audio_device_factory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProviderClient.h"

using content::AudioDeviceFactory;
using std::vector;
using WebKit::WebVector;

RenderAudioSourceProvider::RenderAudioSourceProvider()
    : is_initialized_(false),
      channels_(0),
      sample_rate_(0),
      is_running_(false),
      volume_(1.0),
      renderer_(NULL),
      client_(NULL) {
  // We create the AudioDevice here using the factory. But we don't yet know
  // the audio format (sample-rate, etc.) at this point.  Later, when
  // Initialize() is called, we have the audio format information and call
  // the AudioDevice::Initialize() method to fully initialize it.
  default_sink_ = AudioDeviceFactory::Create();
}

void RenderAudioSourceProvider::setClient(
    WebKit::WebAudioSourceProviderClient* client) {
  // Synchronize with other uses of client_ and default_sink_.
  base::AutoLock auto_lock(sink_lock_);

  if (client && client != client_) {
    // Detach the audio renderer from normal playback.
    default_sink_->Stop();

    // The client will now take control by calling provideInput() periodically.
    client_ = client;

    if (is_initialized_) {
      // The client needs to be notified of the audio format, if available.
      // If the format is not yet available, we'll be notified later
      // when Initialize() is called.

      // Inform WebKit about the audio stream format.
      client->setFormat(channels_, sample_rate_);
    }
  } else if (!client && client_) {
    // Restore normal playback.
    client_ = NULL;
    // TODO(crogers): We should call default_sink_->Play() if we're
    // in the playing state.
  }
}

void RenderAudioSourceProvider::provideInput(
    const WebVector<float*>& audio_data, size_t number_of_frames) {
  DCHECK(client_);

  if (renderer_ && is_initialized_ && is_running_) {
    // Wrap WebVector as std::vector.
    vector<float*> v(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i)
      v[i] = audio_data[i];

    // TODO(crogers): figure out if we should volume scale here or in common
    // WebAudio code.  In any case we need to take care of volume.
    renderer_->Render(v, number_of_frames, 0);
  } else {
    // Provide silence if the source is not running.
    for (size_t i = 0; i < audio_data.size(); ++i)
      memset(audio_data[i], 0, sizeof(float) * number_of_frames);
  }
}

void RenderAudioSourceProvider::Start() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->Start();
  is_running_ = true;
}

void RenderAudioSourceProvider::Stop() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->Stop();
  is_running_ = false;
}

void RenderAudioSourceProvider::Play() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->Play();
  is_running_ = true;
}

void RenderAudioSourceProvider::Pause(bool flush) {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->Pause(flush);
  is_running_ = false;
}

void RenderAudioSourceProvider::SetPlaybackRate(float rate) {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->SetPlaybackRate(rate);
}

bool RenderAudioSourceProvider::SetVolume(double volume) {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->SetVolume(volume);
  volume_ = volume;
  return true;
}

void RenderAudioSourceProvider::GetVolume(double* volume) {
  if (!client_)
    default_sink_->GetVolume(volume);
  else if (volume)
    *volume = volume_;
}

void RenderAudioSourceProvider::Initialize(
    const media::AudioParameters& params,
    RenderCallback* renderer) {
  base::AutoLock auto_lock(sink_lock_);
  CHECK(!is_initialized_);
  renderer_ = renderer;

  default_sink_->Initialize(params, renderer);

  // Keep track of the format in case the client hasn't yet been set.
  channels_ = params.channels();
  sample_rate_ = params.sample_rate();

  if (client_) {
    // Inform WebKit about the audio stream format.
    client_->setFormat(channels_, sample_rate_);
  }

  is_initialized_ = true;
}

RenderAudioSourceProvider::~RenderAudioSourceProvider() {}
