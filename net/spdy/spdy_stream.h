// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_STREAM_H_
#define NET_SPDY_SPDY_STREAM_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/bandwidth_metrics.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/ssl/ssl_client_cert_type.h"

namespace net {

class AddressList;
class IPEndPoint;
class SSLCertRequestInfo;
class SSLInfo;

// The SpdyStream is used by the SpdySession to represent each stream known
// on the SpdySession.  This class provides interfaces for SpdySession to use.
// Streams can be created either by the client or by the server.  When they
// are initiated by the client, both the SpdySession and client object (such as
// a SpdyNetworkTransaction) will maintain a reference to the stream.  When
// initiated by the server, only the SpdySession will maintain any reference,
// until such a time as a client object requests a stream for the path.
class NET_EXPORT_PRIVATE SpdyStream
    : public base::RefCounted<SpdyStream> {
 public:
  // Delegate handles protocol specific behavior of spdy stream.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() {}

    // Called when SYN frame has been sent.
    // Returns true if no more data to be sent after SYN frame.
    virtual bool OnSendHeadersComplete(int status) = 0;

    // Called when stream is ready to send data.
    // Returns network error code. OK when it successfully sent data.
    // ERR_IO_PENDING when performing operation asynchronously.
    virtual int OnSendBody() = 0;

    // Called when data has been sent. |status| indicates network error
    // or number of bytes that has been sent. On return, |eof| is set to true
    // if no more data is available to send in the request body.
    // Returns network error code. OK when it successfully sent data.
    virtual int OnSendBodyComplete(int status, bool* eof) = 0;

    // Called when the SYN_STREAM, SYN_REPLY, or HEADERS frames are received.
    // Normal streams will receive a SYN_REPLY and optional HEADERS frames.
    // Pushed streams will receive a SYN_STREAM and optional HEADERS frames.
    // Because a stream may have a SYN_* frame and multiple HEADERS frames,
    // this callback may be called multiple times.
    // |status| indicates network error. Returns network error code.
    virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                   base::Time response_time,
                                   int status) = 0;

    // Called when a HEADERS frame is sent.
    virtual void OnHeadersSent() = 0;

    // Called when data is received.
    // Returns network error code. OK when it successfully receives data.
    virtual int OnDataReceived(const char* data, int length) = 0;

    // Called when data is sent.
    virtual void OnDataSent(int length) = 0;

    // Called when SpdyStream is closed. No other delegate functions
    // will be called after this is called, and the delegate must not
    // access the stream after this is called.
    virtual void OnClose(int status) = 0;

   protected:
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Indicates pending frame type.
  enum FrameType {
    TYPE_HEADERS,
    TYPE_DATA
  };

  // Structure to contains pending frame information.
  typedef struct {
    FrameType type;
    union {
      SpdyHeaderBlock* header_block;
      SpdyFrame* data_frame;
    };
  } PendingFrame;

  // SpdyStream constructor
  SpdyStream(SpdySession* session,
             bool pushed,
             const BoundNetLog& net_log);

  // Set new |delegate|. |delegate| must not be NULL.
  // If it already received SYN_REPLY or data, OnResponseReceived() or
  // OnDataReceived() will be called.
  void SetDelegate(Delegate* delegate);
  Delegate* GetDelegate() { return delegate_; }

  // Detach delegate from the stream. It will cancel the stream if it was not
  // cancelled yet.  It is safe to call multiple times.
  void DetachDelegate();

  // Is this stream a pushed stream from the server.
  bool pushed() const { return pushed_; }

  SpdyStreamId stream_id() const { return stream_id_; }
  void set_stream_id(SpdyStreamId stream_id) { stream_id_ = stream_id; }

  bool response_received() const { return response_received_; }
  void set_response_received() { response_received_ = true; }

