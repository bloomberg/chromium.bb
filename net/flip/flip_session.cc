// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_session.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/flip/flip_frame_builder.h"
#include "net/flip/flip_protocol.h"
#include "net/flip/flip_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/tools/dump_cache/url_to_filename_encoder.h"

namespace net {

// static
bool FlipSession::use_ssl_ = true;

FlipSession::FlipSession(std::string host, HttpNetworkSession* session)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &FlipSession::OnTCPConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          ssl_connect_callback_(this, &FlipSession::OnSSLConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &FlipSession::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &FlipSession::OnWriteComplete)),
      domain_(host),
      session_(session),
      connection_started_(false),
      connection_ready_(false),
      read_buffer_(new IOBuffer(kReadBufferSize)),
      read_pending_(false),
      stream_hi_water_mark_(1),  // Always start at 1 for the first stream id.
      delayed_write_pending_(false),
      write_pending_(false) {
  // TODO(mbelshe): consider randomization of the stream_hi_water_mark.

  flip_framer_.set_visitor(this);

  session_->ssl_config_service()->GetSSLConfig(&ssl_config_);
}

FlipSession::~FlipSession() {
  // Cleanup all the streams.
  CloseAllStreams(net::ERR_ABORTED);

  if (connection_.is_initialized()) {
    // With Flip we can't recycle sockets.
    connection_.socket()->Disconnect();
  }

  session_->flip_session_pool()->Remove(this);
}

net::Error FlipSession::Connect(const std::string& group_name,
                                const HostResolver::RequestInfo& host,
                                int priority) {
  DCHECK(priority >= 0 && priority < 4);

  // If the connect process is started, let the caller continue.
  if (connection_started_)
    return net::OK;

  connection_started_ = true;

  static StatsCounter flip_sessions("flip.sessions");
  flip_sessions.Increment();

  int rv = connection_.Init(group_name, host, priority, &connect_callback_,
                            session_->tcp_socket_pool(), NULL);

  // If the connect is pending, we still return ok.  The APIs enqueue
  // work until after the connect completes asynchronously later.
  if (rv == net::ERR_IO_PENDING)
    return net::OK;
  return static_cast<net::Error>(rv);
}


// Create a FlipHeaderBlock for a Flip SYN_STREAM Frame from
// a HttpRequestInfo block.
void CreateFlipHeadersFromHttpRequest(
    const HttpRequestInfo* info, flip::FlipHeaderBlock* headers) {
  static const std::string kHttpProtocolVersion("HTTP/1.1");

  HttpUtil::HeadersIterator it(info->extra_headers.begin(),
                               info->extra_headers.end(),
                               "\r\n");
  while (it.GetNext()) {
    std::string name = StringToLowerASCII(it.name());
    (*headers)[name] = it.values();
  }

  (*headers)["method"] = info->method;
  (*headers)["url"] = info->url.PathForRequest();
  (*headers)["version"] = kHttpProtocolVersion;
  (*headers)["host"] = GetHostAndOptionalPort(info->url);
  if (info->user_agent.length())
    (*headers)["user-agent"] = info->user_agent;
  if (!info->referrer.is_empty())
    (*headers)["referer"] = info->referrer.spec();

  // Honor load flags that impact proxy caches.
  if (info->load_flags & LOAD_BYPASS_CACHE) {
    (*headers)["pragma"] = "no-cache";
    (*headers)["cache-control"] = "no-cache";
  } else if (info->load_flags & LOAD_VALIDATE_CACHE) {
    (*headers)["cache-control"] = "max-age=0";
  }
}

