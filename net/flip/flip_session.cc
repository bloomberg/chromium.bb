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
#include "net/base/bandwidth_metrics.h"
#include "net/base/load_flags.h"
#include "net/flip/flip_frame_builder.h"
#include "net/flip/flip_protocol.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/tools/dump_cache/url_to_filename_encoder.h"

namespace net {

// static
scoped_ptr<FlipSessionPool> FlipSession::session_pool_;
bool FlipSession::use_ssl_ = true;
int PrioritizedIOBuffer::order_ = 0;

// The FlipStreamImpl is an internal class representing an active FlipStream.
class FlipStreamImpl {
 public:
  // A FlipStreamImpl represents an active FlipStream.
  // If |delegate| is NULL, then we're receiving pushed data from the server.
 FlipStreamImpl(flip::FlipStreamId stream_id, FlipDelegate* delegate)
      : stream_id_(stream_id),
        delegate_(delegate),
        response_(NULL),
        download_finished_(false),
        metrics_(Singleton<BandwidthMetrics>::get()) {}
  virtual ~FlipStreamImpl() {
    LOG(INFO) << "Deleting FlipStreamImpl for stream " << stream_id_;
  };

  flip::FlipStreamId stream_id() const { return stream_id_; }
  FlipDelegate* delegate() const { return delegate_; }

  // For pushed streams, we track a path to identify them.
  std::string path() const { return path_; }
  void set_path(const std::string& path) { path_ = path; }

  // Attach a delegate to a previously pushed data stream.
  // Returns true if the caller should Deactivate and delete the FlipStreamImpl
  // after this call.
  bool AttachDelegate(FlipDelegate* delegate);

  // Called when a SYN_REPLY is received on the stream.
  void OnReply(const flip::FlipHeaderBlock* headers);

  // Called when data is received on the stream.
  // Returns true if the caller should Deactivate and delete the FlipStreamImpl
  // after this call.
  bool OnData(const char* data, int length);

  // Called if the stream is being prematurely closed for an abort or an error.
  // |err| is actually a net::Error.
  // The caller should Deactivate and delete the FlipStreamImpl after this call.
  void OnError(int err);

 private:
  flip::FlipStreamId stream_id_;
  std::string path_;
  FlipDelegate* delegate_;
  scoped_ptr<HttpResponseInfo> response_;
  std::list<scoped_refptr<IOBufferWithSize> > response_body_;
  bool download_finished_;
  ScopedBandwidthMetrics metrics_;
};

FlipSession* FlipSession::GetFlipSession(
    const HostResolver::RequestInfo& info,
    HttpNetworkSession* session) {
  if (!session_pool_.get())
    session_pool_.reset(new FlipSessionPool());
  return session_pool_->Get(info, session);
}

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
  if (session_pool_.get())
    session_pool_->Remove(this);
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

#undef DIVERT_URLS_TO_TEST_SERVER
#ifdef DIVERT_URLS_TO_TEST_SERVER
  // TODO(mbelshe): Figure out how to remove this.  This is just for hooking
  // up to a test server.
  // For testing content on our test server, we modify the URL.
  GURL url = info->url;
  FilePath path(UrlToFilenameEncoder::Encode(url.spec(), FilePath()));

  // We do the unusual conversion from a FilePath::StringType to
  // an ascii string.  Recognize that StringType is a wstring on windows,
  // so a failure is technically possible, but this is just used as a test
  // case, so it's okay.  This code will be deleted.
  std::string hack_url("/");
#if defined(OS_WIN)
  hack_url.append(WideToASCII(path.value()));
#else
  hack_url.append(path.value());
#endif

  // switch backslashes.  HACK
  std::string::size_type pos(0);
  while ((pos = hack_url.find('\\', pos)) != std::string::npos) {
    hack_url.replace(pos, 1, "/");
    pos += 1;
  }
#else
  std::string hack_url(info->url.PathForRequest());
#endif  // REWRITE_URLS

