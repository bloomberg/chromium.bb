// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_READAHEAD_FILE_STREAM_READER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_READAHEAD_FILE_STREAM_READER_H_

#include <queue>

#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "webkit/browser/blob/file_stream_reader.h"

// Wraps a source FileStreamReader with a readahead buffer.
class ReadaheadFileStreamReader
    : public NON_EXPORTED_BASE(webkit_blob::FileStreamReader) {
 public:
  // Takes ownership of |source|.
  explicit ReadaheadFileStreamReader(webkit_blob::FileStreamReader* source);

  virtual ~ReadaheadFileStreamReader();

  // FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  // Returns the number of bytes consumed from the internal cache into |sink|.
  // Returns an error code if we are out of cache, hit an error, or hit EOF.
  int FinishReadFromCacheOrStoredError(net::DrainableIOBuffer* sink);

  // Reads into a new buffer from the source reader. This calls
  // OnFinishReadFromSource when it completes (either synchronously or
  // asynchronously).
  void ReadFromSourceIfNeeded();
  void OnFinishReadFromSource(net::IOBuffer* buffer, int result);

  // This is reset to NULL upon encountering a read error or EOF.
  scoped_ptr<webkit_blob::FileStreamReader> source_;

  // This stores the error or EOF from the source FileStreamReader. Its
  // value is undefined if |source_| is non-NULL.
  int source_error_;
  bool source_has_pending_read_;

  // This contains a queue of buffers filled from |source_|, waiting to be
  // consumed.
  std::queue<scoped_refptr<net::DrainableIOBuffer> > buffers_;

  // The read buffer waiting for the source FileStreamReader to finish
  // reading and fill the cache.
  scoped_refptr<net::DrainableIOBuffer> pending_sink_buffer_;
  net::CompletionCallback pending_read_callback_;

  base::WeakPtrFactory<ReadaheadFileStreamReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ReadaheadFileStreamReader);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_READAHEAD_FILE_STREAM_READER_H_
