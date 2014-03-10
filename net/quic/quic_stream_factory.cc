// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <set>

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/http/http_server_properties.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/port_suggester.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/socket/client_socket_factory.h"

using std::string;
using std::vector;

namespace net {

QuicStreamFactory::SessionKey::SessionKey() {}

QuicStreamFactory::SessionKey::SessionKey(
    HostPortProxyPair host_port_proxy_pair,
    bool is_https)
    : host_port_proxy_pair(host_port_proxy_pair),
      is_https(is_https) {}

QuicStreamFactory::SessionKey::~SessionKey() {}

bool QuicStreamFactory::SessionKey::operator<(
    const QuicStreamFactory::SessionKey &other) const {
  if (!host_port_proxy_pair.first.Equals(other.host_port_proxy_pair.first)) {
    return host_port_proxy_pair.first < other.host_port_proxy_pair.first;
  }
  if (!(host_port_proxy_pair.second == other.host_port_proxy_pair.second)) {
    return host_port_proxy_pair.second < other.host_port_proxy_pair.second;
  }
  return is_https < other.is_https;
}

bool QuicStreamFactory::SessionKey::operator==(
    const QuicStreamFactory::SessionKey& other) const {
  return is_https == other.is_https &&
      host_port_proxy_pair.second == other.host_port_proxy_pair.second &&
      host_port_proxy_pair.first.Equals(other.host_port_proxy_pair.first);
};

QuicStreamFactory::IpAliasKey::IpAliasKey() {}

QuicStreamFactory::IpAliasKey::IpAliasKey(IPEndPoint ip_endpoint,
                                          bool is_https)
    : ip_endpoint(ip_endpoint),
      is_https(is_https) {}

QuicStreamFactory::IpAliasKey::~IpAliasKey() {}

bool QuicStreamFactory::IpAliasKey::operator<(
    const QuicStreamFactory::IpAliasKey& other) const {
  if (!(ip_endpoint == other.ip_endpoint)) {
    return ip_endpoint < other.ip_endpoint;
  }
  return is_https < other.is_https;
}

bool QuicStreamFactory::IpAliasKey::operator==(
    const QuicStreamFactory::IpAliasKey& other) const {
  return is_https == other.is_https &&
      ip_endpoint == other.ip_endpoint;
};

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete.
class QuicStreamFactory::Job {
 public:
  Job(QuicStreamFactory* factory,
      HostResolver* host_resolver,
      const HostPortProxyPair& host_port_proxy_pair,
      bool is_https,
      base::StringPiece method,
      CertVerifier* cert_verifier,
      const BoundNetLog& net_log);

  ~Job();

  int Run(const CompletionCallback& callback);

  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoConnect();
  int DoConnectComplete(int rv);

  void OnIOComplete(int rv);

  CompletionCallback callback() {
    return callback_;
  }

  const SessionKey session_key() const {
    return session_key_;
  }

 private:
  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
  };
  IoState io_state_;

  QuicStreamFactory* factory_;
  SingleRequestHostResolver host_resolver_;
  bool is_https_;
  SessionKey session_key_;
  bool is_post_;
  CertVerifier* cert_verifier_;
  const BoundNetLog net_log_;
  QuicClientSession* session_;
  CompletionCallback callback_;
  AddressList address_list_;
  DISALLOW_COPY_AND_ASSIGN(Job);
};

QuicStreamFactory::Job::Job(QuicStreamFactory* factory,
                            HostResolver* host_resolver,
                            const HostPortProxyPair& host_port_proxy_pair,
                            bool is_https,
                            base::StringPiece method,
                            CertVerifier* cert_verifier,
                            const BoundNetLog& net_log)
    : factory_(factory),
      host_resolver_(host_resolver),
      is_https_(is_https),
      session_key_(host_port_proxy_pair, is_https),
      is_post_(method == "POST"),
      cert_verifier_(cert_verifier),
      net_log_(net_log),
      session_(NULL) {}

QuicStreamFactory::Job::~Job() {
}

int QuicStreamFactory::Job::Run(const CompletionCallback& callback) {
  io_state_ = STATE_RESOLVE_HOST;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv > 0 ? OK : rv;
}

