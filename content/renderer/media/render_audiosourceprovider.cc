// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_audiosourceprovider.h"

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/media_switches.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProviderClient.h"

using std::vector;
using WebKit::WebVector;

namespace content {

RenderAudioSourceProvider::RenderAudioSourceProvider(int source_render_view_id)
    : is_initialized_(false),
      channels_(0),
      sample_rate_(0),
      is_running_(false),
      renderer_(NULL),
      client_(NULL) {
  // We create an AudioRendererSink here, but we don't yet know the audio format
  // (sample-rate, etc.) at this point.  Later, when Initialize() is called, we
  // have the audio format information and call AudioRendererSink::Initialize()
  // to fully initialize it.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
#if defined(OS_WIN) || defined(OS_MAC)
  const bool use_mixing =
      !cmd_line->HasSwitch(switches::kDisableRendererSideMixing);
#else
  const bool use_mixing =
      cmd_line->HasSwitch(switches::kEnableRendererSideMixing);
#endif

  if (use_mixing) {
    default_sink_ = RenderThreadImpl::current()->
        GetAudioRendererMixerManager()->CreateInput();
    // TODO(miu): Partition mixer instances per RenderView.
  } else {
    scoped_refptr<RendererAudioOutputDevice> device =
        AudioDeviceFactory::NewOutputDevice();
    // The RenderView creating RenderAudioSourceProvider will be the source of
    // the audio (WebMediaPlayer is always associated with a document in a frame
    // at the time RenderAudioSourceProvider is instantiated).
    device->SetSourceRenderView(source_render_view_id);
    default_sink_ = device;
  }
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

    scoped_ptr<media::AudioBus> audio_bus = media::AudioBus::WrapVector(
        number_of_frames, v);

    // TODO(crogers): figure out if we should volume scale here or in common
    // WebAudio code.  In any case we need to take care of volume.
    renderer_->Render(audio_bus.get(), 0);
  } else {
    // Provide silence if the source is not running.
    for (size_t i = 0; i < audio_data.size(); ++i)
      memset(audio_data[i], 0, sizeof(*audio_data[0]) * number_of_frames);
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

bool RenderAudioSourceProvider::SetVolume(double volume) {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    default_sink_->SetVolume(volume);
  return true;
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

}  // namespace content