  // For pushed streams, we track a path to identify them.
  const std::string& path() const { return path_; }
  void set_path(const std::string& path) { path_ = path; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  int32 send_window_size() const { return send_window_size_; }
  void set_send_window_size(int32 window_size) {
    send_window_size_ = window_size;
  }

  int32 recv_window_size() const { return recv_window_size_; }
  void set_recv_window_size(int32 window_size) {
    recv_window_size_ = window_size;
  }

  bool send_stalled_by_flow_control() { return send_stalled_by_flow_control_; }

  void set_send_stalled_by_flow_control(bool stalled) {
    send_stalled_by_flow_control_ = stalled;
  }

  // If stream flow control is turned on, called by the session to
  // adjust this stream's send window size by |delta_window_size|,
  // which is the difference between the SETTINGS_INITIAL_WINDOW_SIZE
  // in the most recent SETTINGS frame and the previous initial send
  // window size, possibly unstalling this stream. Although
  // |delta_window_size| may cause this stream's send window size to
  // go negative, it must not cause it to wrap around in either
  // direction. Does nothing if the stream is already closed.
  //
  // If stream flow control is turned off, this must not be called.
  void AdjustSendWindowSize(int32 delta_window_size);

  // If stream flow control is turned on, called by the session to
  // increase this stream's send window size by |delta_window_size|
  // from a WINDOW_UPDATE frome, which must be at least 1, possibly
  // unstalling this stream. If |delta_window_size| would cause this
  // stream's send window size to overflow, calls into the session to
  // reset this stream. Does nothing if the stream is already closed.
  //
  // If stream flow control is turned off, this must not be called.
  void IncreaseSendWindowSize(int32 delta_window_size);

  // If stream flow control is turned on, called by the session to
  // decrease this stream's send window size by |delta_window_size|,
  // which must be at least 0 and at most kMaxSpdyFrameChunkSize.
  // |delta_window_size| must not cause this stream's send window size
  // to go negative. Does nothing if the stream is already closed.
  //
  // If stream flow control is turned off, this must not be called.
  void DecreaseSendWindowSize(int32 delta_window_size);

  // Called by the delegate to increase this stream's receive window
  // size by |delta_window_size|, which must be at least 1 and must
  // not cause this stream's receive window size to overflow, possibly
  // also sending a WINDOW_UPDATE frame.
  //
  // Unlike the functions above, this may be called even when stream
  // flow control is turned off, although this does nothing in that
  // case (and also if the stream is inactive).
  void IncreaseRecvWindowSize(int32 delta_window_size);

  int GetPeerAddress(IPEndPoint* address) const;
  int GetLocalAddress(IPEndPoint* address) const;

  // Returns true if the underlying transport socket ever had any reads or
  // writes.
  bool WasEverUsed() const;

  const BoundNetLog& net_log() const { return net_log_; }

  const SpdyHeaderBlock& spdy_headers() const;
  void set_spdy_headers(scoped_ptr<SpdyHeaderBlock> headers);
  base::Time GetRequestTime() const;
  void SetRequestTime(base::Time t);

  // Called by the SpdySession when a response (e.g. a SYN_STREAM or SYN_REPLY)
  // has been received for this stream. Returns a status code.
  int OnResponseReceived(const SpdyHeaderBlock& response);

  // Called by the SpdySession when late-bound headers are received for a
  // stream. Returns a status code.
  int OnHeaders(const SpdyHeaderBlock& headers);

  // Called by the SpdySession when response data has been received for this
  // stream.  This callback may be called multiple times as data arrives
  // from the network, and will never be called prior to OnResponseReceived.
  //
  // |buffer| contains the data received, or NULL if the stream is
  //          being closed.  The stream must copy any data from this
  //          buffer before returning from this callback.
  //
  // |length| is the number of bytes received (at most 2^24 - 1) or 0 if
  //          the stream is being closed.
  void OnDataReceived(const char* buffer, size_t length);

  // Called by the SpdySession when a write has completed.  This callback
  // will be called multiple times for each write which completes.  Writes
  // include the SYN_STREAM write and also DATA frame writes.
  // |result| is the number of bytes written or a net error code.
  void OnWriteComplete(int bytes);

  // Called by the SpdySession when the request is finished.  This callback
  // will always be called at the end of the request and signals to the
  // stream that the stream has no more network events.  No further callbacks
  // to the stream will be made after this call.
  // |status| is an error code or OK.
  void OnClose(int status);

  // Called by the SpdySession to log stream related errors.
  void LogStreamError(int status, const std::string& description);

  void Cancel();
  void Close();
  bool cancelled() const { return cancelled_; }
  bool closed() const { return io_state_ == STATE_DONE; }
  // TODO(satorux): This is only for testing. We should be able to remove
  // this once crbug.com/113107 is addressed.
  bool body_sent() const { return io_state_ > STATE_SEND_BODY_COMPLETE; }

  // Interface for Spdy[Http|WebSocket]Stream to use.

  // Sends the request.
  // For non push stream, it will send SYN_STREAM frame.
  int SendRequest(bool has_upload_data);

  // Queues a HEADERS frame to be sent.
  void QueueHeaders(scoped_ptr<SpdyHeaderBlock> headers);

  // Queues a DATA frame to be sent. May not queue all the data that
  // is given (or even any of it) depending on flow control.
  void QueueStreamData(IOBuffer* data, int length,
                       SpdyDataFlags flags);

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  bool GetSSLInfo(SSLInfo* ssl_info,
                  bool* was_npn_negotiated,
                  NextProto* protocol_negotiated);

  // Fills SSL Certificate Request info |cert_request_info| and returns
  // true when SSL is in use.
  bool GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);

  // If the stream is stalled on sending data, but the session is not
  // stalled on sending data and |send_window_size_| is positive, then
  // set |send_stalled_by_flow_control_| to false and unstall the data
  // sending. Called by the session or by the stream itself. Must be
  // called only when the stream is still open.
  void PossiblyResumeIfSendStalled();

