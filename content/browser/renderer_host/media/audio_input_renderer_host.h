// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputRendererHost serves audio related requests from audio capturer
// which lives inside the render process and provide access to audio hardware.
//
// Create stream sequence (AudioInputController = AIC):
//
// AudioInputHostMsg_CreateStream -> OnCreateStream -> AIC::CreateLowLatency ->
//   <- AudioInputMsg_NotifyStreamCreated <- DoCompleteCreation <- OnCreated <-
//
// Close stream sequence:
//
// AudioInputHostMsg_CloseStream -> OnCloseStream -> AIC::Close ->
//
// This class is owned by RenderProcessHostImpl and instantiated on UI
// thread. All other operations and method calls happen on IO thread, so we
// need to be extra careful about the lifetime of this object.
//
// To ensure low latency audio, a SyncSocket pair is used to signal buffer
// readiness without having to route messages using the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/audio/audio_input_delegate.h"

namespace media {
class AudioManager;
class AudioLog;
class UserInputMonitor;
}

namespace content {
class AudioMirroringManager;
class MediaStreamManager;

class CONTENT_EXPORT AudioInputRendererHost
    : public BrowserMessageFilter,
      public media::AudioInputDelegate::EventHandler {
 public:
  // Error codes to make native logging more clear. These error codes are added
  // to generic error strings to provide a higher degree of details.
  // Changing these values can lead to problems when matching native debug
  // logs with the actual cause of error.
  enum ErrorCode {
    // Note: usage of many of these codes have been replaced with textual log
    // messages.

    // An unspecified error occured. Not used.
    // UNKNOWN_ERROR = 0,

    // Failed to look up audio intry for the provided stream id.
    INVALID_AUDIO_ENTRY = 1,

    // A stream with the specified stream id already exists.
    STREAM_ALREADY_EXISTS = 2,

    // The page does not have permission to open the specified capture device.
    PERMISSION_DENIED = 3,

    // Failed to create shared memory.
    // Obsolete.
    // SHARED_MEMORY_CREATE_FAILED = 4

    // Failed to initialize the AudioInputSyncWriter instance.
    // Obsolete.
    // SYNC_WRITER_INIT_FAILED = 5,

    // Failed to create native audio input stream.
    // Obsolete.
    // STREAM_CREATE_ERROR = 6,

    // Failed to map and share the shared memory.
    MEMORY_SHARING_FAILED = 7,

    // Unable to prepare the foreign socket handle.
    SYNC_SOCKET_ERROR = 8,

    // This error message comes from the AudioInputController instance.
    AUDIO_INPUT_CONTROLLER_ERROR = 9,
  };

  // Called from UI thread from the owner of this object.
  // |user_input_monitor| is used for typing detection and can be null.
  AudioInputRendererHost(int render_process_id,
                         media::AudioManager* audio_manager,
                         MediaStreamManager* media_stream_manager,
                         AudioMirroringManager* audio_mirroring_manager,
                         media::UserInputMonitor* user_input_monitor);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // AudioInputDelegate::EventHandler implementation.
  void OnStreamCreated(int stream_id,
                       const base::SharedMemory* shared_memory,
                       std::unique_ptr<base::CancelableSyncSocket> socket,
                       bool initially_muted) override;
  void OnStreamError(int stream_id) override;
  void OnMuted(int stream_id, bool is_muted) override;

 protected:
  ~AudioInputRendererHost() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<AudioInputRendererHost>;

  // Holds the stream_id -> delegate mapping.
  using AudioInputDelegateMap =
      base::flat_map<int, std::unique_ptr<media::AudioInputDelegate>>;

  // Methods called on IO thread ----------------------------------------------

  // Audio related IPC message handlers.

  // For ChromeOS: Checks if the stream should contain keyboard mic, if so
  // registers to AudioInputDeviceManager. Then calls DoCreateStream.
  // For non-ChromeOS: Just calls DoCreateStream.
  void OnCreateStream(int stream_id,
                      int render_frame_id,
                      int session_id,
                      const AudioInputHostMsg_CreateStream_Config& config);

  // Creates an audio input stream with the specified format whose data is
  // consumed by an entity in the RenderFrame referenced by |render_frame_id|.
  // |session_id| is used to identify the device that should be used for the
  // stream. Upon success/failure, the peer is notified via the
  // NotifyStreamCreated message.
  void DoCreateStream(
      int stream_id,
      int render_frame_id,
      int session_id,
      const AudioInputHostMsg_CreateStream_Config& config
#if defined(OS_CHROMEOS)
      ,
      AudioInputDeviceManager::KeyboardMicRegistration registration
#endif
      );

  // Record the audio input stream referenced by |stream_id|.
  void OnRecordStream(int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(int stream_id, double volume);

  // Complete the process of creating an audio input stream. This will set up
  // the shared memory or shared socket in low latency mode and send the
  // NotifyStreamCreated message to the peer.
  void DoCompleteCreation(int stream_id, bool initially_muted);

  // Handle error coming from audio stream.
  void DoHandleError(int stream_id);

  // Log audio level of captured audio stream.
  void DoLog(int stream_id, const std::string& message);

  // Notify renderer of a change to a stream's muted state.
  void DoNotifyMutedState(int stream_id, bool is_muted);

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id, ErrorCode error_code);

  void DeleteDelegateOnError(int stream_id, ErrorCode error_code);

  // A helper method to look up a AudioInputDelegate identified by |stream_id|.
  // Returns null if not found.
  media::AudioInputDelegate* LookupById(int stream_id);

  // ID of the RenderProcessHost that owns this instance.
  const int render_process_id_;

  media::AudioManager* audio_manager_;

  // Used to access to AudioInputDeviceManager.
  MediaStreamManager* media_stream_manager_;

  AudioMirroringManager* audio_mirroring_manager_;

  // A map of stream IDs to audio sources.
  AudioInputDelegateMap delegates_;

  // Raw pointer of the UserInputMonitor.
  media::UserInputMonitor* const user_input_monitor_;

  std::unique_ptr<media::AudioLog> audio_log_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputRendererHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
