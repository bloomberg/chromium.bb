// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_FLIP_STREAM_H_
#define NET_FLIP_FLIP_STREAM_H_

#include <string>
#include <list>

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "net/base/bandwidth_metrics.h"
#include "net/base/io_buffer.h"
#include "net/flip/flip_protocol.h"
#include "net/flip/flip_framer.h"

namespace net {

class FlipDelegate;
class HttpResponseInfo;

// The FlipStream is used by the FlipSession to represent each stream known
// on the FlipSession.
// Streams can be created either by the client or by the server.  When they
// are initiated by the client, they will usually have a delegate attached
// to them (such as a FlipNetworkTransaction).  However, when streams are
// pushed by the server, they may not have a delegate yet.
class FlipStream {
 public:
  // FlipStream constructor
  // If |delegate| is NULL, then we're receiving pushed data from the server.
  FlipStream(flip::FlipStreamId stream_id, FlipDelegate* delegate)
      : stream_id_(stream_id),
        delegate_(delegate),
        response_(NULL),
        download_finished_(false),
        metrics_(Singleton<BandwidthMetrics>::get()) {}

  virtual ~FlipStream() {
    DLOG(INFO) << "Deleting FlipStream for stream " << stream_id_;
  };

  flip::FlipStreamId stream_id() const { return stream_id_; }
  FlipDelegate* delegate() const { return delegate_; }

  // For pushed streams, we track a path to identify them.
  std::string path() const { return path_; }
  void set_path(const std::string& path) { path_ = path; }

  // Attach a delegate to a previously pushed data stream.
  // Returns true if the caller should Deactivate and delete the FlipStream
  // after this call.
  bool AttachDelegate(FlipDelegate* delegate);

  // Called when a SYN_REPLY is received on the stream.
  void OnReply(const flip::FlipHeaderBlock* headers);

  // Called when data is received on the stream.
  // Returns true if the caller should Deactivate and delete the FlipStream
  // after this call.
  bool OnData(const char* data, int length);

  // Called if the stream is being prematurely closed for an abort or an error.
  // |err| is actually a net::Error.
  // The caller should Deactivate and delete the FlipStream after this call.
  void OnError(int err);

  // Called when data has been sent on the stream.
  // |result| is either an error or the number of bytes written.
  // Returns an error if the stream is errored, or OK.
  int OnWriteComplete(int result);

 private:
  flip::FlipStreamId stream_id_;
  std::string path_;
  scoped_refptr<FlipDelegate> delegate_;
  scoped_ptr<HttpResponseInfo> response_;
  std::list<scoped_refptr<IOBufferWithSize> > response_body_;
  bool download_finished_;
  ScopedBandwidthMetrics metrics_;
};

}  // namespace net

#endif  // NET_FLIP_FLIP_STREAM_H_

