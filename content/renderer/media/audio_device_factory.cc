// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_device_factory.h"

#include "base/logging.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "url/origin.h"

namespace content {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = NULL;

namespace {

// This is where we decide which audio will go to mixers and which one to
// AudioOutpuDevice directly.
bool IsMixable(AudioDeviceFactory::SourceType source_type) {
  if (source_type == AudioDeviceFactory::kSourceMediaElement)
    return true;  // Must ALWAYS go through mixer.

  // TODO(olka): make a decision for the rest of the sources basing on OS
  // type and configuration parameters.
  return false;
}

scoped_refptr<media::RestartableAudioRendererSink> NewMixableSink(
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  return scoped_refptr<media::AudioRendererMixerInput>(
      render_thread->GetAudioRendererMixerManager()->CreateInput(
          render_frame_id, device_id, security_origin));
}

scoped_refptr<media::AudioRendererSink> NewUnmixableSink(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  return AudioDeviceFactory::NewOutputDevice(render_frame_id, session_id,
                                             device_id, security_origin);
}

}  // namespace

// static
scoped_refptr<media::AudioOutputDevice> AudioDeviceFactory::NewOutputDevice(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    media::AudioOutputDevice* const device = factory_->CreateOutputDevice(
        render_frame_id, session_id, device_id, security_origin);
    if (device)
      return device;
  }

  AudioMessageFilter* const filter = AudioMessageFilter::Get();
  scoped_refptr<media::AudioOutputDevice> device = new media::AudioOutputDevice(
      filter->CreateAudioOutputIPC(render_frame_id), filter->io_task_runner(),
      session_id, device_id, security_origin);
  device->RequestDeviceAuthorization();
  return device;
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererSink(SourceType source_type,
                                         int render_frame_id,
                                         int session_id,
                                         const std::string& device_id,
                                         const url::Origin& security_origin) {
  if (factory_) {
    media::AudioRendererSink* const device = factory_->CreateAudioRendererSink(
        source_type, render_frame_id, session_id, device_id, security_origin);
    if (device)
      return device;
  }

  if (IsMixable(source_type))
    return NewMixableSink(render_frame_id, device_id, security_origin);

  return NewUnmixableSink(render_frame_id, session_id, device_id,
                          security_origin);
}

// static
scoped_refptr<media::RestartableAudioRendererSink>
AudioDeviceFactory::NewRestartableAudioRendererSink(
    SourceType source_type,
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    media::RestartableAudioRendererSink* const device =
        factory_->CreateRestartableAudioRendererSink(
            source_type, render_frame_id, session_id, device_id,
            security_origin);
    if (device)
      return device;
  }

  if (IsMixable(source_type))
    return NewMixableSink(render_frame_id, device_id, security_origin);

  // AudioOutputDevice is not RestartableAudioRendererSink, so we can't return
  // anything for those who wants to create an unmixable sink.
  NOTIMPLEMENTED();
  return nullptr;
}

// static
scoped_refptr<media::AudioInputDevice> AudioDeviceFactory::NewInputDevice(
    int render_frame_id) {
  if (factory_) {
    media::AudioInputDevice* const device =
        factory_->CreateInputDevice(render_frame_id);
    if (device)
      return device;
  }

  AudioInputMessageFilter* const filter = AudioInputMessageFilter::Get();
  return new media::AudioInputDevice(
      filter->CreateAudioInputIPC(render_frame_id), filter->io_task_runner());
}

// static
// TODO(http://crbug.com/587461): Find a better way to check if device exists
// and is authorized.
media::OutputDeviceStatus AudioDeviceFactory::GetOutputDeviceStatus(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  scoped_refptr<media::AudioOutputDevice> device =
      NewOutputDevice(render_frame_id, session_id, device_id, security_origin);
  media::OutputDeviceStatus status = device->GetDeviceStatus();

  device->Stop();  // Must be stopped.
  return status;
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = NULL;
}

}  // namespace content
