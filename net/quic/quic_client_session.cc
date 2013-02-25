// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_stream_factory.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

QuicClientSession::QuicClientSession(QuicConnection* connection,
                                     QuicConnectionHelper* helper,
                                     QuicStreamFactory* stream_factory,
                                     const string& server_hostname,
                                     NetLog* net_log)
    : QuicSession(connection, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(crypto_stream_(this, server_hostname)),
      helper_(helper),
      stream_factory_(stream_factory),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      read_pending_(false),
      num_total_streams_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_QUIC_SESSION)),
      logger_(net_log_) {
  connection->set_debug_visitor(&logger_);
  // TODO(rch): pass in full host port proxy pair
  net_log_.BeginEvent(
      NetLog::TYPE_QUIC_SESSION,
      NetLog::StringCallback("host", &server_hostname));
}

QuicClientSession::~QuicClientSession() {
  connection()->set_debug_visitor(NULL);
  net_log_.EndEvent(NetLog::TYPE_QUIC_SESSION);
}

QuicReliableClientStream* QuicClientSession::CreateOutgoingReliableStream() {
  if (!crypto_stream_.handshake_complete()) {
    DLOG(INFO) << "Crypto handshake not complete, no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DLOG(INFO) << "Failed to create a new outgoing stream. "
               << "Already received goaway.";
    return NULL;
  }
  QuicReliableClientStream* stream =
      new QuicReliableClientStream(GetNextStreamId(), this, net_log_);
  ActivateStream(stream);
  ++num_total_streams_;
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return &crypto_stream_;
};

int QuicClientSession::CryptoConnect(const CompletionCallback& callback) {
  if (!crypto_stream_.CryptoConnect()) {
    // TODO(wtc): change crypto_stream_.CryptoConnect() to return a
    // QuicErrorCode and map it to a net error code.
    return ERR_CONNECTION_FAILED;
  }

  if (IsCryptoHandshakeComplete()) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

ReliableQuicStream* QuicClientSession::CreateIncomingReliableStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

void QuicClientSession::CloseStream(QuicStreamId stream_id) {
  QuicSession::CloseStream(stream_id);

  if (GetNumOpenStreams() == 0) {
    stream_factory_->OnIdleSession(this);
  }
}

void QuicClientSession::OnCryptoHandshakeComplete(QuicErrorCode error) {
  if (!callback_.is_null()) {
    callback_.Run(error == QUIC_NO_ERROR ? OK : ERR_UNEXPECTED);
  }
}

void QuicClientSession::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;
  int rv = helper_->Read(read_buffer_, read_buffer_->size(),
                         base::Bind(&QuicClientSession::OnReadComplete,
                                    weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    return;
  }

  // Data was read, process it.
  // Schedule the work through the message loop to avoid recursive
  // callbacks.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::OnReadComplete,
                 weak_factory_.GetWeakPtr(), rv));
}

void QuicClientSession::CloseSessionOnError(int error) {
  while (!streams()->empty()) {
    ReliableQuicStream* stream = streams()->begin()->second;
    QuicStreamId id = stream->id();
    static_cast<QuicReliableClientStream*>(stream)->OnError(error);
    CloseStream(id);
  }
  net_log_.BeginEvent(
      NetLog::TYPE_QUIC_SESSION,
      NetLog::IntegerCallback("net_error", error));
  // Will delete |this|.
  stream_factory_->OnSessionClose(this);
}

Value* QuicClientSession::GetInfoAsValue(const HostPortPair& pair) const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("host_port_pair", pair.ToString());
  dict->SetInteger("open_streams", GetNumOpenStreams());
  dict->SetInteger("total_streams", num_total_streams_);
  dict->SetString("peer_address", peer_address().ToString());
  dict->SetString("guid", base::Uint64ToString(guid()));
  return dict;
}

void QuicClientSession::OnReadComplete(int result) {
  read_pending_ = false;
  // TODO(rch): Inform the connection about the result.
  if (result > 0) {
    scoped_refptr<IOBufferWithSize> buffer(read_buffer_);
    read_buffer_ = new IOBufferWithSize(kMaxPacketSize);
    QuicEncryptedPacket packet(buffer->data(), result);
    IPEndPoint local_address;
    IPEndPoint peer_address;
    helper_->GetLocalAddress(&local_address);
    helper_->GetPeerAddress(&peer_address);
    // ProcessUdpPacket might result in |this| being deleted, so we
    // use a weak pointer to be safe.
    connection()->ProcessUdpPacket(local_address, peer_address, packet);
    if (!connection()->connected()) {
      stream_factory_->OnSessionClose(this);
      return;
    }
    StartReading();
  }
}

}  // namespace net