int QuicStreamFactory::Job::DoLoop(int rv) {
  do {
    IoState state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        CHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_CONNECT:
        CHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

void QuicStreamFactory::Job::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    callback_.Run(rv);
  }
}

int QuicStreamFactory::Job::DoResolveHost() {
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  return host_resolver_.Resolve(
      HostResolver::RequestInfo(session_key_.host_port_proxy_pair.first),
      DEFAULT_PRIORITY,
      &address_list_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete, base::Unretained(this)),
      net_log_);
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(session_key_));

  // Inform the factory of this resolution, which will set up
  // a session alias, if possible.
  if (factory_->OnResolution(session_key_, address_list_)) {
    return OK;
  }

  io_state_ = STATE_CONNECT;
  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory) {}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(const HostPortProxyPair& host_port_proxy_pair,
                               bool is_https,
                               base::StringPiece method,
                               CertVerifier* cert_verifier,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) {
  DCHECK(!stream_);
  DCHECK(callback_.is_null());
  DCHECK(factory_);
  int rv = factory_->Create(host_port_proxy_pair, is_https,
                            method, cert_verifier, net_log, this);
  if (rv == ERR_IO_PENDING) {
    host_port_proxy_pair_ = host_port_proxy_pair;
    is_https_ = is_https;
    cert_verifier_ = cert_verifier;
    net_log_ = net_log;
    callback_ = callback;
  } else {
    factory_ = NULL;
  }
  if (rv == OK)
    DCHECK(stream_);
  return rv;
}

void QuicStreamRequest::set_stream(scoped_ptr<QuicHttpStream> stream) {
  DCHECK(stream);
  stream_ = stream.Pass();
}

void QuicStreamRequest::OnRequestComplete(int rv) {
  factory_ = NULL;
  callback_.Run(rv);
}

scoped_ptr<QuicHttpStream> QuicStreamRequest::ReleaseStream() {
  DCHECK(stream_);
  return stream_.Pass();
}

int QuicStreamFactory::Job::DoConnect() {
  io_state_ = STATE_CONNECT_COMPLETE;

  int rv = factory_->CreateSession(session_key_.host_port_proxy_pair, is_https_,
      cert_verifier_, address_list_, net_log_, &session_);
  if (rv != OK) {
    DCHECK(rv != ERR_IO_PENDING);
    DCHECK(!session_);
    return rv;
  }

  session_->StartReading();
  if (!session_->connection()->connected()) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  rv = session_->CryptoConnect(
      factory_->require_confirmation() || is_https_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 base::Unretained(this)));
  return rv;
}

int QuicStreamFactory::Job::DoConnectComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(session_key_));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  AddressList address(session_->connection()->peer_address());
  if (factory_->OnResolution(session_key_, address)) {
    session_->connection()->SendConnectionClose(QUIC_NO_ERROR);
    session_ = NULL;
    return OK;
  }

  factory_->ActivateSession(session_key_, session_);

  return OK;
}

QuicStreamFactory::QuicStreamFactory(
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    base::WeakPtr<HttpServerProperties> http_server_properties,
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
    QuicRandom* random_generator,
    QuicClock* clock,
    size_t max_packet_length,
    const QuicVersionVector& supported_versions,
    bool enable_port_selection,
    bool enable_pacing)
    : require_confirmation_(true),
      host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      http_server_properties_(http_server_properties),
      quic_server_info_factory_(NULL),
      quic_crypto_client_stream_factory_(quic_crypto_client_stream_factory),
      random_generator_(random_generator),
      clock_(clock),
      max_packet_length_(max_packet_length),
      supported_versions_(supported_versions),
      enable_port_selection_(enable_port_selection),
      enable_pacing_(enable_pacing),
      port_seed_(random_generator_->RandUint64()),
      weak_factory_(this) {
  config_.SetDefaults();
  config_.EnablePacing(enable_pacing_);
  config_.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(30),
      QuicTime::Delta::FromSeconds(30));

  canoncial_suffixes_.push_back(string(".c.youtube.com"));
  canoncial_suffixes_.push_back(string(".googlevideo.com"));
}

