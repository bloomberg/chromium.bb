// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_H_

#include <map>
#include <string>
#include <vector>

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
  int Request(const HostPortProxyPair& host_port_proxy_pair,
              bool is_https,
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
  HostPortProxyPair host_port_proxy_pair_;
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
      size_t max_packet_length);
  virtual ~QuicStreamFactory();

  // Creates a new QuicHttpStream to |host_port_proxy_pair| which will be
  // owned by |request|. |is_https| specifies if the protocol is https or not.
  // |cert_verifier| is used by ProofVerifier for verifying the certificate
  // chain and signature. For http, this can be null. If a matching session
  // already exists, this method will return OK.  If no matching session exists,
  // this will return ERR_IO_PENDING and will invoke OnRequestComplete
  // asynchronously.
  int Create(const HostPortProxyPair& host_port_proxy_pair,
             bool is_https,
             CertVerifier* cert_verifier,
             const BoundNetLog& net_log,
             QuicStreamRequest* request);

  // Returns a newly created QuicHttpStream owned by the caller, if a
  // matching session already exists.  Returns NULL otherwise.
  scoped_ptr<QuicHttpStream> CreateIfSessionExists(
      const HostPortProxyPair& host_port_proxy_pair,
      const BoundNetLog& net_log);

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

 private:
  class Job;
  friend class test::QuicStreamFactoryPeer;

  typedef std::map<HostPortProxyPair, QuicClientSession*> SessionMap;
  typedef std::set<HostPortProxyPair> AliasSet;
  typedef std::map<QuicClientSession*, AliasSet> SessionAliasMap;
  typedef std::set<QuicClientSession*> SessionSet;
  typedef std::map<HostPortProxyPair, QuicCryptoClientConfig*> CryptoConfigMap;
  typedef std::map<HostPortPair, HostPortProxyPair> CanonicalHostMap;
  typedef std::map<HostPortProxyPair, Job*> JobMap;
  typedef std::map<QuicStreamRequest*, Job*> RequestMap;
  typedef std::set<QuicStreamRequest*> RequestSet;
  typedef std::map<Job*, RequestSet> JobRequestsMap;

  void OnJobComplete(Job* job, int rv);
  bool HasActiveSession(const HostPortProxyPair& host_port_proxy_pair);
  bool HasActiveJob(const HostPortProxyPair& host_port_proxy_pair);
  int CreateSession(const HostPortProxyPair& host_port_proxy_pair,
                    bool is_https,
                    CertVerifier* cert_verifier,
                    const AddressList& address_list,
                    const BoundNetLog& net_log,
                    QuicClientSession** session);
  void ActivateSession(const HostPortProxyPair& host_port_proxy_pair,
                       QuicClientSession* session);

  QuicCryptoClientConfig* GetOrCreateCryptoConfig(
      const HostPortProxyPair& host_port_proxy_pair);

  // If |host_port_proxy_pair| suffix contains ".c.youtube.com" (in future we
  // could support other suffixes), then populate |crypto_config| with a
  // canonical server config data from |canonical_hostname_to_origin_map_| for
  // that suffix.
  void PopulateFromCanonicalConfig(
      const HostPortProxyPair& host_port_proxy_pair,
      QuicCryptoClientConfig* crypto_config);

  bool require_confirmation_;
  HostResolver* host_resolver_;
  ClientSocketFactory* client_socket_factory_;
  base::WeakPtr<HttpServerProperties> http_server_properties_;
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
  SessionAliasMap session_aliases_;

  // Contains owning pointers to QuicCryptoClientConfig. QuicCryptoClientConfig
  // contains configuration and cached state about servers.
  // TODO(rtenneti): Persist all_crypto_configs_ to disk and decide when to
  // clear the data in the map.
  CryptoConfigMap all_crypto_configs_;

  // Contains a map of servers which could share the same server config. Map
  // from a Canonical host/port (host is some postfix of host names) to an
  // actual origin, which has a plausible set of initial certificates (or at
  // least server public key).
  CanonicalHostMap canonical_hostname_to_origin_map_;

  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com") of cannoncial hostnames.
  std::vector<std::string> cannoncial_suffixes_;

  QuicConfig config_;

  JobMap active_jobs_;
  JobRequestsMap job_requests_map_;
  RequestMap active_requests_;

  base::WeakPtrFactory<QuicStreamFactory> weak_factory_;

  // Each profile will (probably) have a unique port_seed_ value.  This value is
  // used to help seed a pseudo-random number generator (PortSuggester) so that
  // we consistently (within this profile) suggest the same ephemeral port when
  // we re-connect to any given server/port.  The differences between profiles
  // (probablistically) prevent two profiles from colliding in their ephemeral
  // port requests.
  uint64 port_seed_;

  DISALLOW_COPY_AND_ASSIGN(QuicStreamFactory);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_H_
