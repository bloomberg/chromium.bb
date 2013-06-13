// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include "base/callback.h"
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

}

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

SpdySessionPool::SpdySessionPool(
    HostResolver* resolver,
    SSLConfigService* ssl_config_service,
    HttpServerProperties* http_server_properties,
    size_t max_sessions_per_domain,
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
      max_sessions_per_domain_(max_sessions_per_domain == 0 ?
                               kMaxSessionsPerDomain :
                               max_sessions_per_domain),
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

scoped_refptr<SpdySession> SpdySessionPool::Get(
    const SpdySessionKey& spdy_session_key,
    const BoundNetLog& net_log) {
  return GetInternal(spdy_session_key, net_log, false);
}

scoped_refptr<SpdySession> SpdySessionPool::GetIfExists(
    const SpdySessionKey& spdy_session_key,
    const BoundNetLog& net_log) {
  return GetInternal(spdy_session_key, net_log, true);
}

scoped_refptr<SpdySession> SpdySessionPool::GetInternal(
    const SpdySessionKey& spdy_session_key,
    const BoundNetLog& net_log,
    bool only_use_existing_sessions) {
  scoped_refptr<SpdySession> spdy_session;
  SpdySessionList* list = GetSessionList(spdy_session_key);
  if (!list) {
    // Check if we have a Session through a domain alias.
    spdy_session = GetFromAlias(spdy_session_key, net_log, true);
    if (spdy_session.get()) {
      UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                                FOUND_EXISTING_FROM_IP_POOL,
                                SPDY_SESSION_GET_MAX);
      net_log.AddEvent(
          NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
          spdy_session->net_log().source().ToEventParametersCallback());
      // Add this session to the map so that we can find it next time.
      list = AddSessionList(spdy_session_key);
      list->push_back(spdy_session);
      spdy_session->AddPooledAlias(spdy_session_key);
      return spdy_session;
    } else if (only_use_existing_sessions) {
      return NULL;
    }
    list = AddSessionList(spdy_session_key);
  }

  DCHECK(list);
  if (list->size() && list->size() == max_sessions_per_domain_) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                              FOUND_EXISTING,
                              SPDY_SESSION_GET_MAX);
    spdy_session = GetExistingSession(list, net_log);
    net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_FOUND_EXISTING_SESSION,
      spdy_session->net_log().source().ToEventParametersCallback());
    return spdy_session;
  }

  DCHECK(!only_use_existing_sessions);

  spdy_session = new SpdySession(spdy_session_key, this,
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
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet",
                            CREATED_NEW,
                            SPDY_SESSION_GET_MAX);
  list->push_back(spdy_session);
  net_log.AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_CREATED_NEW_SESSION,
      spdy_session->net_log().source().ToEventParametersCallback());
  DCHECK_LE(list->size(), max_sessions_per_domain_);
  return spdy_session;
}

net::Error SpdySessionPool::GetSpdySessionFromSocket(
    const SpdySessionKey& spdy_session_key,
    ClientSocketHandle* connection,
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
  SpdySessionList* list = GetSessionList(spdy_session_key);
  if (!list)
    list = AddSessionList(spdy_session_key);
  DCHECK(list->empty());
  list->push_back(*spdy_session);

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
      AddAlias(address, spdy_session_key);
  }

  // Now we can initialize the session with the SSL socket.
  return (*spdy_session)->InitializeWithSocket(connection, is_secure,
                                               certificate_error_code);
}

bool SpdySessionPool::HasSession(
    const SpdySessionKey& spdy_session_key) const {
  if (GetSessionList(spdy_session_key))
    return true;

  // Check if we have a session via an alias.
  scoped_refptr<SpdySession> spdy_session =
      GetFromAlias(spdy_session_key, BoundNetLog(), false);
  return spdy_session.get() != NULL;
}

void SpdySessionPool::Remove(const scoped_refptr<SpdySession>& session) {
  bool ok = RemoveFromSessionList(session, session->spdy_session_key());
  DCHECK(ok);
  session->net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_POOL_REMOVE_SESSION,
      session->net_log().source().ToEventParametersCallback());

  const std::set<SpdySessionKey>& aliases = session->pooled_aliases();
  for (std::set<SpdySessionKey>::const_iterator it = aliases.begin();
       it != aliases.end(); ++it) {
    ok = RemoveFromSessionList(session, *it);
    DCHECK(ok);
  }
}

bool SpdySessionPool::RemoveFromSessionList(
    const scoped_refptr<SpdySession>& session,
    const SpdySessionKey& spdy_session_key) {
  SpdySessionList* list = GetSessionList(spdy_session_key);
  if (!list)
    return false;
  list->remove(session);
  if (list->empty())
    RemoveSessionList(spdy_session_key);
  return true;
}

