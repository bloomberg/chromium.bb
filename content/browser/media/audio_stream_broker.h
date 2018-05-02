// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"

namespace audio {
namespace mojom {
class StreamFactory;
}
}  // namespace audio

namespace base {
class UnguessableToken;
}

namespace media {
class AudioParameters;
}

namespace content {

// An AudioStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It also sets up all objects
// used for monitoring the stream.
class CONTENT_EXPORT AudioStreamBroker {
 public:
  using DeleterCallback = base::OnceCallback<void(AudioStreamBroker*)>;

  AudioStreamBroker(int render_process_id, int render_frame_id);
  virtual ~AudioStreamBroker();

  virtual void CreateStream(audio::mojom::StreamFactory* factory) = 0;

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

 protected:
  const int render_process_id_;
  const int render_frame_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioStreamBroker);
};

// Used for dependency injection into ForwardingAudioStreamFactory.
class CONTENT_EXPORT AudioStreamBrokerFactory {
 public:
  static std::unique_ptr<AudioStreamBrokerFactory> CreateImpl();

  AudioStreamBrokerFactory();
  virtual ~AudioStreamBrokerFactory();

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      AudioStreamBroker::DeleterCallback deleter,
      media::mojom::AudioOutputStreamProviderClientPtr client) = 0;

  // TODO(https://crbug.com/830493): Other kinds of streams.

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioStreamBrokerFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_H_
