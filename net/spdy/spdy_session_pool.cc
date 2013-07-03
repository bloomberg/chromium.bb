// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/spdy/spdy_session.h"


namespace net {

namespace {

enum SpdySessionGetTypes {
  CREATED_NEW                 = 0,
  FOUND_EXISTING              = 1,
  FOUND_EXISTING_FROM_IP_POOL = 2,
  IMPORTED_FROM_SOCKET        = 3,
  SPDY_SESSION_GET_MAX        = 4
};

}  // namespace

SpdySessionPool::SpdySessionPool(
    HostResolver* resolver,
    SSLConfigService* ssl_config_service,
    HttpServerProperties* http_server_properties,
    bool force_single_domain,
    bool enable_ip_pooling,
    bool enable_credential_frames,
    bool enable_compression,
    bool enable_ping_based_connection_checking,
    NextProto default_protocol,
    size_t stream_initial_recv_window_size,
    size_t initial_max_concurrent_streams,
    size_t max_concurrent_streams_limit,
    SpdySessionPool::TimeFunc time_func,
    const std::string& trusted_spdy_proxy)
    : http_server_properties_(http_server_properties),
      ssl_config_service_(ssl_config_service),
      resolver_(resolver),
      verify_domain_authentication_(true),
      enable_sending_initial_settings_(true),
      force_single_domain_(force_single_domain),
      enable_ip_pooling_(enable_ip_pooling),
      enable_credential_frames_(enable_credential_frames),
      enable_compression_(enable_compression),
      enable_ping_based_connection_checking_(
          enable_ping_based_connection_checking),
      default_protocol_(default_protocol),
      stream_initial_recv_window_size_(stream_initial_recv_window_size),
      initial_max_concurrent_streams_(initial_max_concurrent_streams),
      max_concurrent_streams_limit_(max_concurrent_streams_limit),
      time_func_(time_func),
      trusted_spdy_proxy_(
          HostPortPair::FromString(trusted_spdy_proxy)) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  if (ssl_config_service_.get())
    ssl_config_service_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
  CloseAllSessions();

  if (ssl_config_service_.get())
    ssl_config_service_->RemoveObserver(this);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

scoped_refptr<SpdySession> SpdySessionPool::GetIfExists(
    const SpdySessionKey& spdy_session_key,
    const BoundNetLog& net_log) {
  SpdySessionsMap::iterator it = FindSessionByKey(spdy_session_key);
  if (it != sessions_.end()) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                              FOUND_EXISTING,
                              SPDY_SESSION_GET_MAX);
    net_log.AddEvent(
        NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION,
        it->second->net_log().source().ToEventParametersCallback());
    return it->second;
  }

  // Check if we have a Session through a domain alias.
  scoped_refptr<SpdySession> spdy_session =
      GetFromAlias(spdy_session_key, net_log, true);
  if (spdy_session) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                              FOUND_EXISTING_FROM_IP_POOL,
                              SPDY_SESSION_GET_MAX);
    net_log.AddEvent(
        NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
        spdy_session->net_log().source().ToEventParametersCallback());
    // Add this session to the map so that we can find it next time.
    AddSession(spdy_session_key, spdy_session);
    spdy_session->AddPooledAlias(spdy_session_key);
    return spdy_session;
  }

  return scoped_refptr<SpdySession>();
}

net::Error SpdySessionPool::GetSpdySessionFromSocket(
    const SpdySessionKey& spdy_session_key,
    scoped_ptr<ClientSocketHandle> connection,
    const BoundNetLog& net_log,
    int certificate_error_code,
    scoped_refptr<SpdySession>* spdy_session,
    bool is_secure) {
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                            IMPORTED_FROM_SOCKET,
                            SPDY_SESSION_GET_MAX);
  // Create the SPDY session and add it to the pool.
  *spdy_session = new SpdySession(spdy_session_key, this,
                                  http_server_properties_,
                                  verify_domain_authentication_,
                                  enable_sending_initial_settings_,
                                  enable_credential_frames_,
                                  enable_compression_,
                                  enable_ping_based_connection_checking_,
                                  default_protocol_,
                                  stream_initial_recv_window_size_,
                                  initial_max_concurrent_streams_,
                                  max_concurrent_streams_limit_,
                                  time_func_,
                                  trusted_spdy_proxy_,
                                  net_log.net_log());
  AddSession(spdy_session_key, *spdy_session);

  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET,
      (*spdy_session)->net_log().source().ToEventParametersCallback());

  // We have a new session.  Lookup the IP address for this session so that we
  // can match future Sessions (potentially to different domains) which can
  // potentially be pooled with this one. Because GetPeerAddress() reports the
  // proxy's address instead of the origin server, check to see if this is a
  // direct connection.
  if (enable_ip_pooling_  &&
      spdy_session_key.proxy_server().is_direct()) {
    IPEndPoint address;
    if (connection->socket()->GetPeerAddress(&address) == OK)
      aliases_[address] = spdy_session_key;
  }

  // Now we can initialize the session with the SSL socket.
  return (*spdy_session)->InitializeWithSocket(connection.Pass(), is_secure,
                                               certificate_error_code);
}

