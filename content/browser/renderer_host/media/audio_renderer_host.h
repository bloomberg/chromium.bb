// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
// singleton of AudioManager is created.
//
// Here's an example of a typical IPC dialog for audio:
//
//   Renderer                     AudioRendererHost
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

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/simple_sources.h"

namespace content {
class MediaObserver;
class ResourceContext;
}  // namespace content

namespace media {
class AudioManager;
class AudioParameters;
}

class CONTENT_EXPORT AudioRendererHost
    : public content::BrowserMessageFilter,
      public media::AudioOutputController::EventHandler {
 public:
  struct AudioEntry {
    AudioEntry();
    ~AudioEntry();

    // The AudioOutputController that manages the audio stream.
    scoped_refptr<media::AudioOutputController> controller;

    // The audio stream ID.
    int stream_id;

    // Shared memory for transmission of the audio data.
    base::SharedMemory shared_memory;

    // The synchronous reader to be used by the controller. We have the
    // ownership of the reader.
    scoped_ptr<media::AudioOutputController::SyncReader> reader;

    // Set to true after we called Close() for the controller.
    bool pending_close;
  };

  typedef std::map<int, AudioEntry*> AudioEntryMap;

  // Called from UI thread from the owner of this object.
  AudioRendererHost(media::AudioManager* audio_manager,
                    content::MediaObserver* media_observer);

  // content::BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // AudioOutputController::EventHandler implementations.
  virtual void OnCreated(media::AudioOutputController* controller) OVERRIDE;
  virtual void OnPlaying(media::AudioOutputController* controller) OVERRIDE;
  virtual void OnPaused(media::AudioOutputController* controller) OVERRIDE;
  virtual void OnError(media::AudioOutputController* controller,
                       int error_code) OVERRIDE;

 private:
  friend class AudioRendererHostTest;
  friend class content::BrowserThread;
  friend class base::DeleteHelper<AudioRendererHost>;
  friend class MockAudioRendererHost;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, CreateMockStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererHostTest, MockStreamDataConversation);

  virtual ~AudioRendererHost();

  // Methods called on IO thread ----------------------------------------------

  // Audio related IPC message handlers.
  // Creates an audio output stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream for the
  // required properties.
  void OnCreateStream(int stream_id,
                      const media::AudioParameters& params,
                      int input_channels);

  // Play the audio stream referenced by |stream_id|.
  void OnPlayStream(int stream_id);

  // Pause the audio stream referenced by |stream_id|.
  void OnPauseStream(int stream_id);

  // Discard all audio data in stream referenced by |stream_id|.
  void OnFlushStream(int stream_id);

  // Close the audio stream referenced by |stream_id|.
  void OnCloseStream(int stream_id);

  // Set the volume of the audio stream referenced by |stream_id|.
  void OnSetVolume(int stream_id, double volume);

  // Complete the process of creating an audio stream. This will set up the
  // shared memory or shared socket in low latency mode.
  void DoCompleteCreation(media::AudioOutputController* controller);

  // Send a state change message to the renderer.
  void DoSendPlayingMessage(media::AudioOutputController* controller);
  void DoSendPausedMessage(media::AudioOutputController* controller);

  // Handle error coming from audio stream.
  void DoHandleError(media::AudioOutputController* controller, int error_code);

  // Send an error message to the renderer.
  void SendErrorMessage(int stream_id);

  // Delete all audio entry and all audio streams
  void DeleteEntries();

  // Closes the stream. The stream is then deleted in DeleteEntry() after it
  // is closed.
  void CloseAndDeleteStream(AudioEntry* entry);

  // Delete an audio entry and close the related audio stream.
  void DeleteEntry(AudioEntry* entry);

  // Delete audio entry and close the related audio stream due to an error,
  // and error message is send to the renderer.
  void DeleteEntryOnError(AudioEntry* entry);

  // A helper method to look up a AudioEntry identified by |stream_id|.
  // Returns NULL if not found.
  AudioEntry* LookupById(int stream_id);

  // Search for a AudioEntry having the reference to |controller|.
  // This method is used to look up an AudioEntry after a controller
  // event is received.
  AudioEntry* LookupByController(media::AudioOutputController* controller);

  // A map of stream IDs to audio sources.
  AudioEntryMap audio_entries_;

  media::AudioManager* audio_manager_;
  content::MediaObserver* media_observer_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_RENDERER_HOST_H_
