// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/util.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"

namespace domain_reliability {

namespace {

const struct NetErrorMapping {
  int net_error;
  const char* beacon_status;
} net_error_map[] = {
  { net::OK, "ok" },
  { net::ERR_ABORTED, "aborted" },
  { net::ERR_TIMED_OUT, "tcp.connection.timed_out" },
  { net::ERR_CONNECTION_CLOSED, "tcp.connection.closed" },
  { net::ERR_CONNECTION_RESET, "tcp.connection.reset" },
  { net::ERR_CONNECTION_REFUSED, "tcp.connection.refused" },
  { net::ERR_CONNECTION_ABORTED, "tcp.connection.aborted" },
  { net::ERR_CONNECTION_FAILED, "tcp.connection.failed" },
  { net::ERR_NAME_NOT_RESOLVED, "dns" },
  { net::ERR_SSL_PROTOCOL_ERROR, "ssl.protocol.error" },
  { net::ERR_ADDRESS_INVALID, "tcp.connection.address_invalid" },
  { net::ERR_ADDRESS_UNREACHABLE, "tcp.connection.address_unreachable" },
  { net::ERR_CONNECTION_TIMED_OUT, "tcp.connection.timed_out" },
  { net::ERR_NAME_RESOLUTION_FAILED, "dns" },
  { net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
        "ssl.cert.pinned_key_not_in_cert_chain" },
  { net::ERR_CERT_COMMON_NAME_INVALID, "ssl.cert.name_invalid" },
  { net::ERR_CERT_DATE_INVALID, "ssl.cert.date_invalid" },
  { net::ERR_CERT_AUTHORITY_INVALID, "ssl.cert.authority_invalid" },
  { net::ERR_CERT_REVOKED, "ssl.cert.revoked" },
  { net::ERR_CERT_INVALID, "ssl.cert.invalid" },
  { net::ERR_EMPTY_RESPONSE, "http.response.empty" },
  { net::ERR_SPDY_PING_FAILED, "spdy.ping_failed" },
  { net::ERR_SPDY_PROTOCOL_ERROR, "spdy.protocol" },
  { net::ERR_QUIC_PROTOCOL_ERROR, "quic.protocol" },
  { net::ERR_DNS_MALFORMED_RESPONSE, "dns.protocol" },
  { net::ERR_DNS_SERVER_FAILED, "dns.server" },
  { net::ERR_DNS_TIMED_OUT, "dns.timed_out" },
  { net::ERR_INSECURE_RESPONSE, "ssl" },
  { net::ERR_CONTENT_LENGTH_MISMATCH, "http.response.content_length_mismatch" },
  { net::ERR_INCOMPLETE_CHUNKED_ENCODING,
        "http.response.incomplete_chunked_encoding" },
  { net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH,
        "ssl.version_or_cipher_mismatch" },
  { net::ERR_BAD_SSL_CLIENT_AUTH_CERT, "ssl.bad_client_auth_cert" },
  { net::ERR_INVALID_CHUNKED_ENCODING,
        "http.response.invalid_chunked_encoding" },
  { net::ERR_RESPONSE_HEADERS_TRUNCATED, "http.response.headers.truncated" },
  { net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
        "http.request.range_not_satisfiable" },
  { net::ERR_INVALID_RESPONSE, "http.response.invalid" },
  { net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION,
        "http.response.headers.multiple_content_disposition" },
  { net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH,
        "http.response.headers.multiple_content_length" },
  { net::ERR_SSL_UNRECOGNIZED_NAME_ALERT, "ssl.unrecognized_name_alert" }
};

const struct QuicErrorMapping {
  net::QuicErrorCode quic_error;
  const char* beacon_quic_error;
} quic_error_map[] = {
  // Connection has reached an invalid state.
  { net::QUIC_INTERNAL_ERROR, "quic.internal_error" },
  // There were data frames after the a fin or reset.
  { net::QUIC_STREAM_DATA_AFTER_TERMINATION,
   "quic.stream_data.after_termination" },
  // Control frame is malformed.
  { net::QUIC_INVALID_PACKET_HEADER, "quic.invalid.packet_header" },
  // Frame data is malformed.
  { net::QUIC_INVALID_FRAME_DATA, "quic.invalid_frame_data" },
  // The packet contained no payload.
  { net::QUIC_MISSING_PAYLOAD, "quic.missing.payload" },
  // FEC data is malformed.
  { net::QUIC_INVALID_FEC_DATA, "quic.invalid.fec_data" },
  // STREAM frame data is malformed.
  { net::QUIC_INVALID_STREAM_DATA, "quic.invalid.stream_data" },
  // STREAM frame data is not encrypted.
  { net::QUIC_UNENCRYPTED_STREAM_DATA, "quic.unencrypted.stream_data" },
  // FEC frame data is not encrypted.
  { net::QUIC_UNENCRYPTED_FEC_DATA, "quic.unencrypted.fec.data" },
  // RST_STREAM frame data is malformed.
  { net::QUIC_INVALID_RST_STREAM_DATA, "quic.invalid.rst_stream_data" },
  // CONNECTION_CLOSE frame data is malformed.
  { net::QUIC_INVALID_CONNECTION_CLOSE_DATA,
   "quic.invalid.connection_close_data" },
  // GOAWAY frame data is malformed.
  { net::QUIC_INVALID_GOAWAY_DATA, "quic.invalid.goaway_data" },
  // WINDOW_UPDATE frame data is malformed.
  { net::QUIC_INVALID_WINDOW_UPDATE_DATA, "quic.invalid.window_update_data" },
  // BLOCKED frame data is malformed.
  { net::QUIC_INVALID_BLOCKED_DATA, "quic.invalid.blocked_data" },
  // STOP_WAITING frame data is malformed.
  { net::QUIC_INVALID_STOP_WAITING_DATA, "quic.invalid.stop_waiting_data" },
  // PATH_CLOSE frame data is malformed.
  { net::QUIC_INVALID_PATH_CLOSE_DATA, "quic.invalid_path_close_data" },
  // ACK frame data is malformed.
  { net::QUIC_INVALID_ACK_DATA, "quic.invalid.ack_data" },

  // Version negotiation packet is malformed.
  { net::QUIC_INVALID_VERSION_NEGOTIATION_PACKET,
   "quic_invalid_version_negotiation_packet" },
  // Public RST packet is malformed.
  { net::QUIC_INVALID_PUBLIC_RST_PACKET, "quic.invalid.public_rst_packet" },

  // There was an error decrypting.
  { net::QUIC_DECRYPTION_FAILURE, "quic.decryption.failure" },
  // There was an error encrypting.
  { net::QUIC_ENCRYPTION_FAILURE, "quic.encryption.failure" },
  // The packet exceeded kMaxPacketSize.
  { net::QUIC_PACKET_TOO_LARGE, "quic.packet.too_large" },
  // The peer is going away.  May be a client or server.
  { net::QUIC_PEER_GOING_AWAY, "quic.peer_going_away" },
  // A stream ID was invalid.
  { net::QUIC_INVALID_STREAM_ID, "quic.invalid_stream_id" },
  // A priority was invalid.
  { net::QUIC_INVALID_PRIORITY, "quic.invalid_priority" },
  // Too many streams already open.
  { net::QUIC_TOO_MANY_OPEN_STREAMS, "quic.too_many_open_streams" },
  // The peer created too many available streams.
  { net::QUIC_TOO_MANY_AVAILABLE_STREAMS, "quic.too_many_available_streams" },
  // Received public reset for this connection.
  { net::QUIC_PUBLIC_RESET, "quic.public_reset" },
  // Invalid protocol version.
  { net::QUIC_INVALID_VERSION, "quic.invalid_version" },

  // The Header ID for a stream was too far from the previous.
  { net::QUIC_INVALID_HEADER_ID, "quic.invalid_header_id" },
  // Negotiable parameter received during handshake had invalid value.
  { net::QUIC_INVALID_NEGOTIATED_VALUE, "quic.invalid_negotiated_value" },
  // There was an error decompressing data.
  { net::QUIC_DECOMPRESSION_FAILURE, "quic.decompression_failure" },
  // We hit our prenegotiated (or default) timeout
  { net::QUIC_NETWORK_IDLE_TIMEOUT, "quic.connection.idle_time_out" },
  // We hit our overall connection timeout
  { net::QUIC_HANDSHAKE_TIMEOUT,
   "quic.connection.handshake_timed_out" },
  // There was an error encountered migrating addresses
  { net::QUIC_ERROR_MIGRATING_ADDRESS, "quic.error_migrating_address" },
  // There was an error while writing to the socket.
  { net::QUIC_PACKET_WRITE_ERROR, "quic.packet.write_error" },
  // There was an error while reading from the socket.
  { net::QUIC_PACKET_READ_ERROR, "quic.packet.read_error" },
  // We received a STREAM_FRAME with no data and no fin flag set.
  { net::QUIC_INVALID_STREAM_FRAME, "quic.invalid_stream_frame" },
  // We received invalid data on the headers stream.
  { net::QUIC_INVALID_HEADERS_STREAM_DATA, "quic.invalid_headers_stream_data" },
  // The peer received too much data, violating flow control.
  { net::QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA,
   "quic.flow_control.received_too_much_data" },
  // The peer sent too much data, violating flow control.
  { net::QUIC_FLOW_CONTROL_SENT_TOO_MUCH_DATA,
   "quic.flow_control.sent_too_much_data" },
  // The peer received an invalid flow control window.
  { net::QUIC_FLOW_CONTROL_INVALID_WINDOW, "quic.flow_control.invalid_window" },
  // The connection has been IP pooled into an existing connection.
  { net::QUIC_CONNECTION_IP_POOLED, "quic.connection.ip_pooled" },
  // The connection has too many outstanding sent packets.
  { net::QUIC_TOO_MANY_OUTSTANDING_SENT_PACKETS,
   "quic.too_many_outstanding_sent_packets" },
  // The connection has too many outstanding received packets.
  { net::QUIC_TOO_MANY_OUTSTANDING_RECEIVED_PACKETS,
   "quic.too_many_outstanding_received_packets" },
  // The quic connection job to load server config is cancelled.
  { net::QUIC_CONNECTION_CANCELLED, "quic.connection.cancelled" },
  // Disabled QUIC because of high packet loss rate.
  { net::QUIC_BAD_PACKET_LOSS_RATE, "quic.bad_packet_loss_rate" },
  // Disabled QUIC because of too many PUBLIC_RESETs post handshake.
  { net::QUIC_PUBLIC_RESETS_POST_HANDSHAKE,
   "quic.public_resets_post_handshake" },
  // Disabled QUIC because of too many timeouts with streams open.
  { net::QUIC_TIMEOUTS_WITH_OPEN_STREAMS, "quic.timeouts_with_open_streams" },
  // Closed because we failed to serialize a packet.
  { net::QUIC_FAILED_TO_SERIALIZE_PACKET, "quic.failed_to_serialize_packet" },

  // Crypto errors.

  // Hanshake failed.
  { net::QUIC_HANDSHAKE_FAILED, "quic.handshake_failed" },
  // Handshake message contained out of order tags.
  { net::QUIC_CRYPTO_TAGS_OUT_OF_ORDER, "quic.crypto.tags_out_of_order" },
  // Handshake message contained too many entries.
  { net::QUIC_CRYPTO_TOO_MANY_ENTRIES, "quic.crypto.too_many_entries" },
  // Handshake message contained an invalid value length.
  { net::QUIC_CRYPTO_INVALID_VALUE_LENGTH, "quic.crypto.invalid_value_length" },
  // A crypto message was received after the handshake was complete.
  { net::QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
   "quic.crypto_message_after_handshake_complete" },
  // A crypto message was received with an illegal message tag.
  { net::QUIC_INVALID_CRYPTO_MESSAGE_TYPE, "quic.invalid_crypto_message_type" },
  // A crypto message was received with an illegal parameter.
  { net::QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER,
   "quic.invalid_crypto_message_parameter" },
  // An invalid channel id signature was supplied.
  { net::QUIC_INVALID_CHANNEL_ID_SIGNATURE,
   "quic.invalid_channel_id_signature" },
  // A crypto message was received with a mandatory parameter missing.
  { net::QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND,
   "quic.crypto_message.parameter_not_found" },
  // A crypto message was received with a parameter that has no overlap
  // with the local parameter.
  { net::QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP,
   "quic.crypto_message.parameter_no_overlap" },
  // A crypto message was received that contained a parameter with too few
  // values.
  { net::QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND,
   "quic_crypto_message_index_not_found" },
  // An internal error occured in crypto processing.
  { net::QUIC_CRYPTO_INTERNAL_ERROR, "quic.crypto.internal_error" },
  // A crypto handshake message specified an unsupported version.
  { net::QUIC_CRYPTO_VERSION_NOT_SUPPORTED,
   "quic.crypto.version_not_supported" },
  // A crypto handshake message resulted in a stateless reject.
  { net::QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT,
   "quic.crypto.handshake_stateless_reject" },
  // There was no intersection between the crypto primitives supported by the
  // peer and ourselves.
  { net::QUIC_CRYPTO_NO_SUPPORT, "quic.crypto.no_support" },
  // The server rejected our client hello messages too many times.
  { net::QUIC_CRYPTO_TOO_MANY_REJECTS, "quic.crypto.too_many_rejects" },
  // The client rejected the server's certificate chain or signature.
  { net::QUIC_PROOF_INVALID, "quic.proof_invalid" },
  // A crypto message was received with a duplicate tag.
  { net::QUIC_CRYPTO_DUPLICATE_TAG, "quic.crypto.duplicate_tag" },
  // A crypto message was received with the wrong encryption level (i.e. it
  // should have been encrypted but was not.)
  { net::QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
   "quic.crypto.encryption_level_incorrect" },
  // The server config for a server has expired.
  { net::QUIC_CRYPTO_SERVER_CONFIG_EXPIRED,
   "quic.crypto.server_config_expired" },
  // We failed to setup the symmetric keys for a connection.
  { net::QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED,
   "quic.crypto.symmetric_key_setup_failed" },
  // A handshake message arrived, but we are still validating the
  // previous handshake message.
  { net::QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO,
   "quic.crypto_message_while_validating_client_hello" },
  // A server config update arrived before the handshake is complete.
  { net::QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE,
   "quic.crypto.update_before_handshake_complete" },
  // This connection involved a version negotiation which appears to have been
  // tampered with.
  { net::QUIC_VERSION_NEGOTIATION_MISMATCH,
   "quic.version_negotiation_mismatch" },

  // Multipath is not enabled, but a packet with multipath flag on is received.
  { net::QUIC_BAD_MULTIPATH_FLAG, "quic.bad_multipath_flag" },

  // Network change and connection migration errors.

  // IP address changed causing connection close.
  { net::QUIC_IP_ADDRESS_CHANGED, "quic.ip_address_changed" },
  // Network changed, but connection had no migratable streams.
  { net::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
    "quic.connection_migration_no_migratable_streams" },
  // Connection changed networks too many times.
  { net::QUIC_CONNECTION_MIGRATION_TOO_MANY_CHANGES,
    "quic.connection_migration_too_many_changes" },
  // Connection migration was attempted, but there was no new network to
  // migrate to.
  { net::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK,
    "quic.connection_migration_no_new_network" },

  // No error. Used as bound while iterating.
  { net::QUIC_LAST_ERROR, "quic.last_error"}
};

// static
const int quic_error_map_size =
    sizeof(quic_error_map) / sizeof(QuicErrorMapping);

static_assert(quic_error_map_size == net::kActiveQuicErrorCount,
              "quic_error_map is not in sync with quic protocol!");

bool CanReportFullBeaconURLToCollector(const GURL& beacon_url,
                                       const GURL& collector_url) {
  return beacon_url.GetOrigin() == collector_url.GetOrigin();
}

}  // namespace

// static
bool GetDomainReliabilityBeaconStatus(
    int net_error,
    int http_response_code,
    std::string* beacon_status_out) {
  if (net_error == net::OK) {
    if (http_response_code >= 400 && http_response_code < 600)
      *beacon_status_out = "http.error";
    else
      *beacon_status_out = "ok";
    return true;
  }

  // TODO(ttuttle): Consider sorting and using binary search?
  for (size_t i = 0; i < arraysize(net_error_map); i++) {
    if (net_error_map[i].net_error == net_error) {
      *beacon_status_out = net_error_map[i].beacon_status;
      return true;
    }
  }
  return false;
}

// static
bool GetDomainReliabilityBeaconQuicError(net::QuicErrorCode quic_error,
                                         std::string* beacon_quic_error_out) {
  if (quic_error != net::QUIC_NO_ERROR) {
    // Convert a QUIC error.
    // TODO(ttuttle): Consider sorting and using binary search?
    for (size_t i = 0; i < arraysize(quic_error_map); i++) {
      if (quic_error_map[i].quic_error == quic_error) {
        *beacon_quic_error_out = quic_error_map[i].beacon_quic_error;
        return true;
      }
    }
  }
  beacon_quic_error_out->clear();
  return false;
}

// TODO(ttuttle): Consider using NPN/ALPN instead, if there's a good way to
//                differentiate HTTP and HTTPS.
std::string GetDomainReliabilityProtocol(
    net::HttpResponseInfo::ConnectionInfo connection_info,
    bool ssl_info_populated) {
  switch (connection_info) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
      return "";
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1:
      return ssl_info_populated ? "HTTPS" : "HTTP";
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      return "SPDY";
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3:
      return "QUIC";
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

int GetNetErrorFromURLRequestStatus(const net::URLRequestStatus& status) {
  switch (status.status()) {
    case net::URLRequestStatus::SUCCESS:
      return net::OK;
    case net::URLRequestStatus::CANCELED:
      return net::ERR_ABORTED;
    case net::URLRequestStatus::FAILED:
      return status.error();
    default:
      NOTREACHED();
      return net::ERR_FAILED;
  }
}

void GetUploadResultFromResponseDetails(
    int net_error,
    int http_response_code,
    base::TimeDelta retry_after,
    DomainReliabilityUploader::UploadResult* result) {
  if (net_error == net::OK && http_response_code == 200) {
    result->status = DomainReliabilityUploader::UploadResult::SUCCESS;
    return;
  }

  if (net_error == net::OK &&
      http_response_code == 503 &&
      retry_after != base::TimeDelta()) {
    result->status = DomainReliabilityUploader::UploadResult::RETRY_AFTER;
    result->retry_after = retry_after;
    return;
  }

  result->status = DomainReliabilityUploader::UploadResult::FAILURE;
  return;
}

// N.B. This uses a ScopedVector because that's what JSONValueConverter uses
// for repeated fields of any type, and Config uses JSONValueConverter to parse
// JSON configs.
GURL SanitizeURLForReport(const GURL& beacon_url,
                          const GURL& collector_url,
                          const ScopedVector<std::string>& path_prefixes) {
  if (CanReportFullBeaconURLToCollector(beacon_url, collector_url))
    return beacon_url.GetAsReferrer();

  std::string path = beacon_url.path();
  const std::string empty_path;
  const std::string* longest_path_prefix = &empty_path;
  for (const std::string* path_prefix : path_prefixes) {
    if (path.substr(0, path_prefix->length()) == *path_prefix &&
        path_prefix->length() > longest_path_prefix->length()) {
      longest_path_prefix = path_prefix;
    }
  }

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.SetPathStr(*longest_path_prefix);
  replacements.ClearQuery();
  replacements.ClearRef();
  return beacon_url.ReplaceComponents(replacements);
}

namespace {

class ActualTimer : public MockableTime::Timer {
 public:
  // Initialize base timer with retain_user_info and is_repeating false.
  ActualTimer() : base_timer_(false, false) {}

  ~ActualTimer() override {}

  // MockableTime::Timer implementation:
  void Start(const tracked_objects::Location& posted_from,
             base::TimeDelta delay,
             const base::Closure& user_task) override {
    base_timer_.Start(posted_from, delay, user_task);
  }

  void Stop() override { base_timer_.Stop(); }

  bool IsRunning() override { return base_timer_.IsRunning(); }

 private:
  base::Timer base_timer_;
};

}  // namespace

MockableTime::Timer::~Timer() {}
MockableTime::Timer::Timer() {}

MockableTime::~MockableTime() {}
MockableTime::MockableTime() {}

ActualTime::ActualTime() {}
ActualTime::~ActualTime() {}

base::Time ActualTime::Now() { return base::Time::Now(); }
base::TimeTicks ActualTime::NowTicks() { return base::TimeTicks::Now(); }

scoped_ptr<MockableTime::Timer> ActualTime::CreateTimer() {
  return scoped_ptr<MockableTime::Timer>(new ActualTimer());
}

}  // namespace domain_reliability