QuicStreamFactory::~QuicStreamFactory() {
  CloseAllSessions(ERR_ABORTED);
  STLDeleteElements(&all_sessions_);
  STLDeleteValues(&active_jobs_);
  STLDeleteValues(&all_crypto_configs_);
}

int QuicStreamFactory::Create(const HostPortProxyPair& host_port_proxy_pair,
                              bool is_https,
                              base::StringPiece method,
                              CertVerifier* cert_verifier,
                              const BoundNetLog& net_log,
                              QuicStreamRequest* request) {
  SessionKey session_key(host_port_proxy_pair, is_https);
  if (HasActiveSession(session_key)) {
    request->set_stream(CreateIfSessionExists(session_key, net_log));
    return OK;
  }

  if (HasActiveJob(session_key)) {
    Job* job = active_jobs_[session_key];
    active_requests_[request] = job;
    job_requests_map_[job].insert(request);
    return ERR_IO_PENDING;
  }

  // Create crypto config and start the process of loading QUIC server
  // information from disk cache.
  QuicCryptoClientConfig* crypto_config = GetOrCreateCryptoConfig(session_key);
  DCHECK(crypto_config);

  scoped_ptr<Job> job(new Job(this, host_resolver_, host_port_proxy_pair,
                              is_https, method, cert_verifier, net_log));
  int rv = job->Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job.get()));

  if (rv == ERR_IO_PENDING) {
    active_requests_[request] = job.get();
    job_requests_map_[job.get()].insert(request);
    active_jobs_[session_key] = job.release();
  }
  if (rv == OK) {
    DCHECK(HasActiveSession(session_key));
    request->set_stream(CreateIfSessionExists(session_key, net_log));
  }
  return rv;
}

bool QuicStreamFactory::OnResolution(
    const SessionKey& session_key,
    const AddressList& address_list) {
  DCHECK(!HasActiveSession(session_key));
  for (size_t i = 0; i < address_list.size(); ++i) {
    const IPEndPoint& address = address_list[i];
    const IpAliasKey ip_alias_key(address, session_key.is_https);
    if (!ContainsKey(ip_aliases_, ip_alias_key))
      continue;

    const SessionSet& sessions = ip_aliases_[ip_alias_key];
    for (SessionSet::const_iterator i = sessions.begin();
         i != sessions.end(); ++i) {
      QuicClientSession* session = *i;
      if (!session->CanPool(session_key.host_port_proxy_pair.first.host()))
        continue;
      active_sessions_[session_key] = session;
      session_aliases_[session].insert(session_key);
      return true;
    }
  }
  return false;
}

void QuicStreamFactory::OnJobComplete(Job* job, int rv) {
  if (rv == OK) {
    require_confirmation_ = false;

    // Create all the streams, but do not notify them yet.
    for (RequestSet::iterator it = job_requests_map_[job].begin();
         it != job_requests_map_[job].end() ; ++it) {
      DCHECK(HasActiveSession(job->session_key()));
      (*it)->set_stream(CreateIfSessionExists(job->session_key(),
                                              (*it)->net_log()));
    }
  }
  while (!job_requests_map_[job].empty()) {
    RequestSet::iterator it = job_requests_map_[job].begin();
    QuicStreamRequest* request = *it;
    job_requests_map_[job].erase(it);
    active_requests_.erase(request);
    // Even though we're invoking callbacks here, we don't need to worry
    // about |this| being deleted, because the factory is owned by the
    // profile which can not be deleted via callbacks.
    request->OnRequestComplete(rv);
  }
  active_jobs_.erase(job->session_key());
  job_requests_map_.erase(job);
  delete job;
  return;
}

// Returns a newly created QuicHttpStream owned by the caller, if a
// matching session already exists.  Returns NULL otherwise.
scoped_ptr<QuicHttpStream> QuicStreamFactory::CreateIfSessionExists(
    const SessionKey& session_key,
    const BoundNetLog& net_log) {
  if (!HasActiveSession(session_key)) {
    DVLOG(1) << "No active session";
    return scoped_ptr<QuicHttpStream>();
  }

  QuicClientSession* session = active_sessions_[session_key];
  DCHECK(session);
  return scoped_ptr<QuicHttpStream>(
      new QuicHttpStream(session->GetWeakPtr()));
}

