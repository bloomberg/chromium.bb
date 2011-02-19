// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioRendererHost serves audio related requests from AudioRenderer which
// lives inside the render process and provide access to audio hardware.
//
// This class is owned by BrowserRenderProcessHost, and instantiated on UI
// thread, but all other operations and method calls happen on IO thread, so we
// need to be extra careful about the lifetime of this object. AudioManager is a
// singleton and created in IO thread, audio output streams are also created in
// the IO thread, so we need to destroy them also in IO thread. After this class
// is created, a task of OnInitialized() is posted on IO thread in which
// singleton of AudioManager is created and.
//
// Here's an example of a typical IPC dialog for audio:
//
//   Renderer                     AudioRendererHost
//      |                               |
//      |         CreateStream >        |
//      |          < Created            |
//      |                               |
//      |             Play >            |
//      |           < Playing           |  time
//      |                               |
//      |     < RequestAudioPacket      |
//      |      AudioPacketReady >       |
//      |             ...               |
//      |     < RequestAudioPacket      |
//      |      AudioPacketReady >       |
//      |                               |
//      |             ...               |
//      |     < RequestAudioPacket      |
//      |      AudioPacketReady >       |
//      |             ...               |
//      |           Pause >             |
//      |          < Paused             |
//      |            ...                |
//      |           Start >             |
//      |          < Started            |
//      |             ...               |
//      |            Close >            |
//      v                               v

// The above mode of operation uses relatively big buffers and has latencies
// of 50 ms or more. There is a second mode of operation which is low latency.
// For low latency audio, the picture above is modified by not having the
// RequestAudioPacket and the AudioPacketReady messages, instead a SyncSocket
// pair is used to signal buffer readiness without having to route messages
// using the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
#pragma once

#include <map>

#include "base/gtest_prod_util.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "chrome/browser/browser_message_filter.h"
#include "chrome/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/simple_sources.h"

class AudioManager;
struct ViewHostMsg_Audio_CreateStream_Params;

class AudioRendererHost : public BrowserMessageFilter,
                          public media::AudioOutputController::EventHandler {
 public:
  typedef std::pair<int32, int> AudioEntryId;

  struct AudioEntry {
    AudioEntry();
    ~AudioEntry();

    // The AudioOutputController that manages the audio stream.
    scoped_refptr<media::AudioOutputController> controller;

    // Render view ID that requested the audio stream.
    int32 render_view_id;

    // The audio stream ID in the render view.
    int stream_id;

    // Shared memory for transmission of the audio data.
    base::SharedMemory shared_memory;

    // The synchronous reader to be used by the controller. We have the
    // ownership of the reader.
    scoped_ptr<media::AudioOutputController::SyncReader> reader;

    bool pending_buffer_request;

    // Set to true after we called Close() for the controller.
    bool pending_close;
  };

  typedef std::map<AudioEntryId, AudioEntry*> AudioEntryMap;

  // Called from UI thread from the owner of this object.
  AudioRendererHost();


  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing();
  virtual void OnDestruct() const;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  /////////////////////////////////////////////////////////////////////////////
  // AudioOutputController::EventHandler implementations.
  virtual void OnCreated(media::AudioOutputController* controller);
  virtual void OnPlaying(media::AudioOutputController* controller);
  virtual void OnPaused(media::AudioOutputController* controller);
  virtual void OnError(media::AudioOutputController* controller,
                       int error_code);
  virtual void OnMoreData(media::AudioOutputController* controller,
                          AudioBuffersState buffers_state);

 private:
  friend class AudioRendererHostTest;
  friend class BrowserThread;
  friend class DeleteTask<AudioRendererHost>;
  friend class MockAudioRendererHost;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, CreateMockStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, MockStreamDataConversation);

  virtual ~AudioRendererHost();

  ////////////////////////////////////////////////////////////////////////////
  // Methods called on IO thread.
  // Returns true if the message is an audio related message and should be
  // handled by this class.
  bool IsAudioRendererHostMessage(const IPC::Message& message);

  // Audio related IPC message handlers.
  // Creates an audio output stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream for the
  // required properties.
  void OnCreateStream(const IPC::Message& msg, int stream_id,
                      const ViewHostMsg_Audio_CreateStream_Params& params,
                      bool low_latency);

  // Play the audio stream referenced by |stream_id|.
  void OnPlayStream(const IPC::Message& msg, int stream_id);

  // Pause the audio stream referenced by |stream_id|.
  void OnPauseStream(const IPC::Message& msg, int stream_id);

  // Discard all audio data in  stream referenced by |stream_id|.
  void OnFlushStream(const IPC::Message& msg, int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(const IPC::Message& msg, int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(const IPC::Message& msg, int stream_id, double volume);

  // Get the volume of the audio stream referenced by |stream_id|.
  void OnGetVolume(const IPC::Message& msg, int stream_id);

  // Notify packet has been prepared for the audio stream.
  void OnNotifyPacketReady(const IPC::Message& msg, int stream_id,
                           uint32 packet_size);

  // Complete the process of creating an audio stream. This will set up the
  // shared memory or shared socket in low latency mode.
  void DoCompleteCreation(media::AudioOutputController* controller);

  // Send a state change message to the renderer.
  void DoSendPlayingMessage(media::AudioOutputController* controller);
  void DoSendPausedMessage(media::AudioOutputController* controller);

  // Request more data from the renderer. This method is used only in normal
  // latency mode.
  void DoRequestMoreData(media::AudioOutputController* controller,
                         AudioBuffersState buffers_state);

  // Handle error coming from audio stream.
  void DoHandleError(media::AudioOutputController* controller, int error_code);

  // Send an error message to the renderer.
  void SendErrorMessage(int32 render_view_id, int32 stream_id);

  // Delete all audio entry and all audio streams
  void DeleteEntries();

  // Closes the stream. The stream is then deleted in DeleteEntry() after it
  // is closed.
  void CloseAndDeleteStream(AudioEntry* entry);

  // Called on the audio thread after the audio stream is closed.
  void OnStreamClosed(AudioEntry* entry);

  // Delete an audio entry and close the related audio stream.
  void DeleteEntry(AudioEntry* entry);

  // Delete audio entry and close the related audio stream due to an error,
  // and error message is send to the renderer.
  void DeleteEntryOnError(AudioEntry* entry);

  // A helper method to look up a AudioEntry with a tuple of render view
  // id and stream id. Returns NULL if not found.
  AudioEntry* LookupById(int render_view_id, int stream_id);

  // Search for a AudioEntry having the reference to |controller|.
  // This method is used to look up an AudioEntry after a controller
  // event is received.
  AudioEntry* LookupByController(media::AudioOutputController* controller);

  // A map of id to audio sources.
  AudioEntryMap audio_entries_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
