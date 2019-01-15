// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_connection_impl.h"

#include "api/impl/quic/quic_connection_factory_impl.h"
#include "base/make_unique.h"
#include "third_party/abseil/src/absl/types/optional.h"
#include "third_party/chromium_quic/src/net/third_party/quic/platform/impl/quic_chromium_clock.h"

namespace openscreen {

UdpTransport::UdpTransport(platform::UdpSocketPtr socket,
                           const IPEndpoint& destination)
    : socket_(socket), destination_(destination) {}
UdpTransport::UdpTransport(UdpTransport&&) = default;
UdpTransport::~UdpTransport() = default;

UdpTransport& UdpTransport::operator=(UdpTransport&&) = default;

int UdpTransport::Write(const char* buffer,
                        size_t buffer_length,
                        const PacketInfo& info) {
  return platform::SendUdp(socket_, buffer, buffer_length, destination_)
      .value_or(-1);
}

QuicStreamImpl::QuicStreamImpl(QuicStream::Delegate* delegate,
                               ::quic::QuartcStream* stream)
    : QuicStream(delegate, stream->id()), stream_(stream) {
  stream_->SetDelegate(this);
}

QuicStreamImpl::~QuicStreamImpl() = default;

void QuicStreamImpl::Write(const uint8_t* data, size_t data_size) {
  stream_->WriteOrBufferData(
      ::quic::QuicStringPiece(reinterpret_cast<const char*>(data), data_size),
      false, nullptr);
}

void QuicStreamImpl::CloseWriteEnd() {
  stream_->FinishWriting();
}

void QuicStreamImpl::OnReceived(::quic::QuartcStream* stream,
                                const char* data,
                                size_t data_size) {
  delegate_->OnReceived(this, data, data_size);
}

void QuicStreamImpl::OnClose(::quic::QuartcStream* stream) {
  delegate_->OnClose(stream->id());
}

void QuicStreamImpl::OnBufferChanged(::quic::QuartcStream* stream) {}

QuicConnectionImpl::QuicConnectionImpl(
    QuicConnectionFactoryImpl* parent_factory,
    QuicConnection::Delegate* delegate,
    std::unique_ptr<UdpTransport>&& udp_transport,
    std::unique_ptr<::quic::QuartcSession>&& session)
    : QuicConnection(delegate),
      parent_factory_(parent_factory),
      session_(std::move(session)),
      udp_transport_(std::move(udp_transport)) {
  session_->SetDelegate(this);
  session_->OnTransportCanWrite();
  session_->StartCryptoHandshake();
}

QuicConnectionImpl::~QuicConnectionImpl() = default;

void QuicConnectionImpl::OnDataReceived(const platform::ReceivedData& data) {
  session_->OnTransportReceived(
      reinterpret_cast<const char*>(data.bytes.data()), data.length);
}

std::unique_ptr<QuicStream> QuicConnectionImpl::MakeOutgoingStream(
    QuicStream::Delegate* delegate) {
  ::quic::QuartcStream* stream = session_->CreateOutgoingDynamicStream();
  return MakeUnique<QuicStreamImpl>(delegate, stream);
}

void QuicConnectionImpl::Close() {
  session_->CloseConnection("closed");
}

void QuicConnectionImpl::OnCryptoHandshakeComplete() {
  delegate_->OnCryptoHandshakeComplete(session_->connection_id());
}

void QuicConnectionImpl::OnIncomingStream(::quic::QuartcStream* stream) {
  auto public_stream = MakeUnique<QuicStreamImpl>(
      delegate_->NextStreamDelegate(session_->connection_id(), stream->id()),
      stream);
  streams_.push_back(public_stream.get());
  delegate_->OnIncomingStream(session_->connection_id(),
                              std::move(public_stream));
}

void QuicConnectionImpl::OnConnectionClosed(
    ::quic::QuicErrorCode error_code,
    const ::quic::QuicString& error_details,
    ::quic::ConnectionCloseSource source) {
  parent_factory_->OnConnectionClosed(this);
  delegate_->OnConnectionClosed(session_->connection_id());
}

}  // namespace openscreen
