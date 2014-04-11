// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <set>

#include "base/cpu.h"
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
#include "net/quic/quic_server_id.h"
#include "net/socket/client_socket_factory.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using std::string;
using std::vector;

namespace net {

namespace {

enum CreateSessionFailure {
  CREATION_ERROR_CONNECTING_SOCKET,
  CREATION_ERROR_SETTING_RECEIVE_BUFFER,
  CREATION_ERROR_SETTING_SEND_BUFFER,
  CREATION_ERROR_MAX
};

const uint64 kBrokenAlternateProtocolDelaySecs = 300;

// The initial receive window size for both streams and sessions.
const int32 kInitialReceiveWindowSize = 10 * 1024 * 1024;  // 10MB

void HistogramCreateSessionFailure(enum CreateSessionFailure error) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.CreationError", error,
                            CREATION_ERROR_MAX);
}

bool IsEcdsaSupported() {
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
#endif

  return true;
}

}  // namespace

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
      const HostPortPair& host_port_pair,
      bool is_https,
      PrivacyMode privacy_mode,
      base::StringPiece method,
      QuicServerInfo* server_info,
      const BoundNetLog& net_log);

  ~Job();

  int Run(const CompletionCallback& callback);

  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoLoadServerInfo();
  int DoLoadServerInfoComplete(int rv);
  int DoConnect();
  int DoConnectComplete(int rv);

  void OnIOComplete(int rv);

  CompletionCallback callback() {
    return callback_;
  }

  const QuicServerId server_id() const {
    return server_id_;
  }

 private:
  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_LOAD_SERVER_INFO,
    STATE_LOAD_SERVER_INFO_COMPLETE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
  };
  IoState io_state_;

  QuicStreamFactory* factory_;
  SingleRequestHostResolver host_resolver_;
  QuicServerId server_id_;
  bool is_post_;
  scoped_ptr<QuicServerInfo> server_info_;
  const BoundNetLog net_log_;
  QuicClientSession* session_;
  CompletionCallback callback_;
  AddressList address_list_;
  base::TimeTicks disk_cache_load_start_time_;
  base::WeakPtrFactory<Job> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Job);
};

QuicStreamFactory::Job::Job(QuicStreamFactory* factory,
                            HostResolver* host_resolver,
                            const HostPortPair& host_port_pair,
                            bool is_https,
                            PrivacyMode privacy_mode,
                            base::StringPiece method,
                            QuicServerInfo* server_info,
                            const BoundNetLog& net_log)
    : factory_(factory),
      host_resolver_(host_resolver),
      server_id_(host_port_pair, is_https, privacy_mode),
      is_post_(method == "POST"),
      server_info_(server_info),
      net_log_(net_log),
      session_(NULL),
      weak_factory_(this) {}

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
      case STATE_LOAD_SERVER_INFO:
        CHECK_EQ(OK, rv);
        rv = DoLoadServerInfo();
        break;
      case STATE_LOAD_SERVER_INFO_COMPLETE:
        rv = DoLoadServerInfoComplete(rv);
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
  // Start loading the data now, and wait for it after we resolve the host.
  if (server_info_) {
    disk_cache_load_start_time_ = base::TimeTicks::Now();
    server_info_->Start();
  }

  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  return host_resolver_.Resolve(
      HostResolver::RequestInfo(server_id_.host_port_pair()),
      DEFAULT_PRIORITY,
      &address_list_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 weak_factory_.GetWeakPtr()),
      net_log_);
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(server_id_));

  // Inform the factory of this resolution, which will set up
  // a session alias, if possible.
  if (factory_->OnResolution(server_id_, address_list_)) {
    return OK;
  }

  io_state_ = STATE_LOAD_SERVER_INFO;
  return OK;
}

int QuicStreamFactory::Job::DoLoadServerInfo() {
  io_state_ = STATE_LOAD_SERVER_INFO_COMPLETE;

  if (!server_info_)
    return OK;

  return server_info_->WaitForDataReady(
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 weak_factory_.GetWeakPtr()));
}

int QuicStreamFactory::Job::DoLoadServerInfoComplete(int rv) {
  if (server_info_) {
    UMA_HISTOGRAM_TIMES("Net.QuicServerInfo.DiskCacheReadTime",
                        base::TimeTicks::Now() - disk_cache_load_start_time_);
  }

  if (rv != OK) {
    server_info_.reset();
  }

  io_state_ = STATE_CONNECT;
  return OK;
}