void QuicStreamFactory::OnIdleSession(QuicClientSession* session) {
}

void QuicStreamFactory::OnSessionGoingAway(QuicClientSession* session) {
  const QuicConnectionStats& stats = session->connection()->GetStats();
  const AliasSet& aliases = session_aliases_[session];
  for (AliasSet::const_iterator it = aliases.begin(); it != aliases.end();
       ++it) {
    DCHECK(active_sessions_.count(*it));
    DCHECK_EQ(session, active_sessions_[*it]);
    // Track sessions which have recently gone away so that we can disable
    // port suggestions.
    if (session->goaway_received()) {
      gone_away_aliases_.insert(*it);
    }

    active_sessions_.erase(*it);
    if (!http_server_properties_)
      continue;

    if (!session->IsCryptoHandshakeConfirmed()) {
      // TODO(rch):  In the special case where the session has received no
      // packets from the peer, we should consider blacklisting this
      // differently so that we still race TCP but we don't consider the
      // session connected until the handshake has been confirmed.
      http_server_properties_->SetBrokenAlternateProtocol(
          it->host_port_proxy_pair.first);
      UMA_HISTOGRAM_COUNTS("Net.QuicHandshakeNotConfirmedNumPacketsReceived",
                           stats.packets_received);
      continue;
    }

    HttpServerProperties::NetworkStats network_stats;
    network_stats.rtt = base::TimeDelta::FromMicroseconds(stats.rtt);
    network_stats.bandwidth_estimate = stats.estimated_bandwidth;
    http_server_properties_->SetServerNetworkStats(
        it->host_port_proxy_pair.first, network_stats);
  }
  if (!aliases.empty()) {
    const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                  aliases.begin()->is_https);
    ip_aliases_[ip_alias_key].erase(session);
    if (ip_aliases_[ip_alias_key].empty()) {
      ip_aliases_.erase(ip_alias_key);
    }
  }
  session_aliases_.erase(session);
}

void QuicStreamFactory::OnSessionClosed(QuicClientSession* session) {
  DCHECK_EQ(0u, session->GetNumOpenStreams());
  OnSessionGoingAway(session);
  all_sessions_.erase(session);
  delete session;
}

void QuicStreamFactory::CancelRequest(QuicStreamRequest* request) {
  DCHECK(ContainsKey(active_requests_, request));
  Job* job = active_requests_[request];
  job_requests_map_[job].erase(request);
  active_requests_.erase(request);
}

void QuicStreamFactory::CloseAllSessions(int error) {
  while (!active_sessions_.empty()) {
    size_t initial_size = active_sessions_.size();
    active_sessions_.begin()->second->CloseSessionOnError(error);
    DCHECK_NE(initial_size, active_sessions_.size());
  }
  while (!all_sessions_.empty()) {
    size_t initial_size = all_sessions_.size();
    (*all_sessions_.begin())->CloseSessionOnError(error);
    DCHECK_NE(initial_size, all_sessions_.size());
  }
  DCHECK(all_sessions_.empty());
}

base::Value* QuicStreamFactory::QuicStreamFactoryInfoToValue() const {
  base::ListValue* list = new base::ListValue();

  for (SessionMap::const_iterator it = active_sessions_.begin();
       it != active_sessions_.end(); ++it) {
    const SessionKey& session_key = it->first;
    QuicClientSession* session = it->second;
    const AliasSet& aliases = session_aliases_.find(session)->second;
    // Only add a session to the list once.
    if (session_key == *aliases.begin()) {
      std::set<HostPortProxyPair> hosts;
      for (AliasSet::const_iterator alias_it = aliases.begin();
           alias_it != aliases.end(); ++alias_it) {
        hosts.insert(alias_it->host_port_proxy_pair);
      }
      list->Append(session->GetInfoAsValue(hosts));
    }
  }
  return list;
}

void QuicStreamFactory::OnIPAddressChanged() {
  CloseAllSessions(ERR_NETWORK_CHANGED);
  require_confirmation_ = true;
}

