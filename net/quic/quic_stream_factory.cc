// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <set>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/socket/client_socket_factory.h"

namespace net {

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete.
class QuicStreamFactory::Job {
 public:
  Job(QuicStreamFactory* factory,
      HostResolver* host_resolver,
      const HostPortProxyPair& host_port_proxy_pair,
      bool is_https,
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

  const HostPortProxyPair& host_port_proxy_pair() const {
    return host_port_proxy_pair_;
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
  const HostPortProxyPair host_port_proxy_pair_;
  bool is_https_;
  CertVerifier* cert_verifier_;
  const BoundNetLog net_log_;
  QuicClientSession* session_;
  CompletionCallback callback_;
  AddressList address_list_;
  DISALLOW_COPY_AND_ASSIGN(Job);
};

QuicStreamFactory::Job::Job(
    QuicStreamFactory* factory,
    HostResolver* host_resolver,
    const HostPortProxyPair& host_port_proxy_pair,
    bool is_https,
    CertVerifier* cert_verifier,
    const BoundNetLog& net_log)
    : factory_(factory),
      host_resolver_(host_resolver),
      host_port_proxy_pair_(host_port_proxy_pair),
      is_https_(is_https),
      cert_verifier_(cert_verifier),
      net_log_(net_log) {
}

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
      HostResolver::RequestInfo(host_port_proxy_pair_.first), &address_list_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 base::Unretained(this)),
      net_log_);
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  if (rv != OK)
    return rv;

  // TODO(rch): remove this code!
  AddressList::iterator it = address_list_.begin();
  while (it != address_list_.end()) {
    if (it->GetFamily() == ADDRESS_FAMILY_IPV6) {
      it = address_list_.erase(it);
    } else {
      it++;
    }
  }

  DCHECK(!factory_->HasActiveSession(host_port_proxy_pair_));
  io_state_ = STATE_CONNECT;
  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory) {}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(
    const HostPortProxyPair& host_port_proxy_pair,
    bool is_https,
    CertVerifier* cert_verifier,
    const BoundNetLog& net_log,
    const CompletionCallback& callback) {
  DCHECK(!stream_);
  DCHECK(callback_.is_null());
  int rv = factory_->Create(host_port_proxy_pair, is_https, cert_verifier,
                            net_log, this);
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

  session_ = factory_->CreateSession(host_port_proxy_pair_, is_https_,
                                     cert_verifier_, address_list_, net_log_);
  session_->StartReading();
  int rv = session_->CryptoConnect(
      base::Bind(&QuicStreamFactory::Job::OnIOComplete,
                 base::Unretained(this)));
  return rv;
}

int QuicStreamFactory::Job::DoConnectComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(host_port_proxy_pair_));
  factory_->ActivateSession(host_port_proxy_pair_, session_);

  return OK;
}

QuicStreamFactory::QuicStreamFactory(
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
    QuicRandom* random_generator,
    QuicClock* clock)
    : host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      quic_crypto_client_stream_factory_(quic_crypto_client_stream_factory),
      random_generator_(random_generator),
      clock_(clock),
      weak_factory_(this) {
  config_.SetDefaults();
  config_.set_idle_connection_state_lifetime(
      QuicTime::Delta::FromSeconds(30),
      QuicTime::Delta::FromSeconds(30));
}

QuicStreamFactory::~QuicStreamFactory() {
  STLDeleteElements(&all_sessions_);
  STLDeleteValues(&active_jobs_);
  STLDeleteValues(&all_crypto_configs_);
}

int QuicStreamFactory::Create(const HostPortProxyPair& host_port_proxy_pair,
                              bool is_https,
                              CertVerifier* cert_verifier,
                              const BoundNetLog& net_log,
                              QuicStreamRequest* request) {
  if (HasActiveSession(host_port_proxy_pair)) {
    request->set_stream(CreateIfSessionExists(host_port_proxy_pair, net_log));
    return OK;
  }

  if (HasActiveJob(host_port_proxy_pair)) {
    Job* job = active_jobs_[host_port_proxy_pair];
    active_requests_[request] = job;
    job_requests_map_[job].insert(request);
    return ERR_IO_PENDING;
  }

  scoped_ptr<Job> job(new Job(this, host_resolver_, host_port_proxy_pair,
                              is_https, cert_verifier, net_log));
  int rv = job->Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job.get()));

  if (rv == ERR_IO_PENDING) {
    active_requests_[request] = job.get();
    job_requests_map_[job.get()].insert(request);
    active_jobs_[host_port_proxy_pair] = job.release();
  }
  if (rv == OK) {
    DCHECK(HasActiveSession(host_port_proxy_pair));
    request->set_stream(CreateIfSessionExists(host_port_proxy_pair, net_log));
  }
  return rv;
}