int QuicStreamFactory::Job::DoConnect() {
  io_state_ = STATE_CONNECT_COMPLETE;

  int rv = factory_->CreateSession(server_id_, server_info_.Pass(),
                                   address_list_, net_log_, &session_);
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
      factory_->require_confirmation() || server_id_.is_https() || is_post_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 base::Unretained(this)));
  return rv;
}

int QuicStreamFactory::Job::DoConnectComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(server_id_));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  AddressList address(session_->connection()->peer_address());
  if (factory_->OnResolution(server_id_, address)) {
    session_->connection()->SendConnectionClose(QUIC_NO_ERROR);
    session_ = NULL;
    return OK;
  }

  factory_->ActivateSession(server_id_, session_);

  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory) {}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(const HostPortPair& host_port_pair,
                               bool is_https,
                               PrivacyMode privacy_mode,
                               base::StringPiece method,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) {
  DCHECK(!stream_);
  DCHECK(callback_.is_null());
  DCHECK(factory_);
  int rv = factory_->Create(host_port_pair, is_https, privacy_mode, method,
                            net_log, this);
  if (rv == ERR_IO_PENDING) {
    host_port_pair_ = host_port_pair;
    is_https_ = is_https;
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

QuicStreamFactory::QuicStreamFactory(
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    base::WeakPtr<HttpServerProperties> http_server_properties,
    CertVerifier* cert_verifier,
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
      cert_verifier_(cert_verifier),
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

  crypto_config_.SetDefaults();
  crypto_config_.AddCanonicalSuffix(".c.youtube.com");
  crypto_config_.AddCanonicalSuffix(".googlevideo.com");
  crypto_config_.SetProofVerifier(new ProofVerifierChromium(cert_verifier));
  base::CPU cpu;
  if (cpu.has_aesni() && cpu.has_avx())
    crypto_config_.PreferAesGcm();
  if (!IsEcdsaSupported())
    crypto_config_.DisableEcdsa();
}

QuicStreamFactory::~QuicStreamFactory() {
  CloseAllSessions(ERR_ABORTED);
  STLDeleteElements(&all_sessions_);
  STLDeleteValues(&active_jobs_);
}

int QuicStreamFactory::Create(const HostPortPair& host_port_pair,
                              bool is_https,
                              PrivacyMode privacy_mode,
                              base::StringPiece method,
                              const BoundNetLog& net_log,
                              QuicStreamRequest* request) {
  QuicServerId server_id(host_port_pair, is_https, privacy_mode);
  if (HasActiveSession(server_id)) {
    request->set_stream(CreateIfSessionExists(server_id, net_log));
    return OK;
  }

  if (HasActiveJob(server_id)) {
    Job* job = active_jobs_[server_id];
    active_requests_[request] = job;
    job_requests_map_[job].insert(request);
    return ERR_IO_PENDING;
  }

  QuicServerInfo* quic_server_info = NULL;
  if (quic_server_info_factory_) {
    QuicCryptoClientConfig::CachedState* cached =
        crypto_config_.LookupOrCreate(server_id);
    DCHECK(cached);
    if (cached->IsEmpty()) {
      quic_server_info = quic_server_info_factory_->GetForServer(server_id);
    }
  }
  scoped_ptr<Job> job(new Job(this, host_resolver_, host_port_pair, is_https,
                              privacy_mode, method, quic_server_info, net_log));
  int rv = job->Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job.get()));

  if (rv == ERR_IO_PENDING) {
    active_requests_[request] = job.get();
    job_requests_map_[job.get()].insert(request);
    active_jobs_[server_id] = job.release();
  }
  if (rv == OK) {
    DCHECK(HasActiveSession(server_id));
    request->set_stream(CreateIfSessionExists(server_id, net_log));
  }
  return rv;
}