void QuicStreamFactory::OnCertAdded(const X509Certificate* cert) {
  CloseAllSessions(ERR_CERT_DATABASE_CHANGED);
}

void QuicStreamFactory::OnCACertChanged(const X509Certificate* cert) {
  // We should flush the sessions if we removed trust from a
  // cert, because a previously trusted server may have become
  // untrusted.
  //
  // We should not flush the sessions if we added trust to a cert.
  //
  // Since the OnCACertChanged method doesn't tell us what
  // kind of change it is, we have to flush the socket
  // pools to be safe.
  CloseAllSessions(ERR_CERT_DATABASE_CHANGED);
}

bool QuicStreamFactory::HasActiveSession(const SessionKey& session_key) const {
  return ContainsKey(active_sessions_, session_key);
}

int QuicStreamFactory::CreateSession(
    const HostPortProxyPair& host_port_proxy_pair,
    bool is_https,
    CertVerifier* cert_verifier,
    const AddressList& address_list,
    const BoundNetLog& net_log,
    QuicClientSession** session) {
  bool enable_port_selection = enable_port_selection_;
  SessionKey session_key(host_port_proxy_pair, is_https);
  if (enable_port_selection &&
      ContainsKey(gone_away_aliases_, session_key)) {
    // Disable port selection when the server is going away.
    // There is no point in trying to return to the same server, if
    // that server is no longer handling requests.
    enable_port_selection = false;
    gone_away_aliases_.erase(session_key);
  }

  QuicConnectionId connection_id = random_generator_->RandUint64();
  IPEndPoint addr = *address_list.begin();
  scoped_refptr<PortSuggester> port_suggester =
      new PortSuggester(host_port_proxy_pair.first, port_seed_);
  DatagramSocket::BindType bind_type = enable_port_selection ?
      DatagramSocket::RANDOM_BIND :  // Use our callback.
      DatagramSocket::DEFAULT_BIND;  // Use OS to randomize.
  scoped_ptr<DatagramClientSocket> socket(
      client_socket_factory_->CreateDatagramClientSocket(
          bind_type,
          base::Bind(&PortSuggester::SuggestPort, port_suggester),
          net_log.net_log(), net_log.source()));
  int rv = socket->Connect(addr);
  if (rv != OK)
    return rv;
  UMA_HISTOGRAM_COUNTS("Net.QuicEphemeralPortsSuggested",
                       port_suggester->call_count());
  if (enable_port_selection) {
    DCHECK_LE(1u, port_suggester->call_count());
  } else {
    DCHECK_EQ(0u, port_suggester->call_count());
  }

  // We should adaptively set this buffer size, but for now, we'll use a size
  // that is more than large enough for a full receive window, and yet
  // does not consume "too much" memory.  If we see bursty packet loss, we may
  // revisit this setting and test for its impact.
  const int32 kSocketBufferSize(TcpReceiver::kReceiveWindowTCP);
  socket->SetReceiveBufferSize(kSocketBufferSize);
  // Set a buffer large enough to contain the initial CWND's worth of packet
  // to work around the problem with CHLO packets being sent out with the
  // wrong encryption level, when the send buffer is full.
  socket->SetSendBufferSize(kMaxPacketSize * 20); // Support 20 packets.

  scoped_ptr<QuicDefaultPacketWriter> writer(
      new QuicDefaultPacketWriter(socket.get()));

  if (!helper_.get()) {
    helper_.reset(new QuicConnectionHelper(
        base::MessageLoop::current()->message_loop_proxy().get(),
        clock_.get(), random_generator_));
  }

  QuicConnection* connection = new QuicConnection(connection_id, addr,
                                                  helper_.get(),
                                                  writer.get(), false,
                                                  supported_versions_);
  writer->SetConnection(connection);
  connection->options()->max_packet_length = max_packet_length_;

  QuicCryptoClientConfig* crypto_config = GetOrCreateCryptoConfig(session_key);

  DCHECK(crypto_config);

  QuicConfig config = config_;
  if (http_server_properties_) {
    const HttpServerProperties::NetworkStats* stats =
        http_server_properties_->GetServerNetworkStats(
            host_port_proxy_pair.first);
    if (stats != NULL) {
      config.set_initial_round_trip_time_us(stats->rtt.InMicroseconds(),
                                            stats->rtt.InMicroseconds());
    }
  }

  *session = new QuicClientSession(
      connection, socket.Pass(), writer.Pass(), this,
      quic_crypto_client_stream_factory_, host_port_proxy_pair.first.host(),
      config, crypto_config, net_log.net_log());
  all_sessions_.insert(*session);  // owning pointer
  if (is_https) {
    crypto_config->SetProofVerifier(
        new ProofVerifierChromium(cert_verifier, net_log));
  }
  return OK;
}

