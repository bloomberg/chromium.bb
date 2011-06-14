// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputRendererHost serves audio related requests from audio capturer
// which lives inside the render process and provide access to audio hardware.
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

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/browser_thread.h"
#include "media/audio/audio_input_controller.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"

class AudioManager;
struct AudioParameters;

class AudioInputRendererHost : public BrowserMessageFilter,
    public media::AudioInputController::EventHandler {
 public:
  typedef std::pair<int32, int> AudioEntryId;

  struct AudioEntry {
    AudioEntry();
    ~AudioEntry();

    // The AudioInputController that manages the audio input stream.
    scoped_refptr<media::AudioInputController> controller;

    // Render view ID that requested the audio input stream.
    int32 render_view_id;

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

  typedef std::map<AudioEntryId, AudioEntry*> AudioEntryMap;

  // Called from UI thread from the owner of this object.
  AudioInputRendererHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing();
  virtual void OnDestruct() const;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  // AudioInputController::EventHandler implementation.
  virtual void OnCreated(media::AudioInputController* controller);
  virtual void OnRecording(media::AudioInputController* controller);
  virtual void OnError(media::AudioInputController* controller,
                       int error_code);
  virtual void OnData(media::AudioInputController* controller,
                      const uint8* data,
                      uint32 size);

 private:
  // TODO(henrika): extend test suite (compare AudioRenderHost)
  friend class BrowserThread;
  friend class DeleteTask<AudioInputRendererHost>;

  virtual ~AudioInputRendererHost();

  // Methods called on IO thread.
  // Returns true if the message is an audio input related message and should
  // be handled by this class.
  bool IsAudioRendererHostMessage(const IPC::Message& message);

  // Audio related IPC message handlers.
  // Creates an audio input stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream for the
  // required properties.
  void OnCreateStream(const IPC::Message& msg, int stream_id,
                      const AudioParameters& params,
                      bool low_latency);

  // Record the audio input stream referenced by |stream_id|.
  void OnRecordStream(const IPC::Message& msg, int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(const IPC::Message& msg, int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(const IPC::Message& msg, int stream_id, double volume);

  // Get the volume of the audio stream referenced by |stream_id|.
  void OnGetVolume(const IPC::Message& msg, int stream_id);

  // Complete the process of creating an audio input stream. This will set up
  // the shared memory or shared socket in low latency mode.
  void DoCompleteCreation(media::AudioInputController* controller);

  // Send a state change message to the renderer.
  void DoSendRecordingMessage(media::AudioInputController* controller);
  void DoSendPausedMessage(media::AudioInputController* controller);

  // Handle error coming from audio stream.
  void DoHandleError(media::AudioInputController* controller, int error_code);

  // Send an error message to the renderer.
  void SendErrorMessage(int32 render_view_id, int32 stream_id);

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

  // A helper method to look up a AudioEntry with a tuple of render view
  // id and stream id. Returns NULL if not found.
  AudioEntry* LookupById(int render_view_id, int stream_id);

  // Search for a AudioEntry having the reference to |controller|.
  // This method is used to look up an AudioEntry after a controller
  // event is received.
  AudioEntry* LookupByController(media::AudioInputController* controller);

  // A map of id to audio sources.
  AudioEntryMap audio_entries_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputRendererHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_RENDERER_HOST_H_