base::Value* SpdySessionPool::SpdySessionPoolInfoToValue() const {
  base::ListValue* list = new base::ListValue();

  for (SpdySessionsMap::const_iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    SpdySessionList* sessions = it->second;
    for (SpdySessionList::const_iterator session = sessions->begin();
         session != sessions->end(); ++session) {
      // Only add the session if the key in the map matches the main
      // host_port_proxy_pair (not an alias).
      const SpdySessionKey& key = it->first;
      const SpdySessionKey& session_key = session->get()->spdy_session_key();
      if (key.Equals(session_key))
        list->Append(session->get()->GetInfoAsValue());
    }
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

scoped_refptr<SpdySession> SpdySessionPool::GetExistingSession(
    SpdySessionList* list,
    const BoundNetLog& net_log) const {
  DCHECK(list);
  DCHECK_LT(0u, list->size());
  scoped_refptr<SpdySession> spdy_session = list->front();
  if (list->size() > 1) {
    list->pop_front();  // Rotate the list.
    list->push_back(spdy_session);
  }

  return spdy_session;
}

scoped_refptr<SpdySession> SpdySessionPool::GetFromAlias(
      const SpdySessionKey& spdy_session_key,
      const BoundNetLog& net_log,
      bool record_histograms) const {
  // We should only be checking aliases when there is no direct session.
  DCHECK(!GetSessionList(spdy_session_key));

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

    SpdySessionList* list = GetSessionList(alias_key);
    if (!list) {
      NOTREACHED();  // It shouldn't be in the aliases table if we can't get it!
      continue;
    }

    scoped_refptr<SpdySession> spdy_session = GetExistingSession(list, net_log);
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

SpdySessionPool::SpdySessionList*
    SpdySessionPool::AddSessionList(
        const SpdySessionKey& spdy_session_key) {
  const SpdySessionKey& key = NormalizeListKey(spdy_session_key);
  DCHECK(sessions_.find(key) == sessions_.end());
  SpdySessionPool::SpdySessionList* list = new SpdySessionList();
  sessions_[spdy_session_key] = list;
  return list;
}

SpdySessionPool::SpdySessionList*
    SpdySessionPool::GetSessionList(
        const SpdySessionKey& spdy_session_key) const {
  const SpdySessionKey& key = NormalizeListKey(spdy_session_key);
  SpdySessionsMap::const_iterator it = sessions_.find(key);
  if (it != sessions_.end())
    return it->second;
  return NULL;
}

void SpdySessionPool::RemoveSessionList(
    const SpdySessionKey& spdy_session_key) {
  const SpdySessionKey& key = NormalizeListKey(spdy_session_key);
  SpdySessionList* list = GetSessionList(key);
  if (list) {
    delete list;
    sessions_.erase(key);
  } else {
    DCHECK(false) << "removing orphaned session list";
  }
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

void SpdySessionPool::AddAlias(const IPEndPoint& endpoint,
                               const SpdySessionKey& spdy_session_key) {
  DCHECK(enable_ip_pooling_);
  aliases_[endpoint] = spdy_session_key;
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

void SpdySessionPool::CloseAllSessions() {
  while (!sessions_.empty()) {
    SpdySessionList* list = sessions_.begin()->second;
    CHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    CHECK(session.get());
    // This call takes care of removing the session from the pool, as well as
    // removing the session list if the list is empty.
    session->CloseSessionOnError(
        net::ERR_ABORTED, true, "Closing all sessions.");
  }
}

void SpdySessionPool::CloseCurrentSessions(net::Error error) {
  SpdySessionsMap old_map;
  old_map.swap(sessions_);
  for (SpdySessionsMap::const_iterator it = old_map.begin();
       it != old_map.end(); ++it) {
    SpdySessionList* list = it->second;
    CHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    CHECK(session.get());
    session->set_spdy_session_pool(NULL);
  }

  while (!old_map.empty()) {
    SpdySessionList* list = old_map.begin()->second;
    CHECK(list);
    const scoped_refptr<SpdySession>& session = list->front();
    CHECK(session.get());
    session->CloseSessionOnError(error, false, "Closing current sessions.");
    list->pop_front();
    if (list->empty()) {
      delete list;
      RemoveAliases(old_map.begin()->first);
      old_map.erase(old_map.begin()->first);
    }
  }
  DCHECK(sessions_.empty());
  DCHECK(aliases_.empty());
}

void SpdySessionPool::CloseIdleSessions() {
  SpdySessionsMap::const_iterator map_it = sessions_.begin();
  while (map_it != sessions_.end()) {
    SpdySessionList* list = map_it->second;
    CHECK(list);

    // Assumes there is only 1 element in the list.
    SpdySessionList::iterator session_it = list->begin();
    const scoped_refptr<SpdySession>& session = *session_it;
    CHECK(session.get());
    if (session->is_active()) {
      ++map_it;
      continue;
    }

    SpdySessionKey key(map_it->first);
    session->CloseSessionOnError(
        net::ERR_ABORTED, true, "Closing idle sessions.");
    // CloseSessionOnError can invalidate the iterator.
    map_it = sessions_.lower_bound(key);
  }
}

}  // namespace net
