// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONFIG_H_
#define NET_QUIC_QUIC_CONFIG_H_

#include <string>

#include "base/basictypes.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class CryptoHandshakeMessage;

// Describes whether or not a given QuicTag is required or optional in the
// handshake message.
enum QuicConfigPresence {
  // This value can be absent from the handshake message. Default value is
  // selected as the negotiated value in such a case.
  PRESENCE_OPTIONAL,
  // This value is required in the handshake message otherwise the Process*Hello
  // function returns an error.
  PRESENCE_REQUIRED,
};

// An abstract base class that stores a value that can be sent in CHLO/SHLO
// message. These values can be OPTIONAL or REQUIRED, depending on |presence_|.
class NET_EXPORT_PRIVATE QuicConfigValue {
 public:
  QuicConfigValue(QuicTag tag, QuicConfigPresence presence);
  virtual ~QuicConfigValue();

  // Serialises tag name and value(s) to |out|.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const = 0;

  // Selects a mutually acceptable value from those offered in |client_hello|
  // and those defined in the subclass.
  virtual QuicErrorCode ProcessClientHello(
      const CryptoHandshakeMessage& client_hello,
      std::string* error_details) = 0;

  // Selects a mutually acceptable value from those offered in |server_hello|
  // and those defined in the subclass.
  virtual QuicErrorCode ProcessServerHello(
      const CryptoHandshakeMessage& server_hello,
      std::string* error_details) = 0;

 protected:
  const QuicTag tag_;
  const QuicConfigPresence presence_;
};

class NET_EXPORT_PRIVATE QuicNegotiableValue : public QuicConfigValue {
 public:
  QuicNegotiableValue(QuicTag tag, QuicConfigPresence presence);
  virtual ~QuicNegotiableValue();

  bool negotiated() const {
    return negotiated_;
  }

 protected:
  bool negotiated_;
};

class NET_EXPORT_PRIVATE QuicNegotiableUint32 : public QuicNegotiableValue {
 public:
  // Default and max values default to 0.
  QuicNegotiableUint32(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicNegotiableUint32();

  // Sets the maximum possible value that can be achieved after negotiation and
  // also the default values to be assumed if PRESENCE_OPTIONAL and the *HLO msg
  // doesn't contain a value corresponding to |name_|. |max| is serialised via
  // ToHandshakeMessage call if |negotiated_| is false.
  void set(uint32 max, uint32 default_value);

  // Returns the value negotiated if |negotiated_| is true, otherwise returns
  // default_value_ (used to set default values before negotiation finishes).
  uint32 GetUint32() const;

  // Serialises |name_| and value to |out|. If |negotiated_| is true then
  // |negotiated_value_| is serialised, otherwise |max_value_| is serialised.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Sets |negotiated_value_| to the minimum of |max_value_| and the
  // corresponding value from |client_hello|. If the corresponding value is
  // missing and PRESENCE_OPTIONAL then |negotiated_value_| is set to
  // |default_value_|.
  virtual QuicErrorCode ProcessClientHello(
      const CryptoHandshakeMessage& client_hello,
      std::string* error_details) OVERRIDE;

  // Sets the |negotiated_value_| to the corresponding value from
  // |server_hello|. Returns error if the value received in |server_hello| is
  // greater than |max_value_|. If the corresponding value is missing and
  // PRESENCE_OPTIONAL then |negotiated_value_| is set to |0|,
  virtual QuicErrorCode ProcessServerHello(
      const CryptoHandshakeMessage& server_hello,
      std::string* error_details) OVERRIDE;

 private:
  uint32 max_value_;
  uint32 default_value_;
  uint32 negotiated_value_;
};

class NET_EXPORT_PRIVATE QuicNegotiableTag : public QuicNegotiableValue {
 public:
  QuicNegotiableTag(QuicTag name, QuicConfigPresence presence);
  virtual ~QuicNegotiableTag();

  // Sets the possible values that |negotiated_tag_| can take after negotiation
  // and the default value that |negotiated_tag_| takes if OPTIONAL and *HLO
  // msg doesn't contain tag |name_|.
  void set(const QuicTagVector& possible_values, QuicTag default_value);

  // Returns the negotiated tag if |negotiated_| is true, otherwise returns
  // |default_value_| (used to set default values before negotiation finishes).
  QuicTag GetTag() const;

  // Serialises |name_| and vector (either possible or negotiated) to |out|. If
  // |negotiated_| is true then |negotiated_tag_| is serialised, otherwise
  // |possible_values_| is serialised.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Selects the tag common to both tags in |client_hello| for |name_| and
  // |possible_values_| with preference to tag in |possible_values_|. The
  // selected tag is set as |negotiated_tag_|.
  virtual QuicErrorCode ProcessClientHello(
      const CryptoHandshakeMessage& client_hello,
      std::string* error_details) OVERRIDE;

  // Sets the value for |name_| tag in |server_hello| as |negotiated_value_|.
  // Returns error if the value received in |server_hello| isn't present in
  // |possible_values_|.
  virtual QuicErrorCode ProcessServerHello(
      const CryptoHandshakeMessage& server_hello,
      std::string* error_details) OVERRIDE;

