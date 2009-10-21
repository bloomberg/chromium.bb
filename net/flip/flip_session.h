// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_FLIP_SESSION_H_
#define NET_FLIP_FLIP_SESSION_H_

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <string>

#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/upload_data_stream.h"
#include "net/flip/flip_framer.h"
#include "net/flip/flip_protocol.h"
#include "net/flip/flip_session_pool.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_handle.h"
#include "testing/platform_test.h"

namespace net {

class FlipStreamImpl;
class HttpNetworkSession;
class HttpRequestInfo;
class HttpResponseInfo;

// A callback interface for HTTP content retrieved from the Flip stream.
class FlipDelegate {
 public:
  virtual ~FlipDelegate() {}
  virtual const HttpRequestInfo* request() = 0;
  virtual const UploadDataStream* data() = 0;

  virtual void OnRequestSent(int status) = 0;
  virtual void OnResponseReceived(HttpResponseInfo* response) = 0;
  virtual void OnDataReceived(const char* buffer, int bytes) = 0;
  virtual void OnClose(int status) = 0;
  virtual void OnCancel() = 0;
};

class PrioritizedIOBuffer {
 public:
  PrioritizedIOBuffer() : buffer_(0), priority_(0) {}
  PrioritizedIOBuffer(IOBufferWithSize* buffer, int priority)
      : buffer_(buffer),
        priority_(priority),
        position_(++order_) {
  }

  IOBuffer* buffer() const { return buffer_; }

  size_t size() const { return buffer_->size(); }

  void release() { buffer_ = NULL; }

  int priority() { return priority_; }

  // Supports sorting.
  bool operator<(const PrioritizedIOBuffer& other) const {
    if (priority_ != other.priority_)
      return priority_ > other.priority_;
    return position_ >= other.position_;
  }

 private:
  scoped_refptr<IOBufferWithSize> buffer_;
  int priority_;
  int position_;
  static int order_;  // Maintains a FIFO order for equal priorities.
};

class FlipSession : public base::RefCounted<FlipSession>,
                    public flip::FlipFramerVisitorInterface {
 public:
  // Factory for finding open sessions.
  // TODO(mbelshe): Break this out into a connection pool class?
  static FlipSession* GetFlipSession(const HostResolver::RequestInfo&,
                                     HttpNetworkSession* session);
  virtual ~FlipSession();

  // Get the domain for this FlipSession.
  std::string domain() { return domain_; }

  // Connect the FLIP Socket.
  // Returns net::Error::OK on success.
  // Note that this call does not wait for the connect to complete. Callers can
  // immediately start using the FlipSession while it connects.
  net::Error Connect(const std::string& group_name,
                     const HostResolver::RequestInfo& host, int priority);

  // Create a new stream.
  // FlipDelegate must remain valid until the stream is either cancelled by the
  // creator via CancelStream or the FlipDelegate OnClose or OnCancel callbacks
  // have been made.
  // Once the stream is created, the delegate should wait for a callback.
  int CreateStream(FlipDelegate* delegate);

  // Cancel a stream.
  bool CancelStream(int id);

  // Check if a stream is active.
  bool IsStreamActive(int id);

  // The LoadState is used for informing the user of the current network
  // status, such as "resolving host", "connecting", etc.
  LoadState GetLoadState() const;
 protected:
  FRIEND_TEST(FlipNetworkTransactionTest, Connect);
  friend class FlipSessionPool;
  friend class HttpNetworkLayer;  // Temporary for server.

  // Provide access to the framer for testing.
  flip::FlipFramer* GetFramer() { return &flip_framer_; }

  // Create a new FlipSession.
  // |host| is the hostname that this session connects to.
  FlipSession(std::string host, HttpNetworkSession* session);

  // Closes all open streams.  Used as part of shutdown.
  void CloseAllStreams(net::Error code);

  // Enable or disable SSL.  This is only to be used for testing.
  static void SetSSLMode(bool enable) { use_ssl_ = enable; }

 private:
  // FlipFramerVisitorInterface
  virtual void OnError(flip::FlipFramer*);
  virtual void OnStreamFrameData(flip::FlipStreamId stream_id,
                                 const char* data,
                                 size_t len);
  virtual void OnControl(const flip::FlipControlFrame* frame);
  virtual void OnLameDuck();

  // Control frame handlers.
  void OnSyn(const flip::FlipSynStreamControlFrame* frame,
             const flip::FlipHeaderBlock* headers);
  void OnSynReply(const flip::FlipSynReplyControlFrame* frame,
                  const flip::FlipHeaderBlock* headers);
  void OnFin(const flip::FlipFinStreamControlFrame* frame);

  // IO Callbacks
  void OnTCPConnect(int result);
  void OnSSLConnect(int result);
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  // Start reading from the socket.
  void ReadSocket();

  // Write current data to the socket.
  void WriteSocketLater();
  void WriteSocket();

  // Get a new stream id.
  int GetNewStreamId();

  // Track active streams in the active stream list.
  FlipStreamImpl* ActivateStream(flip::FlipStreamId id, FlipDelegate* delegate);
  void DeactivateStream(flip::FlipStreamId id);

  // Check if we have a pending pushed-stream for this url
  // Returns the stream if found (and returns it from the pending
  // list), returns NULL otherwise.
  FlipStreamImpl* GetPushStream(std::string url);

  // Callbacks for the Flip session.
  CompletionCallbackImpl<FlipSession> connect_callback_;
  CompletionCallbackImpl<FlipSession> ssl_connect_callback_;
  CompletionCallbackImpl<FlipSession> read_callback_;
  CompletionCallbackImpl<FlipSession> write_callback_;

  // The domain this session is connected to.
  std::string domain_;

  SSLConfig ssl_config_;

  scoped_refptr<HttpNetworkSession> session_;

  // The socket handle for this session.
  ClientSocketHandle connection_;
  bool connection_started_;  // Is the connect process started.
  bool connection_ready_;  // Is the connection ready for use.

  // The read buffer used to read data from the socket.
  enum { kReadBufferSize = (4 * 1024) };
  scoped_refptr<IOBuffer> read_buffer_;
  bool read_pending_;

  int stream_hi_water_mark_;  // The next stream id to use.

  typedef std::map<int, FlipStreamImpl*> ActiveStreamMap;
  typedef std::list<FlipStreamImpl*> ActiveStreamList;
  ActiveStreamMap active_streams_;

  ActiveStreamList pushed_streams_;
  // List of streams declared in X-Associated-Content headers.
  // The key is a string representing the path of the URI being pushed.
  std::map<std::string, FlipDelegate*> pending_streams_;

  // As we gather data to be sent, we put it into the output queue.
  typedef std::priority_queue<PrioritizedIOBuffer> OutputQueue;
  OutputQueue queue_;

  // TODO(mbelshe): this is ugly!!
  // The packet we are currently sending.
  PrioritizedIOBuffer in_flight_write_;
  bool delayed_write_pending_;
  bool write_pending_;

  // Flip Frame state.
  flip::FlipFramer flip_framer_;

  // This is our weak session pool - one session per domain.
  static scoped_ptr<FlipSessionPool> session_pool_;
  static bool use_ssl_;
};

}  // namespace net

#endif  // NET_FLIP_FLIP_SESSION_H_