int FlipSession::CreateStream(FlipDelegate* delegate) {
  flip::FlipStreamId stream_id = GetNewStreamId();

  GURL url = delegate->request()->url;
  std::string path = url.PathForRequest();

  FlipStream* stream = NULL;

  // Check if we have a push stream for this path.
  if (delegate->request()->method == "GET") {
    stream = GetPushStream(path);
    if (stream) {
      if (stream->AttachDelegate(delegate)) {
        DeactivateStream(stream->stream_id());
        delete stream;
        return 0;
      }
      return stream->stream_id();
    }
  }

  // Check if we have a pending push stream for this url.
  std::string url_path = delegate->request()->url.PathForRequest();
  std::map<std::string, FlipDelegate*>::iterator it;
  it = pending_streams_.find(url_path);
  if (it != pending_streams_.end()) {
    DCHECK(it->second == NULL);
    it->second = delegate;
    return 0;  // TODO(mbelshe): this is overloaded with the fail case.
  }

  // If we still don't have a stream, activate one now.
  stream = ActivateStream(stream_id, delegate);
  if (!stream)
    return net::ERR_FAILED;

  LOG(INFO) << "FlipStream: Creating stream " << stream_id << " for "
            << delegate->request()->url;

  // TODO(mbelshe): Optimize memory allocations
  int priority = delegate->request()->priority;

  // Hack for the priorities
  // TODO(mbelshe): These need to be plumbed through the Http Network Stack.
  if (path.find(".css") != path.npos) {
    priority = 1;
  } else if (path.find(".html") != path.npos) {
    priority = 0;
  } else if (path.find(".js") != path.npos) {
    priority = 1;
  } else {
    priority = 3;
  }

  DCHECK(priority >= FLIP_PRIORITY_HIGHEST &&
         priority <= FLIP_PRIORITY_LOWEST);

  // Convert from HttpRequestHeaders to Flip Headers.
  flip::FlipHeaderBlock headers;
  CreateFlipHeadersFromHttpRequest(delegate->request(), &headers);

  flip::FlipControlFlags flags = flip::CONTROL_FLAG_NONE;
  if (!delegate->data())
    flags = flip::CONTROL_FLAG_FIN;

  // Create a SYN_STREAM packet and add to the output queue.
  flip::FlipSynStreamControlFrame* syn_frame =
      flip_framer_.CreateSynStream(stream_id, priority, flags, false, &headers);
  int length = sizeof(flip::FlipFrame) + syn_frame->length();
  IOBufferWithSize* buffer =
      new IOBufferWithSize(length);
  memcpy(buffer->data(), syn_frame, length);
  delete[] syn_frame;
  queue_.push(FlipIOBuffer(buffer, priority, stream));

  static StatsCounter flip_requests("flip.requests");
  flip_requests.Increment();

  LOG(INFO) << "FETCHING: " << delegate->request()->url.spec();


  // TODO(mbelshe): Implement POST Data here

  // Schedule to write to the socket after we've made it back
  // to the message loop so that we can aggregate multiple
  // requests.
  // TODO(mbelshe): Should we do the "first" request immediately?
  //                maybe we should only 'do later' for subsequent
  //                requests.
  WriteSocketLater();

  return stream_id;
}

bool FlipSession::CancelStream(int id) {
  LOG(INFO) << "Cancelling stream " << id;
  if (!IsStreamActive(id))
    return false;

  // TODO(mbelshe): Write a method for tearing down a stream
  //                that cleans it out of the active list, the pending list,
  //                etc.
  FlipStream* stream = active_streams_[id];
  DeactivateStream(id);
  delete stream;
  return true;
}

bool FlipSession::IsStreamActive(int id) const {
  return active_streams_.find(id) != active_streams_.end();
}

LoadState FlipSession::GetLoadState() const {
  // TODO(mbelshe): needs more work
  return LOAD_STATE_CONNECTING;
}

void FlipSession::OnTCPConnect(int result) {
  LOG(INFO) << "Flip socket connected (result=" << result << ")";

  if (result != net::OK) {
    net::Error err = static_cast<net::Error>(result);
    CloseAllStreams(err);
    this->Release();
    return;
  }

  // Adjust socket buffer sizes.
  // FLIP uses one socket, and we want a really big buffer.
  // This greatly helps on links with packet loss - we can even
  // outperform Vista's dynamic window sizing algorithm.
  // TODO(mbelshe): more study.
  const int kSocketBufferSize = 512 * 1024;
  connection_.socket()->SetReceiveBufferSize(kSocketBufferSize);
  connection_.socket()->SetSendBufferSize(kSocketBufferSize);

  if (use_ssl_) {
    // Add a SSL socket on top of our existing transport socket.
    ClientSocket* socket = connection_.release_socket();
    // TODO(mbelshe): Fix the hostname.  This is BROKEN without having
    //                a real hostname.
    socket = session_->socket_factory()->CreateSSLClientSocket(
        socket, "" /* request_->url.HostNoBrackets() */ , ssl_config_);
    connection_.set_socket(socket);
    // TODO(willchan): Plumb LoadLog into FLIP code.
    int status = connection_.socket()->Connect(&ssl_connect_callback_, NULL);
    CHECK(status == net::ERR_IO_PENDING);
  } else {
    connection_ready_ = true;

    // Make sure we get any pending data sent.
    WriteSocketLater();
    // Start reading
    ReadSocket();
  }
}