bool QuicStreamFactory::OnResolution(
    const QuicServerId& server_id,
    const AddressList& address_list) {
  DCHECK(!HasActiveSession(server_id));
  for (size_t i = 0; i < address_list.size(); ++i) {
    const IPEndPoint& address = address_list[i];
    const IpAliasKey ip_alias_key(address, server_id.is_https());
    if (!ContainsKey(ip_aliases_, ip_alias_key))
      continue;

    const SessionSet& sessions = ip_aliases_[ip_alias_key];
    for (SessionSet::const_iterator i = sessions.begin();
         i != sessions.end(); ++i) {
      QuicClientSession* session = *i;
      if (!session->CanPool(server_id.host()))
        continue;
      active_sessions_[server_id] = session;
      session_aliases_[session].insert(server_id);
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
      DCHECK(HasActiveSession(job->server_id()));
      (*it)->set_stream(CreateIfSessionExists(job->server_id(),
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
  active_jobs_.erase(job->server_id());
  job_requests_map_.erase(job);
  delete job;
  return;
}

// Returns a newly created QuicHttpStream owned by the caller, if a
// matching session already exists.  Returns NULL otherwise.
scoped_ptr<QuicHttpStream> QuicStreamFactory::CreateIfSessionExists(
    const QuicServerId& server_id,
    const BoundNetLog& net_log) {
  if (!HasActiveSession(server_id)) {
    DVLOG(1) << "No active session";
    return scoped_ptr<QuicHttpStream>();
  }

  QuicClientSession* session = active_sessions_[server_id];
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
      HistogramBrokenAlternateProtocolLocation(
          BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY);
      http_server_properties_->SetBrokenAlternateProtocol(it->host_port_pair());
      UMA_HISTOGRAM_COUNTS("Net.QuicHandshakeNotConfirmedNumPacketsReceived",
                           stats.packets_received);
      int count = ++broken_alternate_protocol_map_[it->host_port_pair()];
      base::TimeDelta delay =
          base::TimeDelta::FromSeconds(kBrokenAlternateProtocolDelaySecs);
      BrokenAlternateProtocolEntry entry;
      entry.origin = it->host_port_pair();
      entry.when = base::TimeTicks::Now() + delay * (1 << (count - 1));
      broken_alternate_protocol_list_.push_back(entry);
      // If this is the only entry in the list, schedule an expiration task.
      // Otherwse it will be rescheduled automatically when the pending
      // task runs.
      if (broken_alternate_protocol_list_.size() == 1) {
        ScheduleBrokenAlternateProtocolMappingsExpiration();
      }
      continue;
    }

    broken_alternate_protocol_map_.erase(it->host_port_pair());
    HttpServerProperties::NetworkStats network_stats;
    network_stats.rtt = base::TimeDelta::FromMicroseconds(stats.rtt);
    network_stats.bandwidth_estimate = stats.estimated_bandwidth;
    http_server_properties_->SetServerNetworkStats(it->host_port_pair(),
                                                   network_stats);
  }
  if (!aliases.empty()) {
    const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                  aliases.begin()->is_https());
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
    const QuicServerId& server_id = it->first;
    QuicClientSession* session = it->second;
    const AliasSet& aliases = session_aliases_.find(session)->second;
    // Only add a session to the list once.
    if (server_id == *aliases.begin()) {
      std::set<HostPortPair> hosts;
      for (AliasSet::const_iterator alias_it = aliases.begin();
           alias_it != aliases.end(); ++alias_it) {
        hosts.insert(alias_it->host_port_pair());
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

bool QuicStreamFactory::HasActiveSession(
    const QuicServerId& server_id) const {
  return ContainsKey(active_sessions_, server_id);
}

int QuicStreamFactory::CreateSession(
    const QuicServerId& server_id,
    scoped_ptr<QuicServerInfo> server_info,
    const AddressList& address_list,
    const BoundNetLog& net_log,
    QuicClientSession** session) {
  bool enable_port_selection = enable_port_selection_;
  if (enable_port_selection &&
      ContainsKey(gone_away_aliases_, server_id)) {
    // Disable port selection when the server is going away.
    // There is no point in trying to return to the same server, if
    // that server is no longer handling requests.
    enable_port_selection = false;
    gone_away_aliases_.erase(server_id);
  }

  QuicConnectionId connection_id = random_generator_->RandUint64();
  IPEndPoint addr = *address_list.begin();
  scoped_refptr<PortSuggester> port_suggester =
      new PortSuggester(server_id.host_port_pair(), port_seed_);
  DatagramSocket::BindType bind_type = enable_port_selection ?
      DatagramSocket::RANDOM_BIND :  // Use our callback.
      DatagramSocket::DEFAULT_BIND;  // Use OS to randomize.
  scoped_ptr<DatagramClientSocket> socket(
      client_socket_factory_->CreateDatagramClientSocket(
          bind_type,
          base::Bind(&PortSuggester::SuggestPort, port_suggester),
          net_log.net_log(), net_log.source()));
  int rv = socket->Connect(addr);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_CONNECTING_SOCKET);
    return rv;
  }
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
  rv = socket->SetReceiveBufferSize(kSocketBufferSize);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_RECEIVE_BUFFER);
    return rv;
  }
  // Set a buffer large enough to contain the initial CWND's worth of packet
  // to work around the problem with CHLO packets being sent out with the
  // wrong encryption level, when the send buffer is full.
  rv = socket->SetSendBufferSize(kMaxPacketSize * 20);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_SEND_BUFFER);
    return rv;
  }

  scoped_ptr<QuicDefaultPacketWriter> writer(
      new QuicDefaultPacketWriter(socket.get()));

  if (!helper_.get()) {
    helper_.reset(new QuicConnectionHelper(
        base::MessageLoop::current()->message_loop_proxy().get(),
        clock_.get(), random_generator_));
  }

  QuicConnection* connection =
      new QuicConnection(connection_id, addr, helper_.get(), writer.get(),
                         false, supported_versions_, kInitialReceiveWindowSize);
  writer->SetConnection(connection);
  connection->options()->max_packet_length = max_packet_length_;

  InitializeCachedState(server_id, server_info);

  QuicConfig config = config_;
  if (http_server_properties_) {
    const HttpServerProperties::NetworkStats* stats =
        http_server_properties_->GetServerNetworkStats(
            server_id.host_port_pair());
    if (stats != NULL) {
      config.set_initial_round_trip_time_us(stats->rtt.InMicroseconds(),
                                            stats->rtt.InMicroseconds());
    }
  }

  *session = new QuicClientSession(
      connection, socket.Pass(), writer.Pass(), this,
      quic_crypto_client_stream_factory_, server_info.Pass(), server_id,
      config, &crypto_config_, net_log.net_log());
  all_sessions_.insert(*session);  // owning pointer
  return OK;
}