 private:
  // Reads the vector corresponding to |name_| from |msg| into |out|. If the
  // |name_| is absent in |msg| and |presence_| is set to OPTIONAL |out| is set
  // to |possible_values_|.
  QuicErrorCode ReadVector(const CryptoHandshakeMessage& msg,
                           const QuicTag** out,
                           size_t* out_length,
                           std::string* error_details) const;

  QuicTag negotiated_tag_;
  QuicTagVector possible_values_;
  QuicTag default_value_;
};

// Stores uint32 from CHLO or SHLO messages that are not negotiated.
class NET_EXPORT_PRIVATE QuicFixedUint32 : public QuicConfigValue {
 public:
  QuicFixedUint32(QuicTag name,
                  QuicConfigPresence presence,
                  uint32 default_value);
  virtual ~QuicFixedUint32();

  // Returns the value in the *HLO message (or the default if not).
  uint32 GetUint32() const;

  void set_value(uint32 value) { value_ = value; }

  // Serialises |name_| and |value_| to |out|.
  virtual void ToHandshakeMessage(CryptoHandshakeMessage* out) const OVERRIDE;

  // Sets |value_| to the corresponding value from |client_hello_| if it exists.
  virtual QuicErrorCode ProcessClientHello(
      const CryptoHandshakeMessage& client_hello,
      std::string* error_details) OVERRIDE;

  // Sets |value_| to the corresponding value from |server_hello_| if it exists.
  virtual QuicErrorCode ProcessServerHello(
      const CryptoHandshakeMessage& server_hello,
      std::string* error_details) OVERRIDE;

 private:
  uint32 value_;
};

// QuicConfig contains non-crypto configuration options that are negotiated in
// the crypto handshake.
class NET_EXPORT_PRIVATE QuicConfig {
 public:
  QuicConfig();
  ~QuicConfig();

  void set_congestion_control(const QuicTagVector& congestion_control,
                              QuicTag default_congestion_control);

  QuicTag congestion_control() const;

  void set_idle_connection_state_lifetime(
      QuicTime::Delta max_idle_connection_state_lifetime,
      QuicTime::Delta default_idle_conection_state_lifetime);

  QuicTime::Delta idle_connection_state_lifetime() const;

  QuicTime::Delta keepalive_timeout() const;

  void set_max_streams_per_connection(size_t max_streams,
                                      size_t default_streams);

  uint32 max_streams_per_connection() const;

  void set_max_time_before_crypto_handshake(
      QuicTime::Delta max_time_before_crypto_handshake);

  QuicTime::Delta max_time_before_crypto_handshake() const;

  // Sets the server's TCP sender's max and default initial congestion window
  // in packets.
  void set_server_initial_congestion_window(size_t max_initial_window,
                                            size_t default_initial_window);

  uint32 server_initial_congestion_window() const;

  // Sets an estimated initial round trip time in us.
  void set_initial_round_trip_time_us(size_t max_rtt, size_t default_rtt);

  uint32 initial_round_trip_time_us() const;

  void set_peer_initial_flow_control_window_bytes(uint32 window);

  uint32 peer_initial_flow_control_window_bytes() const;

  bool negotiated();

  // SetDefaults sets the members to sensible, default values.
  void SetDefaults();

  // Enabled pacing.
  void EnablePacing(bool enable_pacing);

  // ToHandshakeMessage serializes the settings in this object as a series of
  // tags /value pairs and adds them to |out|.
  void ToHandshakeMessage(CryptoHandshakeMessage* out) const;

  // Calls ProcessClientHello on each negotiable parameter. On failure returns
  // the corresponding QuicErrorCode and sets detailed error in |error_details|.
  QuicErrorCode ProcessClientHello(const CryptoHandshakeMessage& client_hello,
                                   std::string* error_details);

  // Calls ProcessServerHello on each negotiable parameter. On failure returns
  // the corresponding QuicErrorCode and sets detailed error in |error_details|.
  QuicErrorCode ProcessServerHello(const CryptoHandshakeMessage& server_hello,
                                   std::string* error_details);

 private:
  // Congestion control feedback type.
  QuicNegotiableTag congestion_control_;
  // Idle connection state lifetime
  QuicNegotiableUint32 idle_connection_state_lifetime_seconds_;
  // Keepalive timeout, or 0 to turn off keepalive probes
  QuicNegotiableUint32 keepalive_timeout_seconds_;
  // Maximum number of streams that the connection can support.
  QuicNegotiableUint32 max_streams_per_connection_;
  // Maximum time till the session can be alive before crypto handshake is
  // finished. (Not negotiated).
  QuicTime::Delta max_time_before_crypto_handshake_;
  // Initial congestion window in packets.
  QuicNegotiableUint32 server_initial_congestion_window_;
  // Initial round trip time estimate in microseconds.
  QuicNegotiableUint32 initial_round_trip_time_us_;
  // Peer's initial flow control receive window in bytes.
  QuicFixedUint32 peer_initial_flow_control_window_bytes_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONFIG_H_