void FlipSession::OnSSLConnect(int result) {
  // TODO(mbelshe): We need to replicate the functionality of
  //   HttpNetworkTransaction::DoSSLConnectComplete here, where it calls
  //   HandleCertificateError() and such.
  if (IsCertificateError(result))
    result = OK;   // TODO(mbelshe): pretend we're happy anyway.

  if (result == OK) {
    connection_ready_ = true;

    // After we've connected, send any data to the server, and then issue
    // our read.
    WriteSocketLater();
    ReadSocket();
  } else {
    NOTREACHED();
    // TODO(mbelshe): handle the error case: could not connect
  }
}

void FlipSession::OnReadComplete(int bytes_read) {
  // Parse a frame.  For now this code requires that the frame fit into our
  // buffer (32KB).
  // TODO(mbelshe): support arbitrarily large frames!

  LOG(INFO) << "Flip socket read: " << bytes_read << " bytes";

  read_pending_ = false;

  if (bytes_read <= 0) {
    // Session is tearing down.
    net::Error err = static_cast<net::Error>(bytes_read);
    CloseAllStreams(err);
    this->Release();
    return;
  }

  char *data = read_buffer_->data();
  while (bytes_read &&
         flip_framer_.error_code() == flip::FlipFramer::FLIP_NO_ERROR) {
    uint32 bytes_processed = flip_framer_.ProcessInput(data, bytes_read);
    bytes_read -= bytes_processed;
    data += bytes_processed;
    if (flip_framer_.state() == flip::FlipFramer::FLIP_DONE)
      flip_framer_.Reset();
  }
  // NOTE(mbelshe): Could cause circular callbacks. (when ReadSocket
  //    completes synchronously, calling OnReadComplete, etc).  Should
  //    probably return to the message loop.
  ReadSocket();
}

void FlipSession::OnWriteComplete(int result) {
  DCHECK(write_pending_);
  write_pending_ = false;

  LOG(INFO) << "Flip write complete (result=" << result << ")";

  if (result >= 0) {
    // TODO(mbelshe) Verify that we wrote ALL the bytes we needed to.
    //               The code current is broken in the case of a partial write.
    DCHECK_EQ(static_cast<size_t>(result), in_flight_write_.size());

    // Cleanup the write which just completed.
    in_flight_write_.release();

    // Write more data.  We're already in a continuation, so we can
    // go ahead and write it immediately (without going back to the
    // message loop).
    WriteSocketLater();
  } else {
    // TODO(mbelshe): Deal with result < 0 error case.
    NOTIMPLEMENTED();
  }
}

void FlipSession::ReadSocket() {
  if (read_pending_)
    return;

  int bytes_read = connection_.socket()->Read(read_buffer_.get(),
                                              kReadBufferSize,
                                              &read_callback_);
  switch (bytes_read) {
    case 0:
      // Socket is closed!
      // TODO(mbelshe): Need to abort any active streams here.
      DCHECK(!active_streams_.size());
      return;
    case net::ERR_IO_PENDING:
      // Waiting for data.  Nothing to do now.
      read_pending_ = true;
      return;
    default:
      // Data was read, process it.
      // TODO(mbelshe): check that we can't get a recursive stack?
      OnReadComplete(bytes_read);
      break;
  }
}

void FlipSession::WriteSocketLater() {
  if (delayed_write_pending_)
    return;

  delayed_write_pending_ = true;
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &FlipSession::WriteSocket));
}

void FlipSession::WriteSocket() {
  // This function should only be called via WriteSocketLater.
  DCHECK(delayed_write_pending_);
  delayed_write_pending_ = false;

  // If the socket isn't connected yet, just wait; we'll get called
  // again when the socket connection completes.
  if (!connection_ready_)
    return;

  if (write_pending_)   // Another write is in progress still.
    return;

  // Loop sending frames until we've sent everything or until the write
  // returns error (or ERR_IO_PENDING).
  while (queue_.size()) {
    // Grab the next FlipFrame to send.
    FlipIOBuffer next_buffer = queue_.top();
    queue_.pop();

    // We've deferred compression until just before we write it to the socket,
    // which is now.
    flip::FlipFrame* uncompressed_frame =
        reinterpret_cast<flip::FlipFrame*>(next_buffer.buffer()->data());
    scoped_array<flip::FlipFrame> compressed_frame(
        flip_framer_.CompressFrame(uncompressed_frame));
    size_t size = compressed_frame.get()->length() + sizeof(flip::FlipFrame);

    DCHECK(size > 0);

    // TODO(mbelshe): We have too much copying of data here.
    IOBufferWithSize* buffer = new IOBufferWithSize(size);
    memcpy(buffer->data(), compressed_frame.get(), size);

    // Attempt to send the frame.
    in_flight_write_ = FlipIOBuffer(buffer, 0, next_buffer.stream());
    write_pending_ = true;
    int rv = connection_.socket()->Write(in_flight_write_.buffer(),
                                         size, &write_callback_);
    if (rv == net::ERR_IO_PENDING)
      break;

    // We sent the frame successfully.
    OnWriteComplete(rv);

    // TODO(mbelshe):  Test this error case.  Maybe we should mark the socket
    //                 as in an error state.
    if (rv < 0)
      break;
  }
}

