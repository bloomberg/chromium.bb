// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SYNC_READER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SYNC_READER_H_

#include "base/file_descriptor_posix.h"
#include "base/process.h"
#include "base/sync_socket.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "media/audio/audio_output_controller.h"
#include "media/base/audio_bus.h"

namespace base {
class SharedMemory;
}

// A AudioOutputController::SyncReader implementation using SyncSocket. This
// is used by AudioOutputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class AudioSyncReader : public media::AudioOutputController::SyncReader {
 public:
  AudioSyncReader(base::SharedMemory* shared_memory,
                  const media::AudioParameters& params,
                  int input_channels);

  virtual ~AudioSyncReader();

  // media::AudioOutputController::SyncReader implementations.
  virtual void UpdatePendingBytes(uint32 bytes) OVERRIDE;
  virtual int Read(media::AudioBus* source, media::AudioBus* dest) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool DataReady() OVERRIDE;

  bool Init();
  bool PrepareForeignSocketHandle(base::ProcessHandle process_handle,
#if defined(OS_WIN)
                                  base::SyncSocket::Handle* foreign_handle);
#else
                                  base::FileDescriptor* foreign_handle);
#endif

 private:
  base::SharedMemory* shared_memory_;
  base::Time previous_call_time_;

  // Number of input channels for synchronized I/O.
  int input_channels_;

  // Socket for transmitting audio data.
  scoped_ptr<base::CancelableSyncSocket> socket_;

  // Socket to be used by the renderer. The reference is released after
  // PrepareForeignSocketHandle() is called and ran successfully.
  scoped_ptr<base::CancelableSyncSocket> foreign_socket_;

  // Shared memory wrapper used for transferring audio data to Read() callers.
  scoped_ptr<media::AudioBus> output_bus_;

  // Shared memory wrapper used for transferring audio data from Read() callers.
  scoped_ptr<media::AudioBus> input_bus_;

  // Maximum amount of audio data which can be transferred in one Read() call.
  int packet_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioSyncReader);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SYNC_READER_H_
