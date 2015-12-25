// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/move.h"
#include "media/audio/audio_input_writer.h"

namespace media {

class AudioBus;
class AudioBusRefCounted;

}  // namespace media

namespace content {

// Writes audio input data used for debugging purposes. Can be created on any
// thread. Must be destroyed on the FILE thread. Write call can be made on any
// thread. This object must be unregistered in Write caller before destroyed.
// When created, it takes ownership of |file|.
class AudioInputDebugWriter : public media::AudioInputWriter {
 public:
  explicit AudioInputDebugWriter(base::File file);

  ~AudioInputDebugWriter() override;

  // Write data from |data| to file.
  void Write(scoped_ptr<media::AudioBus> data) override;

 private:
  // Write data from |data| to file. Called on the FILE thread.
  void DoWrite(scoped_ptr<media::AudioBus> data);

  // The file to write to.
  base::File file_;

  // Intermediate buffer to be written to file. Interleaved 16 bit audio data.
  scoped_ptr<int16_t[]> interleaved_data_;
  int interleaved_data_size_;

  base::WeakPtrFactory<AudioInputDebugWriter> weak_factory_;
};

}  // namspace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_
