// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

// Writes audio data used for debugging purposes. All operations are
// non-blocking.
class MEDIA_EXPORT AudioDebugFileWriter {
 public:
  AudioDebugFileWriter(
      const AudioParameters& params,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
  ~AudioDebugFileWriter();

  // Must be called before calling Write() for the first time after creation or
  // Stop() call. Can be called on any sequence; Write() and Stop() must be
  // called on the same sequence as Start().
  void Start(const base::FilePath& file);

  // Must be called to finish recording. Each call to Start() requires a call to
  // Stop(). Will be automatically called on destruction.
  void Stop();

  // Write |data| to file.
  void Write(std::unique_ptr<AudioBus> data);

  // Returns true if Write() call scheduled at this point will most likely write
  // data to the file, and false if it most likely will be a no-op. The result
  // may be ambigulous if Start() or Stop() is executed at the moment. Can be
  // called from any sequence.
  bool WillWrite();

 private:
  class AudioFileWriter;

  // Deleter for AudioFileWriter.
  struct OnThreadDeleter {
   public:
    OnThreadDeleter();
    OnThreadDeleter(const OnThreadDeleter& other);
    OnThreadDeleter(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    ~OnThreadDeleter();
    void operator()(AudioFileWriter* ptr) const;

   private:
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  };

  using AudioFileWriterUniquePtr =
      std::unique_ptr<AudioFileWriter, OnThreadDeleter>;

  AudioFileWriterUniquePtr file_writer_;
  const AudioParameters params_;
  base::SequenceChecker client_sequence_checker_;

  // The task runner to do file output operations on.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AudioDebugFileWriter);
};

}  // namspace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_FILE_WRITER_H_
