// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STREAMS_STREAM_H_
#define CONTENT_BROWSER_STREAMS_STREAM_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/download/byte_stream.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

namespace net {
class IOBuffer;
}

namespace content {

class StreamHandle;
class StreamHandleImpl;
class StreamReadObserver;
class StreamRegistry;
class StreamWriteObserver;

// A stream that sends data from an arbitrary source to an internal URL
// that can be read by an internal consumer.  It will continue to pull from the
// original URL as long as there is data available.  It can be read from
// multiple clients, but only one can be reading at a time. This allows a
// reader to consume part of the stream, then pass it along to another client
// to continue processing the stream.
class CONTENT_EXPORT Stream : public base::RefCountedThreadSafe<Stream> {
 public:
  enum StreamState {
    STREAM_HAS_DATA,
    STREAM_COMPLETE,
    STREAM_EMPTY,
  };

  // Creates a stream useable from the |security_origin|.
  Stream(StreamRegistry* registry,
         StreamWriteObserver* write_observer,
         const GURL& security_origin,
         const GURL& url);

  // Sets the reader of this stream. Returns true on success, or false if there
  // is already a reader.
  bool SetReadObserver(StreamReadObserver* observer);

  // Removes the read observer.  |observer| must be the current observer.
  void RemoveReadObserver(StreamReadObserver* observer);

  // Removes the write observer.  |observer| must be the current observer.
  void RemoveWriteObserver(StreamWriteObserver* observer);

  // Adds the data in |buffer| to the stream.  Takes ownership of |buffer|.
  void AddData(scoped_refptr<net::IOBuffer> buffer, size_t size);

  // Notifies this stream that it will not be receiving any more data.
  void Finalize();

  // Reads a maximum of |buf_size| from the stream into |buf|.  Sets
  // |*bytes_read| to the number of bytes actually read.
  // Returns STREAM_HAS_DATA if data was read, STREAM_EMPTY if no data was read,
  // and STREAM_COMPLETE if the stream is finalized and all data has been read.
  StreamState ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);

  scoped_ptr<StreamHandle> CreateHandle(const GURL& original_url,
                                        const std::string& mime_type);
  void CloseHandle();

  // Indicates whether there is space in the buffer to add more data.
  bool can_add_data() const { return can_add_data_; }

  const GURL& url() const { return url_; }

  const GURL& security_origin() const { return security_origin_; }

 private:
  friend class base::RefCountedThreadSafe<Stream>;

  virtual ~Stream();

  void OnSpaceAvailable();
  void OnDataAvailable();

  size_t data_bytes_read_;
  bool can_add_data_;

  GURL security_origin_;
  GURL url_;

  scoped_refptr<net::IOBuffer> data_;
  size_t data_length_;

  scoped_ptr<ByteStreamWriter> writer_;
  scoped_ptr<ByteStreamReader> reader_;

  StreamRegistry* registry_;
  StreamReadObserver* read_observer_;
  StreamWriteObserver* write_observer_;

  StreamHandleImpl* stream_handle_;

  base::WeakPtrFactory<Stream> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(Stream);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STREAMS_STREAM_H_