// Make a copy of |sessions_| in the Close* functions below to avoid
// reentrancy problems. Due to aliases, it doesn't suffice to simply
// increment the iterator before closing.

void SpdySessionPool::CloseCurrentSessions(net::Error error) {
  SpdySessionsMap sessions_copy = sessions_;
  for (SpdySessionsMap::const_iterator it = sessions_copy.begin();
       it != sessions_copy.end(); ++it) {
    TryCloseSession(it->first, error, "Closing current sessions.");
  }
}

void SpdySessionPool::CloseCurrentIdleSessions() {
  SpdySessionsMap sessions_copy = sessions_;
  for (SpdySessionsMap::const_iterator it = sessions_copy.begin();
       it != sessions_copy.end(); ++it) {
    if (!it->second->is_active())
      TryCloseSession(it->first, ERR_ABORTED, "Closing idle sessions.");
  }
}

void SpdySessionPool::CloseAllSessions() {
  while (!sessions_.empty()) {
    SpdySessionsMap sessions_copy = sessions_;
    for (SpdySessionsMap::const_iterator it = sessions_copy.begin();
         it != sessions_copy.end(); ++it) {
      TryCloseSession(it->first, ERR_ABORTED, "Closing all sessions.");
    }
  }
}

void SpdySessionPool::TryCloseSession(const SpdySessionKey& key,
                                      net::Error error,
                                      const std::string& description) {
  SpdySessionsMap::const_iterator it = sessions_.find(key);
  if (it == sessions_.end())
    return;
  scoped_refptr<SpdySession> session = it->second;
  session->CloseSessionOnError(error, description);
  if (DCHECK_IS_ON()) {
    it = sessions_.find(key);
    // A new session with the same key may have been added, but it
    // must not be the one we just closed.
    if (it != sessions_.end())
      DCHECK_NE(it->second, session);
  }
}

void SpdySessionPool::Remove(const scoped_refptr<SpdySession>& session) {
  RemoveSession(session->spdy_session_key());
  session->net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_REMOVE_SESSION,
      session->net_log().source().ToEventParametersCallback());

  const std::set<SpdySessionKey>& aliases = session->pooled_aliases();
  for (std::set<SpdySessionKey>::const_iterator it = aliases.begin();
       it != aliases.end(); ++it) {
    RemoveSession(*it);
  }
}

base::Value* SpdySessionPool::SpdySessionPoolInfoToValue() const {
  base::ListValue* list = new base::ListValue();

  for (SpdySessionsMap::const_iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    // Only add the session if the key in the map matches the main
    // host_port_proxy_pair (not an alias).
    const SpdySessionKey& key = it->first;
    const SpdySessionKey& session_key = it->second->spdy_session_key();
    if (key.Equals(session_key))
      list->Append(it->second->GetInfoAsValue());
  }
  return list;
}

void SpdySessionPool::OnIPAddressChanged() {
  CloseCurrentSessions(ERR_NETWORK_CHANGED);
  http_server_properties_->ClearAllSpdySettings();
}

void SpdySessionPool::OnSSLConfigChanged() {
  CloseCurrentSessions(ERR_NETWORK_CHANGED);
}

