// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_input_stream_broker.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "media/audio/audio_logging.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if defined(OS_CHROMEOS)
#include "content/browser/media/keyboard_mic_registration.h"
#endif

namespace content {

AudioInputStreamBroker::AudioInputStreamBroker(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool enable_agc,
    AudioStreamBroker::DeleterCallback deleter,
    mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client)
    : AudioStreamBroker(render_process_id, render_frame_id),
      device_id_(device_id),
      params_(params),
      shared_memory_count_(shared_memory_count),
      enable_agc_(enable_agc),
      deleter_(std::move(deleter)),
      renderer_factory_client_(std::move(renderer_factory_client)),
      observer_binding_(this),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(renderer_factory_client_);
  DCHECK(deleter_);

  // Unretained is safe because |this| owns |renderer_factory_client_|.
  renderer_factory_client_.set_connection_error_handler(
      base::BindOnce(&AudioInputStreamBroker::Cleanup, base::Unretained(this)));

  // Notify RenderProcessHost about input stream so the renderer is not
  // background.
  auto* process_host = RenderProcessHost::FromID(render_process_id);
  if (process_host)
    process_host->OnMediaStreamAdded();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    params_.set_format(media::AudioParameters::AUDIO_FAKE);
  }

  BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
  // May be null in unit tests.
  if (!browser_main_loop)
    return;

#if defined(OS_CHROMEOS)
  if (params_.channel_layout() ==
      media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
      browser_main_loop->keyboard_mic_registration()->Register();
  }
#else
  user_input_monitor_ = static_cast<media::UserInputMonitorBase*>(
      browser_main_loop->user_input_monitor());
#endif
}

AudioInputStreamBroker::~AudioInputStreamBroker() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_CHROMEOS)
  if (params_.channel_layout() ==
      media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();

    // May be null in unit tests.
    if (browser_main_loop)
      browser_main_loop->keyboard_mic_registration()->Deregister();
  }
#else
  // Check that DisableKeyPressMonitoring() was called.
  DCHECK(!user_input_monitor_);
#endif

  auto* process_host = RenderProcessHost::FromID(render_process_id());
  if (process_host)
    process_host->OnMediaStreamRemoved();

  // TODO(https://crbug.com/829317) update tab recording indicator.
}

void AudioInputStreamBroker::CreateStream(
    audio::mojom::StreamFactory* factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!observer_binding_.is_bound());
  DCHECK(!client_request_);

  base::ReadOnlySharedMemoryRegion key_press_count_buffer;
  if (user_input_monitor_) {
    key_press_count_buffer =
        user_input_monitor_->EnableKeyPressMonitoringWithMapping();
  }

  media::mojom::AudioInputStreamClientPtr client;
  client_request_ = mojo::MakeRequest(&client);

  media::mojom::AudioInputStreamPtr stream;
  media::mojom::AudioInputStreamRequest stream_request =
      mojo::MakeRequest(&stream);

  media::mojom::AudioInputStreamObserverPtr observer_ptr;
  observer_binding_.Bind(mojo::MakeRequest(&observer_ptr));

  // Unretained is safe because |this| owns |observer_binding_|.
  observer_binding_.set_connection_error_handler(
      base::BindOnce(&AudioInputStreamBroker::Cleanup, base::Unretained(this)));

  // Note that the component id for AudioLog is used to differentiate between
  // several users of the same audio log. Since this audio log is for a single
  // stream, the component id used doesn't matter.
  // TODO(https://crbug.com/836226) pass valid user input monitor handle when
  // switching to audio service input streams.
  constexpr int log_component_id = 0;
  factory->CreateInputStream(
      std::move(stream_request), std::move(client), std::move(observer_ptr),
      MediaInternals::GetInstance()->CreateMojoAudioLog(
          media::AudioLogFactory::AudioComponent::AUDIO_INPUT_CONTROLLER,
          log_component_id, render_process_id(), render_frame_id()),
      device_id_, params_, shared_memory_count_, enable_agc_,
      mojo::WrapReadOnlySharedMemoryRegion(std::move(key_press_count_buffer)),
      base::BindOnce(&AudioInputStreamBroker::StreamCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(stream)));
}

void AudioInputStreamBroker::DidStartRecording() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(https://crbug.com/829317) update tab recording indicator.
}

void AudioInputStreamBroker::StreamCreated(
    media::mojom::AudioInputStreamPtr stream,
    media::mojom::AudioDataPipePtr data_pipe,
    bool initially_muted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!data_pipe) {
    Cleanup();
    return;
  }

  DCHECK(renderer_factory_client_);
  renderer_factory_client_->StreamCreated(
      std::move(stream), std::move(client_request_), std::move(data_pipe),
      initially_muted);
}

void AudioInputStreamBroker::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (user_input_monitor_) {
    user_input_monitor_->DisableKeyPressMonitoring();

    // Set to nullptr to check that DisableKeyPressMonitoring() was called
    // before destructor
    user_input_monitor_ = nullptr;
  }

  std::move(deleter_).Run(this);
}

}  // namespace content
