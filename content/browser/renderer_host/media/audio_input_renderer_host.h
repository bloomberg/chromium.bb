// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputRendererHost serves audio related requests from audio capturer
// which lives inside the render process and provide access to audio hardware.
//
// OnCreateStream() request is only available in the low latency mode. It will
// creates a shared memory, a SyncWriter and a AudioInputController for the
// input stream.

// OnCloseStream() will close the input stream.
//
// Create stream sequence:
//
// OnCreateStream -> AudioInputController::CreateLowLatency() ->
// DoCompleteCreation -> AudioInputMsg_NotifyLowLatencyStreamCreated
//
// Close stream sequence:
// OnCloseStream -> AudioInputController::Close
//
// For the OnStartDevice() request, AudioInputRendererHost starts the device
// referenced by the session id, and an OnDeviceStarted() callback with the
// id of the opened device will be received later. Then it will send a
// IPC message to notify the renderer that the device is ready, so that
// renderer can continue with the OnCreateStream() request.
//
// OnDeviceStopped() is called when the user closes the device through
// AudioInputDeviceManager without calling Stop() before. What
// AudioInputRenderHost::OnDeviceStopped() does is to send a IPC mesaage to
// notify the renderer in order to stop the stream.
//
// Start device sequence:
//
// OnStartDevice -> AudioInputDeviceManager::Start ->
// AudioInputDeviceManagerEventHandler::OnDeviceStarted ->
// AudioInputMsg_NotifyDeviceStarted
//
// Shutdown device sequence:
//
// OnDeviceStopped -> CloseAndDeleteStream
//                    AudioInputMsg_NotifyStreamStateChanged
//
// This class is owned by BrowserRenderProcessHost and instantiated on UI
// thread. All other operations and method calls happen on IO thread, so we
// need to be extra careful about the lifetime of this object. AudioManager is a
// singleton and created in IO thread, audio input streams are also created in
// the IO thread, so we need to destroy them also in IO thread.
//
// This implementation supports low latency audio only.
// For low latency audio, a SyncSocket pair is used to signal buffer readiness
// without having to route messages using the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/renderer_host/media/audio_input_device_manager_event_handler.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_input_controller.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"

namespace content {
class ResourceContext;
}

class AudioManager;
struct AudioParameters;

class CONTENT_EXPORT AudioInputRendererHost
    : public BrowserMessageFilter,
      public media::AudioInputController::EventHandler,
      public media_stream::AudioInputDeviceManagerEventHandler {
 public:
  struct AudioEntry {
    AudioEntry();
    ~AudioEntry();

    // The AudioInputController that manages the audio input stream.
    scoped_refptr<media::AudioInputController> controller;

    // The audio input stream ID in the render view.
    int stream_id;

    // Shared memory for transmission of the audio data.
    base::SharedMemory shared_memory;

    // The synchronous writer to be used by the controller. We have the
    // ownership of the writer.
    scoped_ptr<media::AudioInputController::SyncWriter> writer;

    // Set to true after we called Close() for the controller.
    bool pending_close;
  };

  // Called from UI thread from the owner of this object.
  explicit AudioInputRendererHost(
      const content::ResourceContext* resource_context);

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

  // media_stream::AudioInputDeviceManagerEventHandler implementation.
  virtual void OnDeviceStarted(int session_id,
                               const std::string& device_id) OVERRIDE;
  virtual void OnDeviceStopped(int session_id) OVERRIDE;

 private:
  // TODO(henrika): extend test suite (compare AudioRenderHost)
  friend class content::BrowserThread;
  friend class DeleteTask<AudioInputRendererHost>;

  virtual ~AudioInputRendererHost();

  // Methods called on IO thread ----------------------------------------------

  // Start the audio input device with the session id. If the device
  // starts successfully, it will trigger OnDeviceStarted() callback.
  void OnStartDevice(int stream_id, int session_id);

  // Audio related IPC message handlers.
  // Creates an audio input stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream for the
  // required properties.
  void OnCreateStream(int stream_id,
                      const AudioParameters& params,
                      bool low_latency,
                      const std::string& device_id);

  // Record the audio input stream referenced by |stream_id|.
  void OnRecordStream(int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(int stream_id, double volume);

  // Get the volume of the audio stream referenced by |stream_id|.
  void OnGetVolume(int stream_id);

  // Complete the process of creating an audio input stream. This will set up
  // the shared memory or shared socket in low latency mode.
  void DoCompleteCreation(media::AudioInputController* controller);

  // Send a state change message to the renderer.
  void DoSendRecordingMessage(media::AudioInputController* controller);
  void DoSendPausedMessage(media::AudioInputController* controller);

  // Handle error coming from audio stream.
  void DoHandleError(media::AudioInputController* controller, int error_code);

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id);

  // Delete all audio entry and all audio streams
  void DeleteEntries();

  // Closes the stream. The stream is then deleted in DeleteEntry() after it
  // is closed.
  void CloseAndDeleteStream(AudioEntry* entry);

  // Called on the audio thread after the audio input stream is closed.
  void OnStreamClosed(AudioEntry* entry);

  // Delete an audio entry and close the related audio stream.
  void DeleteEntry(AudioEntry* entry);

  // Delete audio entry and close the related audio input stream.
  void DeleteEntryOnError(AudioEntry* entry);

  // Stop the device and delete its audio session entry.
  void StopAndDeleteDevice(int stream_id);

  // A helper method to look up a AudioEntry identified by |stream_id|.
  // Returns NULL if not found.
  AudioEntry* LookupById(int stream_id);

  // Search for a AudioEntry having the reference to |controller|.
  // This method is used to look up an AudioEntry after a controller
  // event is received.
  AudioEntry* LookupByController(media::AudioInputController* controller);

  // A helper method to look up a session identified by |stream_id|.
  // Returns 0 if not found.
  int LookupSessionById(int stream_id);

  // Used to get an instance of AudioInputDeviceManager.
  const content::ResourceContext* resource_context_;

  // A map of stream IDs to audio sources.
  typedef std::map<int, AudioEntry*> AudioEntryMap;
  AudioEntryMap audio_entries_;

  // A map of session IDs to audio session sources.
  typedef std::map<int, int> SessionEntryMap;
  SessionEntryMap session_entries_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputRendererHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
