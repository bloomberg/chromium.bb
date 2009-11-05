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
#include "net/flip/flip_io_buffer.h"
#include "net/flip/flip_protocol.h"
#include "net/flip/flip_session_pool.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_handle.h"
#include "testing/platform_test.h"

namespace net {

class FlipStream;
class HttpNetworkSession;
class HttpRequestInfo;
class HttpResponseInfo;

// The FlipDelegate interface is an interface so that the FlipSession
// can interact with the provider of a given Flip stream.
class FlipDelegate {
 public:
  virtual ~FlipDelegate() {}

  // Accessors from the delegate.

  // The delegate provides access to the HttpRequestInfo for use by the flip
  // session.
  virtual const HttpRequestInfo* request() = 0;

  // The delegate provides access to an UploadDataStream for use by the
  // flip session.  If the delegate is not uploading content, this call
  // must return NULL.
  virtual const UploadDataStream* data() = 0;

  // Callbacks.

  // Called by the FlipSession when UploadData has been sent.  If the
  // request has no upload data, this call will never be called.  This
  // callback may be called multiple times if large amounts of data are
  // being uploaded.  This callback will only be called prior to the
  // OnRequestSent callback.
  // |result| contains the number of bytes written or an error code.
  virtual void OnUploadDataSent(int result) = 0;

  // Called by the FlipSession when the Request has been entirely sent.
  // If the request contains upload data, all upload data has been sent.
  // |result| contains an error code if a failure has occurred or OK
  // on success.
  virtual void OnRequestSent(int result) = 0;

  // Called by the FlipSession when a response (e.g. a SYN_REPLY) has been
  // received for this request.  This callback will never be called prior
  // to the OnRequestSent() callback.
  virtual void OnResponseReceived(HttpResponseInfo* response) = 0;

  // Called by the FlipSession when response data has been received for this
  // request.  This callback may be called multiple times as data arrives
  // from the network, and will never be called prior to OnResponseReceived.
  // |buffer| contains the data received.  The delegate must copy any data
  //          from this buffer before returning from this callback.
  // |bytes| is the number of bytes received or an error.
  //         A zero-length count does not indicate end-of-stream.
  virtual void OnDataReceived(const char* buffer, int bytes) = 0;

  // Called by the FlipSession when the request is finished.  This callback
  // will always be called at the end of the request and signals to the
  // delegate that the delegate can be torn down.  No further callbacks to the
  // delegate will be made after this call.
  // |status| is an error code or OK.
  virtual void OnClose(int status) = 0;
};

class FlipSession : public base::RefCounted<FlipSession>,
                    public flip::FlipFramerVisitorInterface {
 public:
  // Get the domain for this FlipSession.
  const std::string& domain() const { return domain_; }

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
  bool IsStreamActive(int id) const;

  // The LoadState is used for informing the user of the current network
  // status, such as "resolving host", "connecting", etc.
  LoadState GetLoadState() const;

 protected:
  friend class FlipNetworkTransactionTest;
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
  friend class base::RefCounted<FlipSession>;
  virtual ~FlipSession();

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
  FlipStream* ActivateStream(flip::FlipStreamId id, FlipDelegate* delegate);
  void DeactivateStream(flip::FlipStreamId id);

  // Check if we have a pending pushed-stream for this url
  // Returns the stream if found (and returns it from the pending
  // list), returns NULL otherwise.
  FlipStream* GetPushStream(std::string url);

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

  // TODO(mbelshe): We need to track these stream lists better.
  //                I suspect it is possible to remove a stream from
  //                one list, but not the other.
  typedef std::map<int, FlipStream*> ActiveStreamMap;
  typedef std::list<FlipStream*> ActiveStreamList;
  ActiveStreamMap active_streams_;

  ActiveStreamList pushed_streams_;
  // List of streams declared in X-Associated-Content headers.
  // The key is a string representing the path of the URI being pushed.
  std::map<std::string, FlipDelegate*> pending_streams_;

  // As we gather data to be sent, we put it into the output queue.
  typedef std::priority_queue<FlipIOBuffer> OutputQueue;
  OutputQueue queue_;

  // TODO(mbelshe): this is ugly!!
  // The packet we are currently sending.
  FlipIOBuffer in_flight_write_;
  bool delayed_write_pending_;
  bool write_pending_;

  // Flip Frame state.
  flip::FlipFramer flip_framer_;

  static bool use_ssl_;
};

}  // namespace net

#endif  // NET_FLIP_FLIP_SESSION_H_
