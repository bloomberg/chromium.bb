// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_FORWARDING_AUDIO_STREAM_FACTORY_H_
#define CONTENT_BROWSER_MEDIA_FORWARDING_AUDIO_STREAM_FACTORY_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace service_manager {
class Connector;
}

namespace media {
class AudioParameters;
}

namespace content {

class AudioStreamBroker;
class RenderFrameHost;

// This class handles stream creation operations for a WebContents.
// This class is operated on the UI thread.
class CONTENT_EXPORT ForwardingAudioStreamFactory final
    : public WebContentsObserver {
 public:
  ForwardingAudioStreamFactory(
      WebContents* web_contents,
      std::unique_ptr<service_manager::Connector> connector,
      std::unique_ptr<AudioStreamBrokerFactory> factory);

  ~ForwardingAudioStreamFactory() final;

  // TODO(https://crbug.com/803102): Add input streams, loopback, and muting.
  // TODO(https://crbug.com/787806): Automatically restore streams on audio
  // service restart.
  void CreateOutputStream(
      RenderFrameHost* frame,
      const std::string& device_id,
      const media::AudioParameters& params,
      media::mojom::AudioOutputStreamProviderClientPtr client);

  // WebContentsObserver implementation. We observe these events so that we can
  // clean up streams belonging to a document when that document is destroyed.
  void FrameDeleted(RenderFrameHost* render_frame_host) final;
  void DidFinishNavigation(NavigationHandle* navigation_handle) final;
  void WebContentsDestroyed() final;

 private:
  using StreamBrokerSet = base::flat_set<std::unique_ptr<AudioStreamBroker>,
                                         base::UniquePtrComparator>;

  void CleanupStreamsBelongingTo(RenderFrameHost* render_frame_host);

  void RemoveOutput(AudioStreamBroker* handle);

  audio::mojom::StreamFactory* GetFactory();
  void ResetRemoteFactoryPtrIfIdle();
  void ResetRemoteFactoryPtr();

  const std::unique_ptr<service_manager::Connector> connector_;
  const std::unique_ptr<AudioStreamBrokerFactory> broker_factory_;

  // Lazily acquired. Reset on connection error and when we no longer have any
  // streams. Note: we don't want muting to force the connection to be open,
  // since we want to clean up the service when not in use. If we have active
  // muting but nothing else, we should stop it and start it again when we need
  // to reacquire the factory for some other reason.
  audio::mojom::StreamFactoryPtr remote_factory_;

  // Running id used for tracking audible streams. We keep count here to avoid
  // collisions.
  // TODO(https://crbug.com/830494): Refactor to make this unnecessary and
  // remove it.
  int stream_id_counter_ = 0;

  StreamBrokerSet outputs_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingAudioStreamFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_FORWARDING_AUDIO_STREAM_FACTORY_H_
