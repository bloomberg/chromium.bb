// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_

#include "base/atomic_ref_count.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_source_diverter.h"
#include "media/audio/simple_sources.h"
#include "media/base/media_export.h"

// An AudioOutputController controls an AudioOutputStream and provides data
// to this output stream. It has an important function that it executes
// audio operations like play, pause, stop, etc. on a separate thread,
// namely the audio manager thread.
//
// All the public methods of AudioOutputController are non-blocking.
// The actual operations are performed on the audio manager thread.
//
// Here is a state transition diagram for the AudioOutputController:
//
//   *[ Empty ]  -->  [ Created ]  -->  [ Starting ]  -->  [ Playing ]  --.
//        |                |               |    ^               |         |
//        |                |               |    |               |         |
//        |                |               |    |               v         |
//        |                |               |    `---------  [ Paused ]    |
//        |                |               |                    |         |
//        |                v               v                    |         |
//        `----------->  [      Closed       ]  <-------------------------'
//
// * Initial state
//
// At any time after reaching the Created state but before Closed, the
// AudioOutputController may be notified of a device change via
// OnDeviceChange().  As the OnDeviceChange() is processed, state transitions
// will occur, ultimately ending up in an equivalent pre-call state.  E.g., if
// the state was Paused, the new state will be Created, since these states are
// all functionally equivalent and require a Play() call to continue to the next
// state.
//
// The AudioOutputStream can request data from the AudioOutputController via the
// AudioSourceCallback interface. AudioOutputController uses the SyncReader
// passed to it via construction to synchronously fulfill this read request.
//
// Since AudioOutputController uses AudioManager's message loop the controller
// uses WeakPtr to allow safe cancellation of pending tasks.
//