bool QuicStreamFactory::HasActiveJob(const QuicServerId& key) const {
  return ContainsKey(active_jobs_, key);
}

void QuicStreamFactory::ActivateSession(
    const QuicServerId& server_id,
    QuicClientSession* session) {
  DCHECK(!HasActiveSession(server_id));
  active_sessions_[server_id] = session;
  session_aliases_[session].insert(server_id);
  const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                server_id.is_https());
  DCHECK(!ContainsKey(ip_aliases_[ip_alias_key], session));
  ip_aliases_[ip_alias_key].insert(session);
}

void QuicStreamFactory::InitializeCachedState(
    const QuicServerId& server_id,
    const scoped_ptr<QuicServerInfo>& server_info) {
  if (!server_info)
    return;

  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_.LookupOrCreate(server_id);
  if (!cached->IsEmpty())
    return;

  if (!cached->Initialize(server_info->state().server_config,
                          server_info->state().source_address_token,
                          server_info->state().certs,
                          server_info->state().server_config_sig,
                          clock_->WallNow()))
    return;

  if (!server_id.is_https()) {
    // Don't check the certificates for insecure QUIC.
    cached->SetProofValid();
  }
}

void QuicStreamFactory::ExpireBrokenAlternateProtocolMappings() {
  base::TimeTicks now = base::TimeTicks::Now();
  while (!broken_alternate_protocol_list_.empty()) {
    BrokenAlternateProtocolEntry entry =
        broken_alternate_protocol_list_.front();
    if (now < entry.when) {
      break;
    }

    http_server_properties_->ClearAlternateProtocol(entry.origin);
    broken_alternate_protocol_list_.pop_front();
  }
  ScheduleBrokenAlternateProtocolMappingsExpiration();
}

void QuicStreamFactory::ScheduleBrokenAlternateProtocolMappingsExpiration() {
  if (broken_alternate_protocol_list_.empty()) {
    return;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks when = broken_alternate_protocol_list_.front().when;
  base::TimeDelta delay = when > now ? when - now : base::TimeDelta();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicStreamFactory::ExpireBrokenAlternateProtocolMappings,
                 weak_factory_.GetWeakPtr()),
      delay);
}

}  // namespace net