  (*headers)["method"] = info->method;
  (*headers)["url"] = hack_url;
  (*headers)["version"] = kHttpProtocolVersion;
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

  FlipStreamImpl* stream = NULL;

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
  queue_.push(PrioritizedIOBuffer(buffer, priority));

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
  FlipStreamImpl* stream = active_streams_[id];
  DeactivateStream(id);
  delete stream;
  return true;
}

bool FlipSession::IsStreamActive(int id) {
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
    int status = connection_.socket()->Connect(&ssl_connect_callback_);
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

  while (queue_.size()) {
    const size_t kMaxSegmentSize = 1430;
    const size_t kMaxPayload = 4 * kMaxSegmentSize;
    size_t max_size = std::max(kMaxPayload, queue_.top().size());

    size_t bytes = 0;
    // If we have multiple IOs to do, accumulate up to 4 MSS's worth of data
    // and send them in batch.
    IOBufferWithSize* buffer = new IOBufferWithSize(max_size);
    while (queue_.size() && bytes < max_size) {
      PrioritizedIOBuffer next_buffer = queue_.top();

      // Now that we're outputting the frame, we can finally compress it.
      flip::FlipFrame* uncompressed_frame =
          reinterpret_cast<flip::FlipFrame*>(next_buffer.buffer()->data());
      scoped_array<flip::FlipFrame> compressed_frame(
          flip_framer_.CompressFrame(uncompressed_frame));
      size_t size = compressed_frame.get()->length() + sizeof(flip::FlipFrame);
      if (bytes + size > kMaxPayload)
        break;
      memcpy(buffer->data() + bytes, compressed_frame.get(), size);
      bytes += size;
      queue_.pop();
      next_buffer.release();
    }
    DCHECK(bytes > 0);
    in_flight_write_ = PrioritizedIOBuffer(buffer, 0);
    write_pending_ = true;

    int rv = connection_.socket()->Write(in_flight_write_.buffer(),
                                         bytes, &write_callback_);
    if (rv == net::ERR_IO_PENDING)
      break;
    OnWriteComplete(rv);
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
    FlipStreamImpl** list = new FlipStreamImpl*[active_streams_.size()];
    ActiveStreamMap::const_iterator it;
    int index = 0;
    for (it = active_streams_.begin(); it != active_streams_.end(); ++it)
      list[index++] = it->second;

    // Issue the aborts.
    for (--index; index >= 0; index--) {
      LOG(ERROR) << "ABANDONED: " << list[index]->path();
      list[index]->OnError(ERR_ABORTED);
      delete list[index];
    }

    // Clear out anything pending.
    active_streams_.clear();

    delete list;
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

FlipStreamImpl* FlipSession::ActivateStream(flip::FlipStreamId id,
                                            FlipDelegate* delegate) {
  DCHECK(!IsStreamActive(id));

  FlipStreamImpl* stream = new FlipStreamImpl(id, delegate);
  active_streams_[id] = stream;
  return stream;
}

void FlipSession::DeactivateStream(flip::FlipStreamId id) {
  DCHECK(IsStreamActive(id));

  // Verify it is not on the pushed_streams_ list.
  ActiveStreamList::iterator it;
  for (it = pushed_streams_.begin(); it != pushed_streams_.end(); ++it) {
    FlipStreamImpl* impl = *it;
    if (id == impl->stream_id()) {
      pushed_streams_.erase(it);
      break;
    }
  }

  active_streams_.erase(id);
}

FlipStreamImpl* FlipSession::GetPushStream(std::string path) {
  static StatsCounter used_push_streams("flip.claimed_push_streams");

  LOG(INFO) << "Looking for push stream: " << path;

  // We just walk a linear list here.
  ActiveStreamList::iterator it;
  for (it = pushed_streams_.begin(); it != pushed_streams_.end(); ++it) {
    FlipStreamImpl* impl = *it;
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

  FlipStreamImpl* stream = active_streams_[stream_id];
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
  FlipStreamImpl* stream = ActivateStream(stream_id, NULL);
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

  FlipStreamImpl* stream = active_streams_[stream_id];
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
  FlipStreamImpl* stream = active_streams_[stream_id];
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

bool FlipStreamImpl::AttachDelegate(FlipDelegate* delegate) {
  DCHECK(delegate_ == NULL);  // Don't attach if already attached.
  DCHECK(path_.length() > 0);  // Path needs to be set for push streams.
  delegate_ = delegate;

  // If there is pending data, send it up here.

  // Check for the OnReply, and pass it up.
  if (response_.get())
    delegate_->OnResponseReceived(response_.get());

  // Pass data up
  while (response_body_.size()) {
    scoped_refptr<IOBufferWithSize> buffer = response_body_.front();
    response_body_.pop_front();
    delegate_->OnDataReceived(buffer->data(), buffer->size());
  }

  // Finally send up the end-of-stream.
  if (download_finished_) {
    delegate_->OnClose(net::OK);
    return true;  // tell the caller to shut us down
  }
  return false;
}

void FlipStreamImpl::OnReply(const flip::FlipHeaderBlock* headers) {
  DCHECK(headers);
  DCHECK(headers->find("version") != headers->end());
  DCHECK(headers->find("status") != headers->end());

  // TODO(mbelshe): if no version or status is found, we need to error
  // out the stream.

  metrics_.StartStream();

  // Server initiated streams must send a URL to us in the headers.
  if (headers->find("path") != headers->end())
    path_ = headers->find("path")->second;

  // TODO(mbelshe): For now we convert from our nice hash map back
  // to a string of headers; this is because the HttpResponseInfo
  // is a bit rigid for its http (non-flip) design.
  std::string raw_headers(headers->find("version")->second);
  raw_headers.append(" ", 1);
  raw_headers.append(headers->find("status")->second);
  raw_headers.append("\0", 1);
  flip::FlipHeaderBlock::const_iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    raw_headers.append(it->first);
    raw_headers.append(":", 1);
    raw_headers.append(it->second);
    raw_headers.append("\0", 1);
  }

  LOG(INFO) << "FlipStream: SynReply received for " << stream_id_;

  DCHECK(response_ == NULL);
  response_.reset(new HttpResponseInfo());
  response_->headers = new HttpResponseHeaders(raw_headers);
  // When pushing content from the server, we may not yet have a delegate_
  // to notify.  When the delegate is attached, it will notify then.
  if (delegate_)
    delegate_->OnResponseReceived(response_.get());
}

bool FlipStreamImpl::OnData(const char* data, int length) {
  DCHECK(length >= 0);
  LOG(INFO) << "FlipStream: Data (" << length << " bytes) received for "
            << stream_id_;

  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (!response_.get()) {
    if (delegate_)
      delegate_->OnClose(ERR_SYN_REPLY_NOT_RECEIVED);
    return true;
  }

  // A zero-length read means that the stream is being closed.
  if (!length) {
    metrics_.StopStream();
    download_finished_ = true;
    if (delegate_) {
      delegate_->OnClose(net::OK);
      return true;  // Tell the caller to clean us up.
    }
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);

  // We've received data.  We try to pass it up to the caller.
  // In the case of server-push streams, we may not have a delegate yet assigned
  // to this stream.  In that case we just queue the data for later.
  if (delegate_) {
    delegate_->OnDataReceived(data, length);
  } else {
    // Save the data for use later.
    // TODO(mbelshe): We need to have some throttling on this.  We shouldn't
    //                buffer an infinite amount of data.
    IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
    memcpy(io_buffer->data(), data, length);
    response_body_.push_back(io_buffer);
  }
  return false;
}

void FlipStreamImpl::OnError(int err) {
  if (delegate_)
    delegate_->OnClose(err);
}

}  // namespace net

