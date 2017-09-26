// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_PIPE_PRODUCER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_PIPE_PRODUCER_H_

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// Helper class which takes ownership of a ScopedDataPipeProducerHandle and
// assumes responsibility for feeding it the contents of a given file. This
// takes care of waiting for pipe capacity as needed, and can notify callers
// asynchronously when the operation is complete.
//
// Note that the FileDataPipeProducer must be kept alive until notified of
// completion to ensure that all of the intended contents are written to the
// pipe. Premature destruction may result in partial or total truncation of data
// made available to the consumer.
class MOJO_CPP_SYSTEM_EXPORT FileDataPipeProducer {
 public:
  using CompletionCallback = base::OnceCallback<void(MojoResult result)>;

  // Constructs a new FileDataPipeProducer which will write data to |producer|.
  explicit FileDataPipeProducer(ScopedDataPipeProducerHandle producer);
  ~FileDataPipeProducer();

  // Attempts to eventually write all of |file|'s contents to the pipe. Invokes
  // |callback| asynchronously when done. Note that |callback| IS allowed to
  // delete this FileDataPipeProducer.
  //
  // If the write is successful |result| will be |MOJO_RESULT_OK|. Otherwise
  // (e.g. if the producer detects the consumer is closed and the pipe has no
  // remaining capacity, or if file reads fail for any reason) |result| will be
  // |MOJO_RESULT_ABORTED|.
  //
  // Note that if the FileDataPipeProducer is destroyed before |callback| can be
  // invoked, |callback| is *never* invoked, and the write will be permanently
  // interrupted (and the producer handle closed) after making potentially only
  // partial progress.
  //
  // Multiple writes may be performed in sequence (each one after the last
  // completes), but Write() must not be called before the |callback| for the
  // previous call to Write() (if any) has returned.
  void WriteFromFile(base::File file, CompletionCallback callback);

  // Same as above but takes a FilePath instead of an opened File. Opens the
  // file on an appropriate sequence and then proceeds as WriteFromFile() would.
  void WriteFromPath(const base::FilePath& path, CompletionCallback callback);

 private:
  class FileSequenceState;

  void InitializeNewRequest(CompletionCallback callback);
  void OnWriteComplete(CompletionCallback callback,
                       ScopedDataPipeProducerHandle producer,
                       MojoResult result);

  ScopedDataPipeProducerHandle producer_;
  scoped_refptr<FileSequenceState> file_sequence_state_;
  base::WeakPtrFactory<FileDataPipeProducer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileDataPipeProducer);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_PIPE_PRODUCER_H_