void FlipSession::CloseAllStreams(net::Error code) {
  LOG(INFO) << "Closing all FLIP Streams";

  static StatsCounter abandoned_streams("flip.abandoned_streams");
  static StatsCounter abandoned_push_streams("flip.abandoned_push_streams");

  if (active_streams_.size()) {
    abandoned_streams.Add(active_streams_.size());

    // Create a copy of the list, since aborting streams can invalidate
    // our list.
    FlipStream** list = new FlipStream*[active_streams_.size()];
    ActiveStreamMap::const_iterator it;
    int index = 0;
    for (it = active_streams_.begin(); it != active_streams_.end(); ++it)
      list[index++] = it->second;

    // Issue the aborts.
    for (--index; index >= 0; index--) {
      LOG(ERROR) << "ABANDONED (stream_id=" << list[index]->stream_id()
                 << "): " << list[index]->path();
      list[index]->OnError(ERR_ABORTED);
      delete list[index];
    }

    // Clear out anything pending.
    active_streams_.clear();

    delete[] list;
  }

  if (pushed_streams_.size()) {
    abandoned_push_streams.Add(pushed_streams_.size());
    pushed_streams_.clear();
  }
}

int FlipSession::GetNewStreamId() {
  int id = stream_hi_water_mark_;
  stream_hi_water_mark_ += 2;
  if (stream_hi_water_mark_ > 0x7fff)
    stream_hi_water_mark_ = 1;
  return id;
}

FlipStream* FlipSession::ActivateStream(flip::FlipStreamId id,
                                            FlipDelegate* delegate) {
  DCHECK(!IsStreamActive(id));

  FlipStream* stream = new FlipStream(id, delegate);
  active_streams_[id] = stream;
  return stream;
}

void FlipSession::DeactivateStream(flip::FlipStreamId id) {
  DCHECK(IsStreamActive(id));

  // Verify it is not on the pushed_streams_ list.
  ActiveStreamList::iterator it;
  for (it = pushed_streams_.begin(); it != pushed_streams_.end(); ++it) {
    FlipStream* impl = *it;
    if (id == impl->stream_id()) {
      pushed_streams_.erase(it);
      break;
    }
  }

  active_streams_.erase(id);
}

FlipStream* FlipSession::GetPushStream(std::string path) {
  static StatsCounter used_push_streams("flip.claimed_push_streams");

  LOG(INFO) << "Looking for push stream: " << path;

  // We just walk a linear list here.
  ActiveStreamList::iterator it;
  for (it = pushed_streams_.begin(); it != pushed_streams_.end(); ++it) {
    FlipStream* impl = *it;
    if (path == impl->path()) {
      pushed_streams_.erase(it);
      used_push_streams.Increment();
      LOG(INFO) << "Push Stream Claim for: " << path;
      return impl;
    }
  }
  return NULL;
}

void FlipSession::OnError(flip::FlipFramer* framer) {
  LOG(ERROR) << "FlipSession error!";
  CloseAllStreams(net::ERR_UNEXPECTED);
  Release();
}

void FlipSession::OnStreamFrameData(flip::FlipStreamId stream_id,
                                    const char* data,
                                    size_t len) {
  LOG(INFO) << "Flip data for stream " << stream_id << ", " << len << " bytes";
  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    LOG(WARNING) << "Received data frame for invalid stream " << stream_id;
    return;
  }

  FlipStream* stream = active_streams_[stream_id];
  if (stream->OnData(data, len)) {
    DeactivateStream(stream->stream_id());
    delete stream;
  }
}