bool QuicStreamFactory::HasActiveJob(const SessionKey& key) const {
  return ContainsKey(active_jobs_, key);
}

void QuicStreamFactory::ActivateSession(
    const SessionKey& session_key,
    QuicClientSession* session) {
  DCHECK(!HasActiveSession(session_key));
  active_sessions_[session_key] = session;
  session_aliases_[session].insert(session_key);
  const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                session_key.is_https);
  DCHECK(!ContainsKey(ip_aliases_[ip_alias_key], session));
  ip_aliases_[ip_alias_key].insert(session);
}

QuicCryptoClientConfig* QuicStreamFactory::GetOrCreateCryptoConfig(
    const SessionKey& session_key) {
  QuicCryptoClientConfig* crypto_config;

  if (ContainsKey(all_crypto_configs_, session_key)) {
    crypto_config = all_crypto_configs_[session_key];
    DCHECK(crypto_config);
  } else {
    // TODO(rtenneti): if two quic_sessions for the same host_port_proxy_pair
    // share the same crypto_config, will it cause issues?
    crypto_config = new QuicCryptoClientConfig();
    if (quic_server_info_factory_) {
      QuicCryptoClientConfig::CachedState* cached =
          crypto_config->Create(session_key.host_port_proxy_pair.first.host(),
                                quic_server_info_factory_);
      DCHECK(cached);
    }
    crypto_config->SetDefaults();
    all_crypto_configs_[session_key] = crypto_config;
    PopulateFromCanonicalConfig(session_key, crypto_config);
  }
  return crypto_config;
}

void QuicStreamFactory::PopulateFromCanonicalConfig(
    const SessionKey& session_key,
    QuicCryptoClientConfig* crypto_config) {
  const string server_hostname = session_key.host_port_proxy_pair.first.host();
  const uint16 server_port = session_key.host_port_proxy_pair.first.port();
  unsigned i = 0;
  for (; i < canoncial_suffixes_.size(); ++i) {
    if (EndsWith(server_hostname, canoncial_suffixes_[i], false)) {
      break;
    }
  }
  if (i == canoncial_suffixes_.size())
    return;

  HostPortPair suffix_host_port(canoncial_suffixes_[i], server_port);
  HostPortProxyPair suffix_host_port_proxy_pair(
      suffix_host_port, session_key.host_port_proxy_pair.second);
  SessionKey suffix_session_key(suffix_host_port_proxy_pair,
                                session_key.is_https);
  if (!ContainsKey(canonical_hostname_to_origin_map_, suffix_session_key)) {
    // This is the first host we've seen which matches the suffix, so make it
    // canonical.
    canonical_hostname_to_origin_map_[suffix_session_key] = session_key;
    return;
  }

  const SessionKey& canonical_session_key =
      canonical_hostname_to_origin_map_[suffix_session_key];
  QuicCryptoClientConfig* canonical_crypto_config =
      all_crypto_configs_[canonical_session_key];
  DCHECK(canonical_crypto_config);
  const HostPortProxyPair& canonical_host_port_proxy_pair =
      canonical_session_key.host_port_proxy_pair;

  // Copy the CachedState for the canonical server from canonical_crypto_config
  // as the initial CachedState for the server_hostname in crypto_config.
  crypto_config->InitializeFrom(server_hostname,
                                canonical_host_port_proxy_pair.first.host(),
                                canonical_crypto_config);
  // Update canonical version to point at the "most recent" crypto_config.
  canonical_hostname_to_origin_map_[suffix_session_key] =
      canonical_session_key;
}

}  // namespace net