void QuicStreamFactory::OnJobComplete(Job* job, int rv) {
  if (rv == OK) {
    // Create all the streams, but do not notify them yet.
    for (RequestSet::iterator it = job_requests_map_[job].begin();
         it != job_requests_map_[job].end() ; ++it) {
      DCHECK(HasActiveSession(job->host_port_proxy_pair()));
      (*it)->set_stream(CreateIfSessionExists(job->host_port_proxy_pair(),
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
  active_jobs_.erase(job->host_port_proxy_pair());
  job_requests_map_.erase(job);
  delete job;
  return;
}

// Returns a newly created QuicHttpStream owned by the caller, if a
// matching session already exists.  Returns NULL otherwise.
scoped_ptr<QuicHttpStream> QuicStreamFactory::CreateIfSessionExists(
    const HostPortProxyPair& host_port_proxy_pair,
    const BoundNetLog& net_log) {
  if (!HasActiveSession(host_port_proxy_pair)) {
    DLOG(INFO) << "No active session";
    return scoped_ptr<QuicHttpStream>();
  }

  QuicClientSession* session = active_sessions_[host_port_proxy_pair];
  DCHECK(session);
  return scoped_ptr<QuicHttpStream>(new QuicHttpStream(session->GetWeakPtr()));
}

void QuicStreamFactory::OnIdleSession(QuicClientSession* session) {
}

void QuicStreamFactory::OnSessionClose(QuicClientSession* session) {
  DCHECK_EQ(0u, session->GetNumOpenStreams());
  const AliasSet& aliases = session_aliases_[session];
  for (AliasSet::const_iterator it = aliases.begin(); it != aliases.end();
       ++it) {
    DCHECK(active_sessions_.count(*it));
    DCHECK_EQ(session, active_sessions_[*it]);
    active_sessions_.erase(*it);
  }
  all_sessions_.erase(session);
  session_aliases_.erase(session);
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
    const HostPortProxyPair& pair = it->first;
    const QuicClientSession* session = it->second;

    list->Append(session->GetInfoAsValue(pair.first));
  }
  return list;
}

void QuicStreamFactory::OnIPAddressChanged() {
  CloseAllSessions(ERR_NETWORK_CHANGED);
}

bool QuicStreamFactory::HasActiveSession(
    const HostPortProxyPair& host_port_proxy_pair) {
  return ContainsKey(active_sessions_, host_port_proxy_pair);
}

QuicClientSession* QuicStreamFactory::CreateSession(
    const HostPortProxyPair& host_port_proxy_pair,
    bool is_https,
    CertVerifier* cert_verifier,
    const AddressList& address_list,
    const BoundNetLog& net_log) {
  QuicGuid guid = random_generator_->RandUint64();
  IPEndPoint addr = *address_list.begin();
  DatagramClientSocket* socket =
      client_socket_factory_->CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, base::Bind(&base::RandInt),
          net_log.net_log(), net_log.source());
  socket->Connect(addr);

  // We should adaptively set this buffer size, but for now, we'll use a size
  // that is more than large enough for a 100 packet congestion window, and yet
  // does not consume "too much" memory.  If we see bursty packet loss, we may
  // revisit this setting and test for its impact.
  const int32 kSocketBufferSize(kMaxPacketSize * 100);  // Support 100 packets.
  socket->SetReceiveBufferSize(kSocketBufferSize);
  // TODO(jar): What should the UDP send buffer be set to?  If the send buffer
  // is too large, then we might(?) wastefully queue packets in the OS, when
  // we'd rather construct packets just in time. We do however expect that the
  // calculated send rate (paced, or ack clocked), will be well below the egress
  // rate of the local machine, so that *shouldn't* be a problem.
  // If the buffer setting is too small, then we will starve our outgoing link
  // on a fast connection, because we won't respond fast enough to the many
  // async callbacks to get data from us.  On the other hand, until we have real
  // pacing support (beyond ack-clocked pacing), we get a bit of adhoc-pacing by
  // requiring the application to refill this OS buffer (ensuring that we don't
  // blast a pile of packets at the kernel's max egress rate).
  // socket->SetSendBufferSize(????);

  QuicConnectionHelper* helper = new QuicConnectionHelper(
      base::MessageLoop::current()->message_loop_proxy().get(),
      clock_.get(),
      random_generator_,
      socket);

  QuicConnection* connection = new QuicConnection(guid, addr, helper, false,
                                                  QuicVersionMax());

  QuicCryptoClientConfig* crypto_config =
      GetOrCreateCryptoConfig(host_port_proxy_pair);
  DCHECK(crypto_config);

  QuicClientSession* session =
      new QuicClientSession(connection, socket, this,
                            quic_crypto_client_stream_factory_,
                            host_port_proxy_pair.first.host(), config_,
                            crypto_config, net_log.net_log());
  all_sessions_.insert(session);  // owning pointer
  if (is_https) {
    crypto_config->SetProofVerifier(
        new ProofVerifierChromium(cert_verifier, net_log));
  }
  return session;
}

bool QuicStreamFactory::HasActiveJob(
    const HostPortProxyPair& host_port_proxy_pair) {
  return ContainsKey(active_jobs_, host_port_proxy_pair);
}

void QuicStreamFactory::ActivateSession(
    const HostPortProxyPair& host_port_proxy_pair,
    QuicClientSession* session) {
  DCHECK(!HasActiveSession(host_port_proxy_pair));
  active_sessions_[host_port_proxy_pair] = session;
  session_aliases_[session].insert(host_port_proxy_pair);
}

QuicCryptoClientConfig* QuicStreamFactory::GetOrCreateCryptoConfig(
    const HostPortProxyPair& host_port_proxy_pair) {
  QuicCryptoClientConfig* crypto_config;
  if (ContainsKey(all_crypto_configs_, host_port_proxy_pair)) {
    crypto_config = all_crypto_configs_[host_port_proxy_pair];
    DCHECK(crypto_config);
  } else {
    crypto_config = new QuicCryptoClientConfig();
    crypto_config->SetDefaults();
    all_crypto_configs_[host_port_proxy_pair] = crypto_config;
  }
  return crypto_config;
}

}  // namespace net