void FlipSession::OnSyn(const flip::FlipSynStreamControlFrame* frame,
                        const flip::FlipHeaderBlock* headers) {
  flip::FlipStreamId stream_id = frame->stream_id();

  // Server-initiated streams should have even sequence numbers.
  if ((stream_id & 0x1) != 0) {
    LOG(ERROR) << "Received invalid OnSyn stream id " << stream_id;
    return;
  }

  if (IsStreamActive(stream_id)) {
    LOG(ERROR) << "Received OnSyn for active stream " << stream_id;
    return;
  }

  // Activate a stream and parse the headers.
  FlipStream* stream = ActivateStream(stream_id, NULL);
  stream->OnReply(headers);

  // TODO(mbelshe): DCHECK that this is a GET method?

  // Verify that the response had a URL for us.
  DCHECK(stream->path().length() != 0);
  if (stream->path().length() == 0) {
    LOG(WARNING) << "Pushed stream did not contain a path.";
    DeactivateStream(stream_id);
    delete stream;
    return;
  }

  // Check if we already have a delegate awaiting this stream.
  std::map<std::string,  FlipDelegate*>::iterator it;
  it = pending_streams_.find(stream->path());
  if (it != pending_streams_.end()) {
    FlipDelegate* delegate = it->second;
    pending_streams_.erase(it);
    if (delegate)
      stream->AttachDelegate(delegate);
    else
      pushed_streams_.push_back(stream);
  } else {
    pushed_streams_.push_back(stream);
  }

  LOG(INFO) << "Got pushed stream for " << stream->path();

  static StatsCounter push_requests("flip.pushed_streams");
  push_requests.Increment();
}

void FlipSession::OnSynReply(const flip::FlipSynReplyControlFrame* frame,
                             const flip::FlipHeaderBlock* headers) {
  DCHECK(headers);
  flip::FlipStreamId stream_id = frame->stream_id();
  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    LOG(WARNING) << "Received SYN_REPLY for invalid stream " << stream_id;
    return;
  }

  // We record content declared as being pushed so that we don't
  // request a duplicate stream which is already scheduled to be
  // sent to us.
  flip::FlipHeaderBlock::const_iterator it;
  it = headers->find("X-Associated-Content");
  if (it != headers->end()) {
    const std::string& content = it->second;
    std::string::size_type start = 0;
    std::string::size_type end = 0;
    do {
      end = content.find("||", start);
      if (end == std::string::npos)
        end = content.length();
      std::string url = content.substr(start, end - start);
      std::string::size_type pos = url.find("??");
      if (pos == std::string::npos)
        break;
      url = url.substr(pos + 2);
      GURL gurl(url);
      pending_streams_[gurl.PathForRequest()] = NULL;
      start = end + 2;
    } while (end < content.length());
  }

  FlipStream* stream = active_streams_[stream_id];
  stream->OnReply(headers);
}

void FlipSession::OnControl(const flip::FlipControlFrame* frame) {
  flip::FlipHeaderBlock headers;
  uint32 type = frame->type();
  if (type == flip::SYN_STREAM || type == flip::SYN_REPLY) {
    if (!flip_framer_.ParseHeaderBlock(
        reinterpret_cast<const flip::FlipFrame*>(frame), &headers)) {
      LOG(WARNING) << "Could not parse Flip Control Frame Header";
      return;
    }
  }

  switch (type) {
    case flip::SYN_STREAM:
      LOG(INFO) << "Flip SynStream for stream " << frame->stream_id();
      OnSyn(reinterpret_cast<const flip::FlipSynStreamControlFrame*>(frame),
            &headers);
      break;
    case flip::SYN_REPLY:
      LOG(INFO) << "Flip SynReply for stream " << frame->stream_id();
      OnSynReply(
          reinterpret_cast<const flip::FlipSynReplyControlFrame*>(frame),
          &headers);
      break;
    case flip::FIN_STREAM:
      LOG(INFO) << "Flip Fin for stream " << frame->stream_id();
      OnFin(reinterpret_cast<const flip::FlipFinStreamControlFrame*>(frame));
      break;
    default:
      DCHECK(false);  // Error!
  }
}

void FlipSession::OnFin(const flip::FlipFinStreamControlFrame* frame) {
  flip::FlipStreamId stream_id = frame->stream_id();
  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    LOG(WARNING) << "Received FIN for invalid stream" << stream_id;
    return;
  }
  FlipStream* stream = active_streams_[stream_id];
  bool cleanup_stream = false;
  if (frame->status() == 0) {
    cleanup_stream = stream->OnData(NULL, 0);
  } else {
    LOG(ERROR) << "Flip stream closed: " << frame->status();
    // TODO(mbelshe): Map from Flip-protocol errors to something sensical.
    //                For now, it doesn't matter much - it is a protocol error.
    stream->OnError(ERR_FAILED);
    cleanup_stream = true;
  }

  if (cleanup_stream) {
    DeactivateStream(stream_id);
    delete stream;
  }
}

void FlipSession::OnLameDuck() {
  NOTIMPLEMENTED();
}

}  // namespace net
