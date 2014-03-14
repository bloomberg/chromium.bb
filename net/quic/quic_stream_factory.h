// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_database.h"
#include "net/proxy/proxy_server.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session_key.h"

namespace net {

class CertVerifier;
class ClientSocketFactory;
class HostResolver;
class HttpServerProperties;
class QuicClock;
class QuicClientSession;
class QuicConnectionHelper;
class QuicCryptoClientStreamFactory;
class QuicRandom;
class QuicServerInfoFactory;
class QuicStreamFactory;

namespace test {
class QuicStreamFactoryPeer;
}  // namespace test

// Encapsulates a pending request for a QuicHttpStream.
// If the request is still pending when it is destroyed, it will
// cancel the request with the factory.
class NET_EXPORT_PRIVATE QuicStreamRequest {
 public:
  explicit QuicStreamRequest(QuicStreamFactory* factory);
  ~QuicStreamRequest();

  // For http, |is_https| is false and |cert_verifier| can be null.
  int Request(const HostPortPair& host_port_pair,
              bool is_https,
              base::StringPiece method,
              CertVerifier* cert_verifier,
              const BoundNetLog& net_log,
              const CompletionCallback& callback);

  void OnRequestComplete(int rv);

  scoped_ptr<QuicHttpStream> ReleaseStream();

  void set_stream(scoped_ptr<QuicHttpStream> stream);

  const BoundNetLog& net_log() const{
    return net_log_;
  }

 private:
  QuicStreamFactory* factory_;
  HostPortPair host_port_pair_;
  bool is_https_;
  CertVerifier* cert_verifier_;
  BoundNetLog net_log_;
  CompletionCallback callback_;
  scoped_ptr<QuicHttpStream> stream_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamRequest);
};

