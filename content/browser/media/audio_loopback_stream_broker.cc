// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_loopback_stream_broker.h"

#include <utility>

#include "base/unguessable_token.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

AudioStreamBrokerFactory::LoopbackSource::LoopbackSource() = default;

AudioStreamBrokerFactory::LoopbackSource::LoopbackSource(
    WebContents* source_contents)
    : WebContentsObserver(source_contents) {
  DCHECK(source_contents);
}

AudioStreamBrokerFactory::LoopbackSource::~LoopbackSource() = default;

base::UnguessableToken AudioStreamBrokerFactory::LoopbackSource::GetGroupID() {
  if (WebContentsImpl* source_contents =
          static_cast<WebContentsImpl*>(web_contents())) {
    return source_contents->GetAudioStreamFactory()->group_id();
  }
  return base::UnguessableToken();
}

void AudioStreamBrokerFactory::LoopbackSource::OnStartCapturing() {
  if (WebContentsImpl* source_contents =
          static_cast<WebContentsImpl*>(web_contents())) {
    source_contents->IncrementCapturerCount(gfx::Size());
  }
}

void AudioStreamBrokerFactory::LoopbackSource::OnStopCapturing() {
  if (WebContentsImpl* source_contents =
          static_cast<WebContentsImpl*>(web_contents())) {
    source_contents->DecrementCapturerCount();
  }
}

void AudioStreamBrokerFactory::LoopbackSource::WebContentsDestroyed() {
  if (on_gone_closure_)
    std::move(on_gone_closure_).Run();
}

AudioLoopbackStreamBroker::AudioLoopbackStreamBroker(
    int render_process_id,
    int render_frame_id,
    std::unique_ptr<AudioStreamBrokerFactory::LoopbackSource> source,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool mute_source,
    AudioStreamBroker::DeleterCallback deleter,
    mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client)
    : AudioStreamBroker(render_process_id, render_frame_id),
      source_(std::move(source)),
      params_(params),
      shared_memory_count_(shared_memory_count),
      deleter_(std::move(deleter)),
      renderer_factory_client_(std::move(renderer_factory_client)),
      observer_binding_(this),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(source_);
  DCHECK(source_->GetGroupID());
  DCHECK(renderer_factory_client_);
  DCHECK(deleter_);

  // Unretained is safe because |this| owns |source_|.
  source_->set_on_gone_closure(base::BindOnce(
      &AudioLoopbackStreamBroker::Cleanup, base::Unretained(this)));

  if (mute_source) {
    muter_.emplace(source_->GetGroupID());
  }

  // Unretained is safe because |this| owns |renderer_factory_client_|.
  renderer_factory_client_.set_connection_error_handler(base::BindOnce(
      &AudioLoopbackStreamBroker::Cleanup, base::Unretained(this)));

  // Notify the source that we are capturing from it, to prevent its
  // backgrounding.
  source_->OnStartCapturing();

  // Notify RenderProcessHost about the input stream, so that the destination
  // renderer does not get background.
  if (auto* process_host = RenderProcessHost::FromID(render_process_id))
    process_host->OnMediaStreamAdded();
}

AudioLoopbackStreamBroker::~AudioLoopbackStreamBroker() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  source_->OnStopCapturing();

  if (auto* process_host = RenderProcessHost::FromID(render_process_id()))
    process_host->OnMediaStreamRemoved();
}

void AudioLoopbackStreamBroker::CreateStream(
    audio::mojom::StreamFactory* factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!observer_binding_.is_bound());
  DCHECK(!client_request_);
  DCHECK(source_->GetGroupID());

  if (muter_)  // Mute the source.
    muter_->Connect(factory);

  media::mojom::AudioInputStreamClientPtr client;
  client_request_ = mojo::MakeRequest(&client);

  media::mojom::AudioInputStreamPtr stream;
  media::mojom::AudioInputStreamRequest stream_request =
      mojo::MakeRequest(&stream);

  media::mojom::AudioInputStreamObserverPtr observer_ptr;
  observer_binding_.Bind(mojo::MakeRequest(&observer_ptr));

  // Unretained is safe because |this| owns |observer_binding_|.
  observer_binding_.set_connection_error_handler(base::BindOnce(
      &AudioLoopbackStreamBroker::Cleanup, base::Unretained(this)));

  factory->CreateLoopbackStream(
      std::move(stream_request), std::move(client), std::move(observer_ptr),
      params_, shared_memory_count_, source_->GetGroupID(),
      base::BindOnce(&AudioLoopbackStreamBroker::StreamCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(stream)));
}

void AudioLoopbackStreamBroker::DidStartRecording() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void AudioLoopbackStreamBroker::StreamCreated(
    media::mojom::AudioInputStreamPtr stream,
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!data_pipe) {
    Cleanup();
    return;
  }

  DCHECK(renderer_factory_client_);
  renderer_factory_client_->StreamCreated(
      std::move(stream), std::move(client_request_), std::move(data_pipe),
      false /* |initially_muted|: Loopback streams are never muted. */,
      base::nullopt /* |stream_id|: Loopback streams don't have ids */);
}

void AudioLoopbackStreamBroker::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(deleter_).Run(this);
}

}  // namespace content
