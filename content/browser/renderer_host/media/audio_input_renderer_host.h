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
// This class is owned by BrowserRenderProcessHost and instantiated on UI
// thread. All other operations and method calls happen on IO thread, so we
// need to be extra careful about the lifetime of this object.
//
// To ensure low latency audio, a SyncSocket pair is used to signal buffer
// readiness without having to route messages using the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_input_controller.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"

namespace media {
class AudioManager;
class AudioParameters;
}

namespace content {
class MediaStreamManager;

class CONTENT_EXPORT AudioInputRendererHost
    : public BrowserMessageFilter,
      public media::AudioInputController::EventHandler {
 public:
  // Called from UI thread from the owner of this object.
  AudioInputRendererHost(
      media::AudioManager* audio_manager,
      MediaStreamManager* media_stream_manager);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // AudioInputController::EventHandler implementation.
  virtual void OnCreated(media::AudioInputController* controller) OVERRIDE;
  virtual void OnRecording(media::AudioInputController* controller) OVERRIDE;
  virtual void OnError(media::AudioInputController* controller,
                       int error_code) OVERRIDE;
  virtual void OnData(media::AudioInputController* controller,
                      const uint8* data,
                      uint32 size) OVERRIDE;

 private:
  // TODO(henrika): extend test suite (compare AudioRenderHost)
  friend class BrowserThread;
  friend class base::DeleteHelper<AudioInputRendererHost>;

  struct AudioEntry;
  typedef std::map<int, AudioEntry*> AudioEntryMap;

  virtual ~AudioInputRendererHost();

  // Methods called on IO thread ----------------------------------------------

  // Audio related IPC message handlers.
  // Creates an audio input stream with the specified session id and format.
  // |session_id| is used to find out which device to be used for the stream,
  // when it is AudioInputDeviceManager::kFakeOpenSessionId, it uses the
  // the default device. If this call is successful this object would keep an
  // internal entry of the stream for the required properties.
  void OnCreateStream(int stream_id,
                      int session_id,
                      const media::AudioParameters& params,
                      bool automatic_gain_control,
                      int shared_memory_count);

  // Track that the data for the audio stream referenced by |stream_id| is
  // consumed by an entity in the render view referenced by |render_view_id|.
  void OnAssociateStreamWithConsumer(int stream_id, int render_view_id);

  // Record the audio input stream referenced by |stream_id|.
  void OnRecordStream(int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(int stream_id, double volume);

  // Complete the process of creating an audio input stream. This will set up
  // the shared memory or shared socket in low latency mode.
  void DoCompleteCreation(media::AudioInputController* controller);

  // Send a state change message to the renderer.
  void DoSendRecordingMessage(media::AudioInputController* controller);

  // Handle error coming from audio stream.
  void DoHandleError(media::AudioInputController* controller, int error_code);

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id);

  // Delete all audio entry and all audio streams
  void DeleteEntries();

  // Closes the stream. The stream is then deleted in DeleteEntry() after it
  // is closed.
  void CloseAndDeleteStream(AudioEntry* entry);

  // Delete an audio entry and close the related audio stream.
  void DeleteEntry(AudioEntry* entry);

  // Delete audio entry and close the related audio input stream.
  void DeleteEntryOnError(AudioEntry* entry);

  // A helper method to look up a AudioEntry identified by |stream_id|.
  // Returns NULL if not found.
  AudioEntry* LookupById(int stream_id);

  // Search for a AudioEntry having the reference to |controller|.
  // This method is used to look up an AudioEntry after a controller
  // event is received.
  AudioEntry* LookupByController(media::AudioInputController* controller);

  // Used to create an AudioInputController.
  media::AudioManager* audio_manager_;

  // Used to access to AudioInputDeviceManager.
  MediaStreamManager* media_stream_manager_;

  // A map of stream IDs to audio sources.
  AudioEntryMap audio_entries_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputRendererHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