// A factory for creating new QuicHttpStreams on top of a pool of
// QuicClientSessions.
class NET_EXPORT_PRIVATE QuicStreamFactory
    : public NetworkChangeNotifier::IPAddressObserver,
      public CertDatabase::Observer {
 public:
  QuicStreamFactory(
      HostResolver* host_resolver,
      ClientSocketFactory* client_socket_factory,
      base::WeakPtr<HttpServerProperties> http_server_properties,
      QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
      QuicRandom* random_generator,
      QuicClock* clock,
      size_t max_packet_length,
      const QuicVersionVector& supported_versions,
      bool enable_port_selection,
      bool enable_pacing);
  virtual ~QuicStreamFactory();

  // Creates a new QuicHttpStream to |host_port_pair| which will be
  // owned by |request|. |is_https| specifies if the protocol is https or not.
  // |cert_verifier| is used by ProofVerifier for verifying the certificate
  // chain and signature. For http, this can be null. If a matching session
  // already exists, this method will return OK.  If no matching session exists,
  // this will return ERR_IO_PENDING and will invoke OnRequestComplete
  // asynchronously.
  int Create(const HostPortPair& host_port_pair,
             bool is_https,
             base::StringPiece method,
             CertVerifier* cert_verifier,
             const BoundNetLog& net_log,
             QuicStreamRequest* request);

  // Called by a session when it becomes idle.
  void OnIdleSession(QuicClientSession* session);

  // Called by a session when it is going away and no more streams should be
  // created on it.
  void OnSessionGoingAway(QuicClientSession* session);

  // Called by a session after it shuts down.
  void OnSessionClosed(QuicClientSession* session);

  // Cancels a pending request.
  void CancelRequest(QuicStreamRequest* request);

  // Closes all current sessions.
  void CloseAllSessions(int error);

  base::Value* QuicStreamFactoryInfoToValue() const;

  // NetworkChangeNotifier::IPAddressObserver methods:

  // Until the servers support roaming, close all connections when the local
  // IP address changes.
  virtual void OnIPAddressChanged() OVERRIDE;

  // CertDatabase::Observer methods:

  // We close all sessions when certificate database is changed.
  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE;
  virtual void OnCACertChanged(const X509Certificate* cert) OVERRIDE;

  bool require_confirmation() const { return require_confirmation_; }

  void set_require_confirmation(bool require_confirmation) {
    require_confirmation_ = require_confirmation;
  }

  QuicConnectionHelper* helper() { return helper_.get(); }

  bool enable_port_selection() const { return enable_port_selection_; }

  void set_quic_server_info_factory(
      QuicServerInfoFactory* quic_server_info_factory) {
    DCHECK(!quic_server_info_factory_);
    quic_server_info_factory_ = quic_server_info_factory;
  }

  bool enable_pacing() const { return enable_pacing_; }

 private:
  class Job;
  friend class test::QuicStreamFactoryPeer;

  // The key used to find session by hostname. Includes
  // the ip address, port, and scheme.
  struct NET_EXPORT_PRIVATE IpAliasKey {
    IpAliasKey();
    IpAliasKey(IPEndPoint ip_endpoint, bool is_https);
    ~IpAliasKey();

    IPEndPoint ip_endpoint;
    bool is_https;

    // Needed to be an element of std::set.
    bool operator<(const IpAliasKey &other) const;
    bool operator==(const IpAliasKey &other) const;
  };

  typedef std::map<QuicSessionKey, QuicClientSession*> SessionMap;
  typedef std::set<QuicSessionKey> AliasSet;
  typedef std::map<QuicClientSession*, AliasSet> SessionAliasMap;
  typedef std::set<QuicClientSession*> SessionSet;
  typedef std::map<IpAliasKey, SessionSet> IPAliasMap;
  typedef std::map<QuicSessionKey, QuicCryptoClientConfig*> CryptoConfigMap;
  typedef std::map<QuicSessionKey, QuicSessionKey> CanonicalHostMap;
  typedef std::map<QuicSessionKey, Job*> JobMap;
  typedef std::map<QuicStreamRequest*, Job*> RequestMap;
  typedef std::set<QuicStreamRequest*> RequestSet;
  typedef std::map<Job*, RequestSet> JobRequestsMap;

  // Returns a newly created QuicHttpStream owned by the caller, if a
  // matching session already exists.  Returns NULL otherwise.
  scoped_ptr<QuicHttpStream> CreateIfSessionExists(const QuicSessionKey& key,
                                                   const BoundNetLog& net_log);

  bool OnResolution(const QuicSessionKey& session_key,
                    const AddressList& address_list);
  void OnJobComplete(Job* job, int rv);
  bool HasActiveSession(const QuicSessionKey& session_key) const;
  bool HasActiveJob(const QuicSessionKey& session_key) const;
  int CreateSession(const HostPortPair& host_port_pair,
                    bool is_https,
                    CertVerifier* cert_verifier,
                    const AddressList& address_list,
                    const BoundNetLog& net_log,
                    QuicClientSession** session);
  void ActivateSession(const QuicSessionKey& key,
                       QuicClientSession* session);

  QuicCryptoClientConfig* GetOrCreateCryptoConfig(
      const QuicSessionKey& session_key);

  // If the suffix of the hostname in |session_key| is in |canoncial_suffixes_|,
  // then populate |crypto_config| with a canonical server config data from
  // |canonical_hostname_to_origin_map_| for that suffix.
  void PopulateFromCanonicalConfig(
      const QuicSessionKey& session_key,
      QuicCryptoClientConfig* crypto_config);

  void ExpireBrokenAlternateProtocolMappings();
  void ScheduleBrokenAlternateProtocolMappingsExpiration();

  bool require_confirmation_;
  HostResolver* host_resolver_;
  ClientSocketFactory* client_socket_factory_;
  base::WeakPtr<HttpServerProperties> http_server_properties_;
  QuicServerInfoFactory* quic_server_info_factory_;
  QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory_;
  QuicRandom* random_generator_;
  scoped_ptr<QuicClock> clock_;
  const size_t max_packet_length_;

  // The helper used for all connections.
  scoped_ptr<QuicConnectionHelper> helper_;

  // Contains owning pointers to all sessions that currently exist.
  SessionSet all_sessions_;
  // Contains non-owning pointers to currently active session
  // (not going away session, once they're implemented).
  SessionMap active_sessions_;
  // Map from session to set of aliases that this session is known by.
  SessionAliasMap session_aliases_;
  // Map from IP address to sessions which are connected to this address.
  IPAliasMap ip_aliases_;

  // Origins which have gone away recently.
  AliasSet gone_away_aliases_;

  // Contains owning pointers to QuicCryptoClientConfig. QuicCryptoClientConfig
  // contains configuration and cached state about servers.
  // TODO(rtenneti): Persist all_crypto_configs_ to disk and decide when to
  // clear the data in the map.
  CryptoConfigMap all_crypto_configs_;

  // Contains a map of servers which could share the same server config. Map
  // from a Canonical host/port/scheme (host is some postfix of host names) to
  // an actual origin, which has a plausible set of initial certificates (or at
  // least server public key).
  CanonicalHostMap canonical_hostname_to_origin_map_;

  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com") of canoncial hostnames.
  std::vector<std::string> canoncial_suffixes_;


  // List of broken host:ports and the times when they can be expired.
  struct BrokenAlternateProtocolEntry {
    HostPortPair origin;
    base::TimeTicks when;
  };
  typedef std::list<BrokenAlternateProtocolEntry>
      BrokenAlternateProtocolList;
  BrokenAlternateProtocolList broken_alternate_protocol_list_;

  // Map from host:port to the number of times alternate protocol has
  // been marked broken.
  typedef std::map<HostPortPair, int> BrokenAlternateProtocolMap;
  BrokenAlternateProtocolMap broken_alternate_protocol_map_;

  QuicConfig config_;

  JobMap active_jobs_;
  JobRequestsMap job_requests_map_;
  RequestMap active_requests_;

  QuicVersionVector supported_versions_;

  // Determine if we should consistently select a client UDP port. If false,
  // then we will just let the OS select a random client port for each new
  // connection.
  bool enable_port_selection_;

  // True if packet pacing should be advertised during the crypto handshake.
  bool enable_pacing_;

  // Each profile will (probably) have a unique port_seed_ value.  This value is
  // used to help seed a pseudo-random number generator (PortSuggester) so that
  // we consistently (within this profile) suggest the same ephemeral port when
  // we re-connect to any given server/port.  The differences between profiles
  // (probablistically) prevent two profiles from colliding in their ephemeral
  // port requests.
  uint64 port_seed_;

  base::WeakPtrFactory<QuicStreamFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamFactory);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_H_