namespace media {

class AudioSilenceDetector;

class MEDIA_EXPORT AudioOutputController
    : public base::RefCountedThreadSafe<AudioOutputController>,
      public AudioOutputStream::AudioSourceCallback,
      public AudioSourceDiverter,
      NON_EXPORTED_BASE(public AudioManager::AudioDeviceListener)  {
 public:
  // An event handler that receives events from the AudioOutputController. The
  // following methods are called on the audio manager thread.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual void OnCreated() = 0;
    virtual void OnPlaying() = 0;
    virtual void OnAudible(bool is_audible) = 0;
    virtual void OnPaused() = 0;
    virtual void OnError() = 0;
    virtual void OnDeviceChange(int new_buffer_size, int new_sample_rate) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous reader interface used by AudioOutputController for
  // synchronous reading.
  // TODO(crogers): find a better name for this class and the Read() method
  // now that it can handle synchronized I/O.
  class SyncReader {
   public:
    virtual ~SyncReader() {}

    // Notify the synchronous reader the number of bytes in the
    // AudioOutputController not yet played. This is used by SyncReader to
    // prepare more data and perform synchronization.
    virtual void UpdatePendingBytes(uint32 bytes) = 0;

    // Attempt to completely fill |dest|, return the actual number of
    // frames that could be read.
    // |source| may optionally be provided for input data.
    virtual int Read(AudioBus* source, AudioBus* dest) = 0;

    // Close this synchronous reader.
    virtual void Close() = 0;

    // Check if data is ready.
    virtual bool DataReady() = 0;
  };

  // Factory method for creating an AudioOutputController.
  // This also creates and opens an AudioOutputStream on the audio manager
  // thread, and if this is successful, the |event_handler| will receive an
  // OnCreated() call from the same audio manager thread.  |audio_manager| must
  // outlive AudioOutputController.
  static scoped_refptr<AudioOutputController> Create(
      AudioManager* audio_manager, EventHandler* event_handler,
      const AudioParameters& params, SyncReader* sync_reader);

  // Methods to control playback of the stream.

  // Starts the playback of this audio output stream.
  void Play();

  // Pause this audio output stream.
  void Pause();

  // Discard all audio data buffered in this output stream. This method only
  // has effect when the stream is paused.
  void Flush();

  // Closes the audio output stream. The state is changed and the resources
  // are freed on the audio manager thread. closed_task is executed after that.
  // Callbacks (EventHandler and SyncReader) must exist until closed_task is
  // called.
  //
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  void Close(const base::Closure& closed_task);

  // Sets the volume of the audio output stream.
  void SetVolume(double volume);

  // AudioSourceCallback implementation.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) OVERRIDE;
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnError(AudioOutputStream* stream) OVERRIDE;
  // Deprecated: Currently only used for starting audio playback and for audio
  // mirroring.
  virtual void WaitTillDataReady() OVERRIDE;

  // AudioDeviceListener implementation.  When called AudioOutputController will
  // shutdown the existing |stream_|, transition to the kRecreating state,
  // create a new stream, and then transition back to an equivalent state prior
  // to being called.
  virtual void OnDeviceChange() OVERRIDE;

  // AudioSourceDiverter implementation.
  virtual const AudioParameters& GetAudioParameters() OVERRIDE;
  virtual void StartDiverting(AudioOutputStream* to_stream) OVERRIDE;
  virtual void StopDiverting() OVERRIDE;

 protected:
  // Internal state of the source.
  enum State {
    kEmpty,
    kCreated,
    kStarting,
    kPlaying,
    kPaused,
    kClosed,
    kError,
  };

  friend class base::RefCountedThreadSafe<AudioOutputController>;
  virtual ~AudioOutputController();

 private:
  // We are polling sync reader if data became available.
  static const int kPollNumAttempts;
  static const int kPollPauseInMilliseconds;

  AudioOutputController(AudioManager* audio_manager, EventHandler* handler,
                        const AudioParameters& params, SyncReader* sync_reader);

  // The following methods are executed on the audio manager thread.
  void DoCreate(bool is_for_device_change);
  void DoPlay();
  void PollAndStartIfDataReady();
  void DoPause();
  void DoFlush();
  void DoClose();
  void DoSetVolume(double volume);
  void DoReportError();
  void DoStartDiverting(AudioOutputStream* to_stream);
  void DoStopDiverting();

  // Called at regular intervals during playback to check for a change in
  // silence and call EventHandler::OnAudible() when state changes occur.
  void MaybeInvokeAudibleCallback();

  // Helper methods that start/stop physical stream.
  void StartStream();
  void StopStream();

  // Helper method that stops, closes, and NULLs |*stream_|.
  void DoStopCloseAndClearStream();

  // Sanity-check that entry/exit to OnMoreIOData() by the hardware audio thread
  // happens only between AudioOutputStream::Start() and Stop().
  void AllowEntryToOnMoreIOData();
  void DisallowEntryToOnMoreIOData();

  AudioManager* const audio_manager_;
  const AudioParameters params_;
  EventHandler* const handler_;

  // Note: It's important to invalidate the weak pointers whenever stream_ is
  // changed.  See comment for weak_this_.
  AudioOutputStream* stream_;

  // When non-NULL, audio is being diverted to this stream.
  AudioOutputStream* diverting_to_stream_;

  // The current volume of the audio stream.
  double volume_;

  // |state_| is written on the audio manager thread and is read on the
  // hardware audio thread. These operations need to be locked. But lock
  // is not required for reading on the audio manager thread.
  State state_;

  // Binary semaphore, used to ensure that only one thread enters the
  // OnMoreIOData() method, and only when it is valid to do so.  This is for
  // sanity-checking the behavior of platform implementations of
  // AudioOutputStream.  In other words, multiple contention is not expected,
  // nor in the design here.
  base::AtomicRefCount num_allowed_io_;

  // SyncReader is used only in low latency mode for synchronous reading.
  SyncReader* const sync_reader_;

  // The message loop of audio manager thread that this object runs on.
  const scoped_refptr<base::MessageLoopProxy> message_loop_;

  // When starting stream we wait for data to become available.
  // Number of times left.
  int number_polling_attempts_left_;

  // Used to auto-cancel the delayed tasks that are created to poll for data
  // (when starting-up a stream).
  base::WeakPtrFactory<AudioOutputController> weak_this_;

  // Scans audio samples from OnMoreIOData() as input and causes
  // EventHandler::OnAudbile() to be called whenever a transition to a period of
  // silence or non-silence is detected.
  scoped_ptr<AudioSilenceDetector> silence_detector_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_CONTROLLER_H_
