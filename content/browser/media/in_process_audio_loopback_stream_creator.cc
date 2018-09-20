// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/in_process_audio_loopback_stream_creator.h"

#include <memory>
#include <utility>

#include "content/browser/browser_main_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/service_manager_connection.h"
#include "media/audio/audio_device_description.h"
#include "media/base/user_input_monitor.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

// A mojom::RendererAudioInputStreamFactoryClient that holds a
// AudioLoopbackStreamCreator::StreamCreatedCallback. The callback runs when the
// requested audio stream is created.
class StreamCreatedCallbackAdapter final
    : public mojom::RendererAudioInputStreamFactoryClient {
 public:
  explicit StreamCreatedCallbackAdapter(
      const AudioLoopbackStreamCreator::StreamCreatedCallback& callback)
      : callback_(callback) {
    DCHECK(callback_);
  }

  ~StreamCreatedCallbackAdapter() override {}

  // mojom::RendererAudioInputStreamFactoryClient implementation.
  void StreamCreated(
      media::mojom::AudioInputStreamPtr stream,
      media::mojom::AudioInputStreamClientRequest client_request,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const base::Optional<base::UnguessableToken>& stream_id) override {
    DCHECK(!initially_muted);  // Loopback streams shouldn't be started muted.
    callback_.Run(std::move(stream), std::move(client_request),
                  std::move(data_pipe));
  }

 private:
  const AudioLoopbackStreamCreator::StreamCreatedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(StreamCreatedCallbackAdapter);
};

}  // namespace

InProcessAudioLoopbackStreamCreator::InProcessAudioLoopbackStreamCreator()
    : factory_(nullptr,
               BrowserMainLoop::GetInstance()
                   ? static_cast<media::UserInputMonitorBase*>(
                         BrowserMainLoop::GetInstance()->user_input_monitor())
                   : nullptr,
               content::ServiceManagerConnection::GetForProcess()
                   ->GetConnector()
                   ->Clone(),
               AudioStreamBrokerFactory::CreateImpl()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

InProcessAudioLoopbackStreamCreator::~InProcessAudioLoopbackStreamCreator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  factory_.FrameDeleted(nullptr);
}

void InProcessAudioLoopbackStreamCreator::CreateLoopbackStream(
    WebContents* loopback_source,
    const media::AudioParameters& params,
    uint32_t total_segments,
    const StreamCreatedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  mojo::MakeStrongBinding(
      std::make_unique<StreamCreatedCallbackAdapter>(callback),
      mojo::MakeRequest(&client));
  if (loopback_source) {
    factory_.CreateLoopbackStream(
        nullptr,
        static_cast<WebContentsImpl*>(loopback_source)->GetAudioStreamFactory(),
        params, total_segments, true /* mute_source */, std::move(client));
  } else {
    // A null |frame_of_source_web_contents| requests system-wide loopback.
    factory_.CreateInputStream(
        nullptr, media::AudioDeviceDescription::kLoopbackWithMuteDeviceId,
        params, total_segments, false /* enable_agc */,
        nullptr /* processing_config */, std::move(client));
  }
}

}  // namespace content
