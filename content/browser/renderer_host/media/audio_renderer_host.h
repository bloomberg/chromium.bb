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
//      |  < NotifyStreamStateChanged   | kAudioStreamPlaying
//      |                               |
//      |         PauseStream >         |
//      |  < NotifyStreamStateChanged   | kAudioStreamPaused
//      |                               |
//      |          PlayStream >         |
//      |  < NotifyStreamStateChanged   | kAudioStreamPlaying
//      |             ...               |
//      |         CloseStream >         |
//      v                               v

// A SyncSocket pair is used to signal buffer readiness between processes.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/renderer_host/media/audio_output_device_enumerator.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_logging.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/simple_sources.h"
#include "url/origin.h"

namespace media {
class AudioManager;
class AudioParameters;
}

namespace content {

class AudioMirroringManager;
class MediaInternals;
class MediaStreamManager;
class MediaStreamUIProxy;
class ResourceContext;

class CONTENT_EXPORT AudioRendererHost : public BrowserMessageFilter {
 public:
  // Called from UI thread from the owner of this object.
  AudioRendererHost(int render_process_id,
                    media::AudioManager* audio_manager,
                    AudioMirroringManager* mirroring_manager,
                    MediaInternals* media_internals,
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

  // Returns true if any streams managed by this host are actively playing.  Can
  // be called from any thread.
  bool HasActiveAudio();

 private:
  friend class AudioRendererHostTest;
  friend class BrowserThread;
  friend class base::DeleteHelper<AudioRendererHost>;
  friend class MockAudioRendererHost;
  friend class TestAudioRendererHost;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, CreateMockStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, MockStreamDataConversation);

  class AudioEntry;
  typedef std::map<int, AudioEntry*> AudioEntryMap;

  // Internal callback type for access requests to output devices.
  // |have_access| is true only if there is permission to access the device.
  typedef base::Callback<void(bool have_access)> OutputDeviceAccessCB;

  // Internal callback type for information requests about an output device.
  // |success| indicates the operation was successful. If true, |device_info|
  // contains data about the device.
  typedef base::Callback<void(bool success,
                              const AudioOutputDeviceInfo& device_info)>
      OutputDeviceInfoCB;

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

  // Proceed with device authorization after checking permissions.
  void OnDeviceAuthorized(int stream_id,
                          const std::string& device_id,
                          const url::Origin& security_origin,
                          base::TimeTicks auth_start_time,
                          bool have_access);

  // Proceed with device authorization after translating device ID.
  void OnDeviceIDTranslated(int stream_id,
                            base::TimeTicks auth_start_time,
                            bool device_found,
                            const AudioOutputDeviceInfo& device_info);

  // Start the actual creation of an audio stream, after the device
  // authorization process is complete.
  void DoCreateStream(int stream_id,
                      int render_frame_id,
                      const media::AudioParameters& params,
                      const std::string& device_unique_id,
                      bool render_frame_id_is_valid);

  // Complete the process of creating an audio stream. This will set up the
  // shared memory or shared socket in low latency mode and send the
  // NotifyStreamCreated message to the peer.
  void DoCompleteCreation(int stream_id);

  // Send playing/paused status to the renderer.
  void DoNotifyStreamStateChanged(int stream_id, bool is_playing);

  RenderProcessHost::AudioOutputControllerList DoGetOutputControllers() const;

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id);

  // Delete an audio entry, notifying observers first.  This is called by
  // AudioOutputController after it has closed.
  void DeleteEntry(std::unique_ptr<AudioEntry> entry);

  // Send an error message to the renderer, then close the stream.
  void ReportErrorAndClose(int stream_id);

  // A helper method to look up a AudioEntry identified by |stream_id|.
  // Returns NULL if not found.
  AudioEntry* LookupById(int stream_id);

  // A helper method to update the number of playing streams and alert the
  // ResourceScheduler when the renderer starts or stops playing an audiostream.
  void UpdateNumPlayingStreams(AudioEntry* entry, bool is_playing);

  // Check if the renderer process has access to the requested output device.
  void CheckOutputDeviceAccess(int render_frame_id,
                               const std::string& device_id,
                               const url::Origin& security_origin,
                               const OutputDeviceAccessCB& callback);

  // Invoke |callback| after permission to use a device has been checked.
  void AccessChecked(std::unique_ptr<MediaStreamUIProxy> ui_proxy,
                     const OutputDeviceAccessCB& callback,
                     bool have_access);

  // Translate the hashed |device_id| to a unique device ID.
  void TranslateDeviceID(const std::string& device_id,
                         const url::Origin& security_origin,
                         const OutputDeviceInfoCB& callback,
                         const AudioOutputDeviceEnumeration& enumeration);

  // Helper method to check if the authorization procedure for stream
  // |stream_id| has started.
  bool IsAuthorizationStarted(int stream_id);

#if DCHECK_IS_ON()
  // Called from AudioRendererHostTest to override the function that checks for
  // the existence of the RenderFrameHost at stream creation time.
  void set_render_frame_id_validate_function_for_testing(
      ValidateRenderFrameIdFunction function) {
    validate_render_frame_id_function_ = function;
  }
#endif  // DCHECK_IS_ON()

  // ID of the RenderProcessHost that owns this instance.
  const int render_process_id_;

  media::AudioManager* const audio_manager_;
  AudioMirroringManager* const mirroring_manager_;
  std::unique_ptr<media::AudioLog> audio_log_;

  // Used to access to AudioInputDeviceManager.
  MediaStreamManager* media_stream_manager_;

  // A map of stream IDs to audio sources.
  AudioEntryMap audio_entries_;

  // The number of streams in the playing state. Atomic read safe from any
  // thread, but should only be updated from the IO thread.
  base::AtomicRefCount num_playing_streams_;

  // Salt required to translate renderer device IDs to raw device unique IDs
  std::string salt_;

  // Map of device authorizations for streams that are not yet created
  // The key is the stream ID, and the value is a pair. The pair's first element
  // is a bool that is true if the authorization process completes successfully.
  // The second element contains the unique ID of the authorized device.
  std::map<int, std::pair<bool, std::string>> authorizations_;

#if DCHECK_IS_ON()
  // When DCHECKs are turned on, AudioRendererHost will call this function on
  // the UI thread to validate render frame IDs. A default is set by the
  // constructor, but this can be overridden by unit tests.
  ValidateRenderFrameIdFunction validate_render_frame_id_function_;
#endif  // DCHECK_IS_ON()

  // The maximum number of simultaneous streams during the lifetime of this
  // host. Reported as UMA stat at shutdown.
  size_t max_simultaneous_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_
