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
#include "media/audio/audio_input_controller.h"

namespace base {
class FilePath;
}

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
      public media::AudioInputController::EventHandler {
 public:
  // Error codes to make native logging more clear. These error codes are added
  // to generic error strings to provide a higher degree of details.
  // Changing these values can lead to problems when matching native debug
  // logs with the actual cause of error.
  enum ErrorCode {
    // An unspecified error occured.
    UNKNOWN_ERROR = 0,

    // Failed to look up audio intry for the provided stream id.
    INVALID_AUDIO_ENTRY = 1,

    // A stream with the specified stream id already exists.
    STREAM_ALREADY_EXISTS = 2,

    // The page does not have permission to open the specified capture device.
    PERMISSION_DENIED = 3,

    // Failed to create shared memory.
    // Obsolete, merged with SYNC_WRITER_INIT_FAILED.
    // SHARED_MEMORY_CREATE_FAILED = 4

    // Failed to initialize the AudioInputSyncWriter instance.
    SYNC_WRITER_INIT_FAILED = 5,

    // Failed to create native audio input stream.
    STREAM_CREATE_ERROR = 6,

    // Failed to map and share the shared memory.
    MEMORY_SHARING_FAILED = 7,

    // Unable to prepare the foreign socket handle.
    SYNC_SOCKET_ERROR = 8,

    // This error message comes from the AudioInputController instance.
    AUDIO_INPUT_CONTROLLER_ERROR = 9,
  };

  // Called from UI thread from the owner of this object.
  // |user_input_monitor| is used for typing detection and can be NULL.
  AudioInputRendererHost(int render_process_id,
                         media::AudioManager* audio_manager,
                         MediaStreamManager* media_stream_manager,
                         AudioMirroringManager* audio_mirroring_manager,
                         media::UserInputMonitor* user_input_monitor);

#if BUILDFLAG(ENABLE_WEBRTC)
  // Enable and disable debug recording of input on all audio entries.
  void EnableDebugRecording(const base::FilePath& file);
  void DisableDebugRecording();
#endif

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // AudioInputController::EventHandler implementation.
  void OnCreated(media::AudioInputController* controller,
                 bool initially_muted) override;
  void OnError(media::AudioInputController* controller,
               media::AudioInputController::ErrorCode error_code) override;
  void OnLog(media::AudioInputController* controller,
             const std::string& message) override;
  void OnMuted(media::AudioInputController* controller, bool is_muted) override;

  // Sets the PID renderer. This is used for constructing the debug recording
  // filename.
  void set_renderer_pid(int32_t renderer_pid);

 protected:
  ~AudioInputRendererHost() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<AudioInputRendererHost>;

  struct AudioEntry;
  typedef std::map<int, AudioEntry*> AudioEntryMap;

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
  void DoCompleteCreation(media::AudioInputController* controller,
                          bool initially_muted);

  // Send a state change message to the renderer.
  void DoSendRecordingMessage(media::AudioInputController* controller);

  // Handle error coming from audio stream.
  void DoHandleError(media::AudioInputController* controller,
      media::AudioInputController::ErrorCode error_code);

  // Log audio level of captured audio stream.
  void DoLog(media::AudioInputController* controller,
             const std::string& message);

  // Notify renderer of a change to a stream's muted state.
  void DoNotifyMutedState(media::AudioInputController* controller,
                          bool is_muted);

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id, ErrorCode error_code);

  // Delete all audio entries and all audio streams.
  void DeleteEntries();

  // Closes the stream. The stream is then deleted in DeleteEntry() after it
  // is closed.
  void CloseAndDeleteStream(AudioEntry* entry);

  // Delete an audio entry and close the related audio stream.
  void DeleteEntry(AudioEntry* entry);

  // Delete audio entry and close the related audio input stream.
  void DeleteEntryOnError(AudioEntry* entry, ErrorCode error_code);

  // A helper method to look up a AudioEntry identified by |stream_id|.
  // Returns NULL if not found.
  AudioEntry* LookupById(int stream_id);

  // Search for a AudioEntry having the reference to |controller|.
  // This method is used to look up an AudioEntry after a controller
  // event is received.
  AudioEntry* LookupByController(media::AudioInputController* controller);

#if BUILDFLAG(ENABLE_WEBRTC)
  // TODO(grunell): Move debug recording handling to AudioManager.
  void MaybeEnableDebugRecordingForId(int stream_id);

  base::FilePath GetDebugRecordingFilePathWithExtensions(
      const base::FilePath& file);

  void EnableDebugRecordingForId(const base::FilePath& file, int stream_id);

  // Calls GetDebugRecordingFilePathWithExtensions() and
  // EnableDebugRecordingForId().
  void AddExtensionsToPathAndEnableDebugRecordingForId(
      const base::FilePath& file,
      int stream_id);

  void DoEnableDebugRecording(int stream_id, base::File file);
  void DoDisableDebugRecording(int stream_id);

  // Delete the debug writer used for debug recordings for |stream_id|.
  void DeleteDebugWriter(int stream_id);
#endif

  // ID of the RenderProcessHost that owns this instance.
  const int render_process_id_;

  // PID of the render process connected to the RenderProcessHost that owns this
  // instance.
  int32_t renderer_pid_;

  // Used to create an AudioInputController.
  media::AudioManager* audio_manager_;

  // Used to access to AudioInputDeviceManager.
  MediaStreamManager* media_stream_manager_;

  AudioMirroringManager* audio_mirroring_manager_;

  // A map of stream IDs to audio sources.
  AudioEntryMap audio_entries_;

  // Raw pointer of the UserInputMonitor.
  media::UserInputMonitor* const user_input_monitor_;

  std::unique_ptr<media::AudioLog> audio_log_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputRendererHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
