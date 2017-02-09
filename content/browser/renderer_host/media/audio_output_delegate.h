// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace base {
class SharedMemory;
class CancelableSyncSocket;
}

namespace media {
class AudioOutputController;
}

namespace content {

class CONTENT_EXPORT AudioOutputDelegate {
 public:
  class CONTENT_EXPORT EventHandler {
   public:
    virtual ~EventHandler() {}

    // All these methods are called on the IO thread.

    // Called when construction is finished and the stream is ready for
    // playout.
    virtual void OnStreamCreated(int stream_id,
                                 base::SharedMemory* shared_memory,
                                 base::CancelableSyncSocket* socket) = 0;

    // Called if stream encounters an error and has become unusable.
    virtual void OnStreamError(int stream_id) = 0;
  };

  virtual ~AudioOutputDelegate() {}

  // TODO(maxmorin): Remove GetController() when crbug.com/647185 is closed.
  // This function is used to provide control of the audio stream to
  // WebrtcAudioPrivateGetActiveSinkFunction and others in the webrtc extension
  // API. Since the controller is shared, this means that it might outlive the
  // AudioOutputDelegate. In this case, it is still safe to call functions on
  // the controller, but it will not do anything. The controller is also shared
  // with AudioStreamMonitor.
  virtual scoped_refptr<media::AudioOutputController> GetController() const = 0;
  virtual int GetStreamId() const = 0;

  // Stream control:
  virtual void OnPlayStream() = 0;
  virtual void OnPauseStream() = 0;
  virtual void OnSetVolume(double volume) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_H_
