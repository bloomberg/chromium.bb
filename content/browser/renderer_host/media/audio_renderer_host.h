// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioRendererHost serves audio related requests from AudioRenderer which
// lives inside the render process and provide access to audio hardware.
//
// This class is owned by RenderProcessHostImpl, and instantiated on UI
// thread, but all other operations and method calls happen on IO thread, so we
// need to be extra careful about the lifetime of this object. AudioManager is a
// singleton and created in IO thread, audio output streams are also created in
// the IO thread, so we need to destroy them also in IO thread. After this class
// is created, a task of OnInitialized() is posted on IO thread in which
// singleton of AudioManager is created.
//
// Here's an example of a typical IPC dialog for audio:
//
//   Renderer                     AudioRendererHost
//      |                               |
//      |  RequestDeviceAuthorization > |
//      |    < NotifyDeviceAuthorized   |
//      |                               |
//      |         CreateStream >        |
//      |     < NotifyStreamCreated     |
//      |                               |
//      |          PlayStream >         |
//      |                               |
//      |         PauseStream >         |
//      |                               |
//      |          PlayStream >         |
//      |             ...               |
//      |         CloseStream >         |
//      v                               v
// If there is an error at any point, a NotifyStreamError will
// be sent. Otherwise, the renderer can assume that the actual state
// of the output stream is consistent with the control signals it sends.

// A SyncSocket pair is used to signal buffer readiness between processes.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"
#include "content/browser/renderer_host/media/audio_output_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host.h"

namespace base {
class SharedMemory;
class CancelableSyncSocket;
}

namespace media {
class AudioManager;
class AudioParameters;
}

namespace content {

class AudioMirroringManager;
class MediaStreamManager;

class CONTENT_EXPORT AudioRendererHost
    : public BrowserMessageFilter,
      public AudioOutputDelegate::EventHandler {
 public:
  // Called from UI thread from the owner of this object.
  AudioRendererHost(int render_process_id,
                    media::AudioManager* audio_manager,
                    AudioMirroringManager* mirroring_manager,
                    MediaStreamManager* media_stream_manager,
                    const std::string& salt);

  // Calls |callback| with the list of AudioOutputControllers for this object.
  void GetOutputControllers(
      const RenderProcessHost::GetAudioOutputControllersCallback&
          callback) const;

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // AudioOutputDelegate::EventHandler implementation
  void OnStreamCreated(int stream_id,
                       base::SharedMemory* shared_memory,
                       base::CancelableSyncSocket* foreign_socket) override;
  void OnStreamError(int stream_id) override;

  void OverrideDevicePermissionsForTesting(bool has_access);

 private:
  friend class AudioRendererHostTest;
  friend class BrowserThread;
  friend class base::DeleteHelper<AudioRendererHost>;
  friend class MockAudioRendererHost;
  friend class TestAudioRendererHost;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, CreateMockStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, MockStreamDataConversation);

  // Internal callback type for access requests to output devices.
  // |have_access| is true only if there is permission to access the device.
  typedef base::Callback<void(bool have_access)> OutputDeviceAccessCB;

  using AudioOutputDelegateVector =
      std::vector<std::unique_ptr<AudioOutputDelegate>>;

  // The type of a function that is run on the UI thread to check whether the
  // routing IDs reference a valid RenderFrameHost. The function then runs
  // |callback| on the IO thread with true/false if valid/invalid.
  using ValidateRenderFrameIdFunction =
      void (*)(int render_process_id,
               int render_frame_id,
               const base::Callback<void(bool)>& callback);

  ~AudioRendererHost() override;

  // Methods called on IO thread ----------------------------------------------

  // Audio related IPC message handlers.

  // Request permission to use an output device for use by a stream produced
  // in the RenderFrame referenced by |render_frame_id|.
  // |session_id| is used for unified IO to find out which input device to be
  // opened for the stream. For clients that do not use unified IO,
  // |session_id| will be ignored and the given |device_id| and
  // |security_origin| will be used to select the output device.
  // Upon completion of the process, the peer is notified with the device output
  // parameters via the NotifyDeviceAuthorized message.
  void OnRequestDeviceAuthorization(int stream_id,
                                    int render_frame_id,
                                    int session_id,
                                    const std::string& device_id,
                                    const url::Origin& security_origin);

  void AuthorizationCompleted(int stream_id,
                              const url::Origin& security_origin,
                              base::TimeTicks auth_start_time,
                              media::OutputDeviceStatus status,
                              bool should_send_id,
                              const media::AudioParameters& params,
                              const std::string& raw_device_id);

  // Creates an audio output stream with the specified format.
  // Upon success/failure, the peer is notified via the NotifyStreamCreated
  // message.
  void OnCreateStream(int stream_id,
                      int render_frame_id,
                      const media::AudioParameters& params);

  // Play the audio stream referenced by |stream_id|.
  void OnPlayStream(int stream_id);

  // Pause the audio stream referenced by |stream_id|.
  void OnPauseStream(int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(int stream_id, double volume);

  // Helper methods.

  // Called after the |render_frame_id| provided to OnCreateStream() was
  // validated. When |is_valid| is false, this calls OnStreamError().
  void DidValidateRenderFrame(int stream_id, bool is_valid);

  // Updates status of stream for AudioStreamMonitor and updates
  // the number of playing streams.
  void StreamStateChanged(int stream_id, bool is_playing);

  RenderProcessHost::AudioOutputControllerList DoGetOutputControllers() const;

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id);

  // Helper methods to look up a AudioOutputDelegate identified by |stream_id|.
  // Returns delegates_.end() if not found.
  AudioOutputDelegateVector::iterator LookupIteratorById(int stream_id);
  // Returns nullptr if not found.
  AudioOutputDelegate* LookupById(int stream_id);

  // Helper method to check if the authorization procedure for stream
  // |stream_id| has started.
  bool IsAuthorizationStarted(int stream_id);

  // Called from AudioRendererHostTest to override the function that checks for
  // the existence of the RenderFrameHost at stream creation time.
  void set_render_frame_id_validate_function_for_testing(
      ValidateRenderFrameIdFunction function) {
    validate_render_frame_id_function_ = function;
  }

  // ID of the RenderProcessHost that owns this instance.
  const int render_process_id_;

  media::AudioManager* const audio_manager_;
  AudioMirroringManager* const mirroring_manager_;

  // Used to access to AudioInputDeviceManager.
  MediaStreamManager* media_stream_manager_;

  // A list of the current open streams.
  AudioOutputDelegateVector delegates_;

  // Salt required to translate renderer device IDs to raw device unique IDs
  std::string salt_;

  // Map of device authorizations for streams that are not yet created
  // The key is the stream ID, and the value is a pair. The pair's first element
  // is a bool that is true if the authorization process completes successfully.
  // The second element contains the unique ID of the authorized device.
  std::map<int, std::pair<bool, std::string>> authorizations_;

  // At stream creation time, AudioRendererHost will call this function on the
  // UI thread to validate render frame IDs. A default is set by the
  // constructor, but this can be overridden by unit tests.
  ValidateRenderFrameIdFunction validate_render_frame_id_function_;

  AudioOutputAuthorizationHandler authorization_handler_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_