scoped_refptr<SpdySession> SpdySessionPool::GetFromAlias(
      const SpdySessionKey& spdy_session_key,
      const BoundNetLog& net_log,
      bool record_histograms) {
  // We should only be checking aliases when there is no direct session.
  DCHECK(FindSessionByKey(spdy_session_key) == sessions_.end());

  if (!enable_ip_pooling_)
    return NULL;

  AddressList addresses;
  if (!LookupAddresses(spdy_session_key, net_log, &addresses))
    return NULL;
  for (AddressList::const_iterator iter = addresses.begin();
       iter != addresses.end();
       ++iter) {
    SpdyAliasMap::const_iterator alias_iter = aliases_.find(*iter);
    if (alias_iter == aliases_.end())
      continue;

    // We found an alias.
    const SpdySessionKey& alias_key = alias_iter->second;

    // If the proxy and privacy settings match, we can reuse this session.
    if (!(alias_key.proxy_server() == spdy_session_key.proxy_server()) ||
        !(alias_key.privacy_mode() ==
            spdy_session_key.privacy_mode()))
      continue;

    SpdySessionsMap::iterator it = FindSessionByKey(alias_key);
    if (it == sessions_.end()) {
      NOTREACHED();  // It shouldn't be in the aliases table if we can't get it!
      continue;
    }

    scoped_refptr<SpdySession> spdy_session = it->second;
    // If the SPDY session is a secure one, we need to verify that the server
    // is authenticated to serve traffic for |host_port_proxy_pair| too.
    if (!spdy_session->VerifyDomainAuthentication(
         spdy_session_key.host_port_pair().host())) {
      if (record_histograms)
        UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 0, 2);
      continue;
    }
    if (record_histograms)
      UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 1, 2);
    return spdy_session;
  }
  return NULL;
}

void SpdySessionPool::OnCertAdded(const X509Certificate* cert) {
  CloseCurrentSessions(ERR_NETWORK_CHANGED);
}

void SpdySessionPool::OnCertTrustChanged(const X509Certificate* cert) {
  // Per wtc, we actually only need to CloseCurrentSessions when trust is
  // reduced. CloseCurrentSessions now because OnCertTrustChanged does not
  // tell us this.
  // See comments in ClientSocketPoolManager::OnCertTrustChanged.
  CloseCurrentSessions(ERR_NETWORK_CHANGED);
}

const SpdySessionKey& SpdySessionPool::NormalizeListKey(
    const SpdySessionKey& spdy_session_key) const {
  if (!force_single_domain_)
    return spdy_session_key;

  static SpdySessionKey* single_domain_key = NULL;
  if (!single_domain_key) {
    HostPortPair single_domain = HostPortPair("singledomain.com", 80);
    single_domain_key = new SpdySessionKey(single_domain,
                                           ProxyServer::Direct(),
                                           kPrivacyModeDisabled);
  }
  return *single_domain_key;
}

void SpdySessionPool::AddSession(
    const SpdySessionKey& spdy_session_key,
    const scoped_refptr<SpdySession>& spdy_session) {
  const SpdySessionKey& key = NormalizeListKey(spdy_session_key);
  std::pair<SpdySessionsMap::iterator, bool> result =
      sessions_.insert(std::make_pair(key, spdy_session));
  CHECK(result.second);
}

SpdySessionPool::SpdySessionsMap::iterator
SpdySessionPool::FindSessionByKey(const SpdySessionKey& spdy_session_key) {
  const SpdySessionKey& key = NormalizeListKey(spdy_session_key);
  return sessions_.find(key);
}

void SpdySessionPool::RemoveSession(const SpdySessionKey& spdy_session_key) {
  SpdySessionsMap::iterator it = FindSessionByKey(spdy_session_key);
  CHECK(it != sessions_.end());
  sessions_.erase(it);
  RemoveAliases(spdy_session_key);
}

bool SpdySessionPool::LookupAddresses(const SpdySessionKey& spdy_session_key,
                                      const BoundNetLog& net_log,
                                      AddressList* addresses) const {
  net::HostResolver::RequestInfo resolve_info(
      spdy_session_key.host_port_pair());
  int rv = resolver_->ResolveFromCache(resolve_info, addresses, net_log);
  DCHECK_NE(ERR_IO_PENDING, rv);
  return rv == OK;
}

void SpdySessionPool::RemoveAliases(const SpdySessionKey& spdy_session_key) {
  // Walk the aliases map, find references to this pair.
  // TODO(mbelshe):  Figure out if this is too expensive.
  SpdyAliasMap::iterator alias_it = aliases_.begin();
  while (alias_it != aliases_.end()) {
    if (alias_it->second.Equals(spdy_session_key)) {
      aliases_.erase(alias_it);
      alias_it = aliases_.begin();  // Iterator was invalidated.
      continue;
    }
    ++alias_it;
  }
}

}  // namespace net
