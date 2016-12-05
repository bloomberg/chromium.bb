// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/audio/audio_output_controller.h"

namespace base {
class SharedMemory;
class CancelableSyncSocket;
}

namespace content {
class AudioMirroringManager;
class AudioSyncReader;
class MediaObserver;
}

namespace media {
class AudioLog;
class AudioParameters;
}

namespace content {

// This class, except for the AudioOutputDelegate::EventHandler implementation,
// is operated on the IO thread.
class CONTENT_EXPORT AudioOutputDelegate
    : public media::AudioOutputController::EventHandler {
 public:
  class CONTENT_EXPORT EventHandler {
   public:
    virtual ~EventHandler() {}

    // All these methods are called on the IO thread.

    // Called when the state changes between playing and not playing.
    virtual void OnStreamStateChanged(bool playing) = 0;

    // Called when construction is finished and the stream is ready for
    // playout.
    virtual void OnStreamCreated(int stream_id,
                                 base::SharedMemory* shared_memory,
                                 base::CancelableSyncSocket* socket) = 0;

    // Called if stream encounters an error and has become unusable.
    virtual void OnStreamError(int stream_id) = 0;
  };

  class CONTENT_EXPORT Deleter {
   public:
    Deleter() = default;
    explicit Deleter(AudioMirroringManager* mirroring_manager)
        : mirroring_manager_(mirroring_manager) {}
    void operator()(AudioOutputDelegate* delegate);

   private:
    AudioMirroringManager* mirroring_manager_;
  };

  using UniquePtr = std::unique_ptr<AudioOutputDelegate, Deleter>;

  // The AudioOutputDelegate might issue callbacks until the UniquePtr
  // destructor finishes, so calling |handler| must be valid until then.
  static UniquePtr Create(EventHandler* handler,
                          media::AudioManager* audio_manager,
                          std::unique_ptr<media::AudioLog> audio_log,
                          AudioMirroringManager* mirroring_manager,
                          MediaObserver* media_observer,
                          int stream_id,
                          int render_frame_id,
                          int render_process_id,
                          const media::AudioParameters& params,
                          const std::string& output_device_id);
  ~AudioOutputDelegate() override;

  // TODO(maxmorin): Remove this when crbug.com/647185 is closed.
  // This function is used to provide control of the audio stream to
  // WebrtcAudioPrivateGetActiveSinkFunction and others in the webrtc extension
  // API. Since the controller is shared, this means that it might outlive the
  // AudioOutputDelegate. In this case, it is still safe to call functions on
  // the controller, but it will not do anything. The controller is also shared
  // with AudioStreamMonitor.
  scoped_refptr<media::AudioOutputController> controller() const {
    return controller_;
  }

  int stream_id() const { return stream_id_; }

  // Stream control:
  void OnPlayStream();
  void OnPauseStream();
  void OnSetVolume(double volume);

  // AudioOutputController::EventHandler implementation. These methods may
  // be called on any thead.
  void OnControllerCreated() override;
  void OnControllerPlaying() override;
  void OnControllerPaused() override;
  void OnControllerError() override;

 private:
  AudioOutputDelegate(EventHandler* handler,
                      media::AudioManager* audio_manager,
                      std::unique_ptr<media::AudioLog> audio_log,
                      int stream_id,
                      int render_frame_id,
                      int render_process_id,
                      const media::AudioParameters& params,
                      const std::string& output_device_id);

  void SendCreatedNotification();
  void OnError();
  void UpdatePlayingState(bool playing);

  // |handler_| is null if we are in the process of destruction. In this case,
  // we will ignore events from |controller_|.
  EventHandler* handler_;
  std::unique_ptr<media::AudioLog> const audio_log_;
  std::unique_ptr<AudioSyncReader> reader_;
  scoped_refptr<media::AudioOutputController> controller_;
  const int stream_id_;
  const int render_frame_id_;
  const int render_process_id_;

  // This flag ensures that we only send OnStreamStateChanged notifications
  // and (de)register with the stream monitor when the state actually changes.
  bool playing_ = false;
  base::WeakPtr<AudioOutputDelegate> weak_this_;
  base::WeakPtrFactory<AudioOutputDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_H_
