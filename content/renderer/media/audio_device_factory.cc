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

scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  AudioMessageFilter* const filter = AudioMessageFilter::Get();
  scoped_refptr<media::AudioOutputDevice> device(new media::AudioOutputDevice(
      filter->CreateAudioOutputIPC(render_frame_id), filter->io_task_runner(),
      session_id, device_id, security_origin));
  device->RequestDeviceAuthorization();
  return device;
}

// This is where we decide which audio will go to mixers and which one to
// AudioOutpuDevice directly.
bool IsMixable(AudioDeviceFactory::SourceType source_type) {
  if (source_type == AudioDeviceFactory::kSourceMediaElement)
    return true;  // Must ALWAYS go through mixer.

  // TODO(olka): make a decision for the rest of the sources basing on OS
  // type and configuration parameters.
  return false;
}

scoped_refptr<media::SwitchableAudioRendererSink> NewMixableSink(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  return scoped_refptr<media::AudioRendererMixerInput>(
      render_thread->GetAudioRendererMixerManager()->CreateInput(
          render_frame_id, session_id, device_id, security_origin));
}

}  // namespace

scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererMixerSink(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  return NewFinalAudioRendererSink(render_frame_id, session_id, device_id,
                                   security_origin);
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererSink(SourceType source_type,
                                         int render_frame_id,
                                         int session_id,
                                         const std::string& device_id,
                                         const url::Origin& security_origin) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> device =
        factory_->CreateAudioRendererSink(source_type, render_frame_id,
                                          session_id, device_id,
                                          security_origin);
    if (device)
      return device;
  }

  if (IsMixable(source_type))
    return NewMixableSink(render_frame_id, session_id, device_id,
                          security_origin);

  return NewFinalAudioRendererSink(render_frame_id, session_id, device_id,
                                   security_origin);
}

// static
scoped_refptr<media::SwitchableAudioRendererSink>
AudioDeviceFactory::NewSwitchableAudioRendererSink(
    SourceType source_type,
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    scoped_refptr<media::SwitchableAudioRendererSink> sink =
        factory_->CreateSwitchableAudioRendererSink(source_type,
                                                    render_frame_id, session_id,
                                                    device_id, security_origin);
    if (sink)
      return sink;
  }

  if (IsMixable(source_type))
    return NewMixableSink(render_frame_id, session_id, device_id,
                          security_origin);

  // AudioOutputDevice is not RestartableAudioRendererSink, so we can't return
  // anything for those who wants to create an unmixable sink.
  NOTIMPLEMENTED();
  return nullptr;
}

// static
scoped_refptr<media::AudioCapturerSource>
AudioDeviceFactory::NewAudioCapturerSource(int render_frame_id) {
  if (factory_) {
    scoped_refptr<media::AudioCapturerSource> source =
        factory_->CreateAudioCapturerSource(render_frame_id);
    if (source)
      return source;
  }

  AudioInputMessageFilter* const filter = AudioInputMessageFilter::Get();
  return new media::AudioInputDevice(
      filter->CreateAudioInputIPC(render_frame_id), filter->io_task_runner());
}

// static
media::OutputDeviceInfo AudioDeviceFactory::GetOutputDeviceInfo(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread) << "RenderThreadImpl is not instantiated, or "
                        << "GetOutputDeviceInfo() is called on a wrong thread ";
  return render_thread->GetAudioRendererMixerManager()->GetOutputDeviceInfo(
      render_frame_id, session_id, device_id, security_origin);
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = NULL;
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewFinalAudioRendererSink(
    int render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> sink =
        factory_->CreateFinalAudioRendererSink(render_frame_id, session_id,
                                               device_id, security_origin);
    if (sink)
      return sink;
  }

  return NewOutputDevice(render_frame_id, session_id, device_id,
                         security_origin);
}

}  // namespace content
