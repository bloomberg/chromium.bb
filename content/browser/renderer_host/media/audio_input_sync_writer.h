// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_

#include "base/memory/scoped_vector.h"
#include "base/process/process.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/audio/audio_input_controller.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace content {

// A AudioInputController::SyncWriter implementation using SyncSocket. This
// is used by AudioInputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class CONTENT_EXPORT AudioInputSyncWriter
    : public media::AudioInputController::SyncWriter {
 public:
  AudioInputSyncWriter(void* shared_memory,
                       size_t shared_memory_size,
                       int shared_memory_segment_count,
                       const media::AudioParameters& params);

  ~AudioInputSyncWriter() override;

  // media::AudioInputController::SyncWriter implementation.
  void Write(const media::AudioBus* data,
             double volume,
             bool key_pressed,
             uint32 hardware_delay_bytes) override;
  void Close() override;

  bool Init();
  bool PrepareForeignSocket(base::ProcessHandle process_handle,
                            base::SyncSocket::TransitDescriptor* descriptor);

 protected:
  // Socket for transmitting audio data.
  scoped_ptr<base::CancelableSyncSocket> socket_;

 private:
  // Virtual function for native logging to be able to override in tests.
  virtual void AddToNativeLog(const std::string& message);

  uint8* shared_memory_;
  uint32 shared_memory_segment_size_;
  uint32 shared_memory_segment_count_;
  uint32 current_segment_id_;

  // Socket to be used by the renderer. The reference is released after
  // PrepareForeignSocketHandle() is called and ran successfully.
  scoped_ptr<base::CancelableSyncSocket> foreign_socket_;

  // The time of the creation of this object.
  base::Time creation_time_;

  // The time of the last Write call.
  base::Time last_write_time_;

  // Size in bytes of each audio bus.
  const int audio_bus_memory_size_;

  // Increasing ID used for checking audio buffers are in correct sequence at
  // read side.
  uint32_t next_buffer_id_;

  // Next expected audio buffer index to have been read at the other side. We
  // will get the index read at the other side over the socket. Note that this
  // index does not correspond to |next_buffer_id_|, it's two separate counters.
  uint32_t next_read_buffer_index_;

  // Keeps track of number of filled buffer segments in the ring buffer to
  // ensure the we don't overwrite data that hasn't been read yet.
  int number_of_filled_segments_;

  // Counts the total number of calls to Write() and number of failures due to
  // ring buffer being full.
  size_t write_count_;
  size_t write_error_count_;

  // Counts how many errors we get during renderer process teardown so that we
  // can account for that (subtract) when we calculate the overall error count.
  size_t trailing_error_count_;

  // Vector of audio buses allocated during construction and deleted in the
  // destructor.
  ScopedVector<media::AudioBus> audio_buses_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioInputSyncWriter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
