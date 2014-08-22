// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BUFFERING_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BUFFERING_FILE_STREAM_READER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/blob/file_stream_reader.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace file_system_provider {

// Wraps the file stream reader implementation with a prefetching buffer.
// Reads data from the internal file stream reader in chunks of size at least
// |buffer_size| bytes (or less for the last chunk, because of EOF).
//
// The underlying inner file stream reader *must not* return any values
// synchronously. Instead, results must be returned by a callback, including
// errors.
class BufferingFileStreamReader : public storage::FileStreamReader {
 public:
  BufferingFileStreamReader(
      scoped_ptr<storage::FileStreamReader> file_stream_reader,
      int buffer_size);

  virtual ~BufferingFileStreamReader();

  // storage::FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf,
                   int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  // Copies data from the preloading buffer and updates the internal iterator.
  // Returns number of bytes successfully copied.
  int CopyFromPreloadingBuffer(scoped_refptr<net::IOBuffer> buffer,
                               int buffer_length);

  // Preloads data from the internal stream reader and calls the |callback|.
  void Preload(const net::CompletionCallback& callback);

  // Called when preloading of a buffer chunk is finished. Updates state of the
  // preloading buffer and copied requested data to the |buffer|.
  void OnPreloadCompleted(scoped_refptr<net::IOBuffer> buffer,
                          int buffer_length,
                          const net::CompletionCallback& callback,
                          int result);

  scoped_ptr<storage::FileStreamReader> file_stream_reader_;
  int buffer_size_;
  scoped_refptr<net::IOBuffer> preloading_buffer_;
  int preloading_buffer_offset_;
  int buffered_bytes_;

  base::WeakPtrFactory<BufferingFileStreamReader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BufferingFileStreamReader);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BUFFERING_FILE_STREAM_READER_H_