  bool is_idle() const {
    return io_state_ == STATE_OPEN || io_state_ == STATE_DONE;
  }

  int response_status() const { return response_status_; }

  // Returns true if the URL for this stream is known.
  bool HasUrl() const;

  // Get the URL associated with this stream.  Only valid when has_url() is
  // true.
  GURL GetUrl() const;

  int GetProtocolVersion() const;

 private:
  class SpdyStreamIOBufferProducer;

  enum State {
    STATE_NONE,
    STATE_GET_DOMAIN_BOUND_CERT,
    STATE_GET_DOMAIN_BOUND_CERT_COMPLETE,
    STATE_SEND_DOMAIN_BOUND_CERT,
    STATE_SEND_DOMAIN_BOUND_CERT_COMPLETE,
    STATE_SEND_HEADERS,
    STATE_SEND_HEADERS_COMPLETE,
    STATE_SEND_BODY,
    STATE_SEND_BODY_COMPLETE,
    STATE_WAITING_FOR_RESPONSE,
    STATE_OPEN,
    STATE_DONE
  };

  friend class base::RefCounted<SpdyStream>;

  virtual ~SpdyStream();

  void OnGetDomainBoundCertComplete(int result);

  // Try to make progress sending/receiving the request/response.
  int DoLoop(int result);

  // The implementations of each state of the state machine.
  int DoGetDomainBoundCert();
  int DoGetDomainBoundCertComplete(int result);
  int DoSendDomainBoundCert();
  int DoSendDomainBoundCertComplete(int result);
  int DoSendHeaders();
  int DoSendHeadersComplete(int result);
  int DoSendBody();
  int DoSendBodyComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoOpen(int result);

  // Update the histograms.  Can safely be called repeatedly, but should only
  // be called after the stream has completed.
  void UpdateHistograms();

  // When a server pushed stream is first created, this function is posted on
  // the MessageLoop to replay all the data that the server has already sent.
  void PushedStreamReplayData();

  // Informs the SpdySession that this stream has a write available.
  void SetHasWriteAvailable();

  // Returns a newly created SPDY frame owned by the called that contains
  // the next frame to be sent by this frame.  May return NULL if this
  // stream has become stalled on flow control.
  scoped_ptr<SpdyFrame> ProduceNextFrame();

  // If the stream is active and stream flow control is turned on,
  // called by OnDataReceived (which is in turn called by the session)
  // to decrease this stream's receive window size by
  // |delta_window_size|, which must be at least 1 and must not cause
  // this stream's receive window size to go negative.
  void DecreaseRecvWindowSize(int32 delta_window_size);

  // There is a small period of time between when a server pushed stream is
  // first created, and the pushed data is replayed. Any data received during
  // this time should continue to be buffered.
  bool continue_buffering_data_;

  SpdyStreamId stream_id_;
  std::string path_;
  RequestPriority priority_;
  size_t slot_;

  // Flow control variables.
  bool send_stalled_by_flow_control_;
  int32 send_window_size_;
  int32 recv_window_size_;
  int32 unacked_recv_window_bytes_;

  const bool pushed_;
  ScopedBandwidthMetrics metrics_;
  bool response_received_;

  scoped_refptr<SpdySession> session_;

  // The transaction should own the delegate.
  SpdyStream::Delegate* delegate_;

  // The request to send.
  scoped_ptr<SpdyHeaderBlock> request_;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time_;

  scoped_ptr<SpdyHeaderBlock> response_;
  base::Time response_time_;

  // An in order list of pending frame data that are going to be sent. HEADERS
  // frames are queued as SpdyHeaderBlock structures because these must be
  // compressed just before sending. Data frames are queued as SpdyDataFrame.
  std::list<PendingFrame> pending_frames_;

  // An in order list of sending frame types. It will be used to know which type
  // of frame is sent and which callback should be invoked in OnOpen().
  std::list<FrameType> waiting_completions_;

  State io_state_;

  // Since we buffer the response, we also buffer the response status.
  // Not valid until the stream is closed.
  int response_status_;

  bool cancelled_;
  bool has_upload_data_;

  BoundNetLog net_log_;

  base::TimeTicks send_time_;
  base::TimeTicks recv_first_byte_time_;
  base::TimeTicks recv_last_byte_time_;
  int send_bytes_;
  int recv_bytes_;
  // Data received before delegate is attached.
  std::vector<scoped_refptr<IOBufferWithSize> > pending_buffers_;

  SSLClientCertType domain_bound_cert_type_;
  std::string domain_bound_private_key_;
  std::string domain_bound_cert_;
  ServerBoundCertService::RequestHandle domain_bound_cert_request_handle_;

  DISALLOW_COPY_AND_ASSIGN(SpdyStream);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_STREAM_H_
