// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
#pragma once

#include "base/file_descriptor_posix.h"
#include "base/process.h"
#include "base/sync_socket.h"
#include "media/audio/audio_input_controller.h"

namespace base {
class SharedMemory;
}

// A AudioInputController::SyncWriter implementation using SyncSocket. This
// is used by AudioInputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class AudioInputSyncWriter : public media::AudioInputController::SyncWriter {
 public:
  explicit AudioInputSyncWriter(base::SharedMemory* shared_memory);

  virtual ~AudioInputSyncWriter();

  // media::AudioOutputController::SyncWriter implementation.
  virtual void UpdateRecordedBytes(uint32 bytes) OVERRIDE;
  virtual uint32 Write(const void* data, uint32 size) OVERRIDE;
  virtual void Close() OVERRIDE;

  bool Init();
  bool PrepareForeignSocketHandle(base::ProcessHandle process_handle,
#if defined(OS_WIN)
                                  base::SyncSocket::Handle* foreign_handle);
#else
                                  base::FileDescriptor* foreign_handle);
#endif

 private:
  base::SharedMemory* shared_memory_;

  // A pair of SyncSocket for transmitting audio data.
  scoped_ptr<base::SyncSocket> socket_;

  // SyncSocket to be used by the renderer. The reference is released after
  // PrepareForeignSocketHandle() is called and ran successfully.
  scoped_ptr<base::SyncSocket> foreign_socket_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioInputSyncWriter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
