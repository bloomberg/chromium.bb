// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_POOL_H_
#define NET_SPDY_SPDY_SESSION_POOL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_database.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class AddressList;
class BoundNetLog;
class ClientSocketHandle;
class HostResolver;
class HttpServerProperties;
class SpdySession;

// This is a very simple pool for open SpdySessions.
class NET_EXPORT SpdySessionPool
    : public NetworkChangeNotifier::IPAddressObserver,
      public SSLConfigService::Observer,
      public CertDatabase::Observer {
 public:
  typedef base::TimeTicks (*TimeFunc)(void);

  SpdySessionPool(HostResolver* host_resolver,
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
                  const std::string& trusted_spdy_proxy);
  virtual ~SpdySessionPool();

  // Returns the SPDY session for the given key, or NULL if there is
  // none.
  scoped_refptr<SpdySession> GetIfExists(
      const SpdySessionKey& spdy_session_key,
      const BoundNetLog& net_log);

  // Builds a SpdySession from an existing SSL socket.  There must not
  // already be a session for the given key.  Note that ownership of
  // |connection| is transferred from the caller to the SpdySession.
  //
  // |certificate_error_code| is used to indicate the certificate
  // error encountered when connecting the SSL socket.  OK means there
  // was no error.  For testing and when SPDY is configured to work
  // with non-secure sockets, setting is_secure to false allows Spdy
  // to connect with a pre-existing TCP socket.
  //
  // Returns OK on success, and the |spdy_session| will be provided.
  // Returns an error on failure, and |spdy_session| will be NULL.
  net::Error GetSpdySessionFromSocket(
      const SpdySessionKey& spdy_session_key,
      scoped_ptr<ClientSocketHandle> connection,
      const BoundNetLog& net_log,
      int certificate_error_code,
      scoped_refptr<SpdySession>* spdy_session,
      bool is_secure);

  // Close only the currently existing SpdySessions with |error|.
  // Let any new ones created while this method is running continue to
  // live.
  void CloseCurrentSessions(net::Error error);

  // Close only the currently existing SpdySessions that are idle.
  // Let any new ones created while this method is running continue to
  // live.
  void CloseCurrentIdleSessions();

  // Close all SpdySessions, including any new ones created in the process of
  // closing the current ones.
  void CloseAllSessions();

  // Look up the session for the given key and close it if found.
  void TryCloseSession(const SpdySessionKey& key,
                       net::Error error,
                       const std::string& description);

  // Removes a SpdySession from the SpdySessionPool. This should only be called
  // by SpdySession, because otherwise session->state_ is not set to CLOSED.
  void Remove(const scoped_refptr<SpdySession>& session);

  // Creates a Value summary of the state of the spdy session pool. The caller
  // responsible for deleting the returned value.
  base::Value* SpdySessionPoolInfoToValue() const;

  HttpServerProperties* http_server_properties() {
    return http_server_properties_;
  }

  // NetworkChangeNotifier::IPAddressObserver methods:

  // We flush all idle sessions and release references to the active ones so
  // they won't get re-used.  The active ones will either complete successfully
  // or error out due to the IP address change.
  virtual void OnIPAddressChanged() OVERRIDE;

  // SSLConfigService::Observer methods:

  // We perform the same flushing as described above when SSL settings change.
  virtual void OnSSLConfigChanged() OVERRIDE;

  // CertDatabase::Observer methods:
  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE;
  virtual void OnCertTrustChanged(const X509Certificate* cert) OVERRIDE;

 private:
  friend class SpdySessionPoolPeer;  // For testing.

  typedef std::map<SpdySessionKey, scoped_refptr<SpdySession> > SpdySessionsMap;
  typedef std::map<IPEndPoint, SpdySessionKey> SpdyAliasMap;

  // Looks up any aliases for the given key, which must not already
  // have a session, and returns the session for first matching one,
  // or NULL if there is none. If a matching session is found, it is
  // then inserted into |sessions_|.
  scoped_refptr<SpdySession> GetFromAlias(
      const SpdySessionKey& spdy_session_key,
      const BoundNetLog& net_log,
      bool record_histograms);

  // Returns a normalized version of the given key suitable for lookup
  // into |sessions_|.
  const SpdySessionKey& NormalizeListKey(
      const SpdySessionKey& spdy_session_key) const;

  // Add the given key/session mapping. There must not already be a
  // session for the given key.
  void AddSession(const SpdySessionKey& spdy_session_key,
                  const scoped_refptr<SpdySession>& spdy_session);

  // Returns an iterator into |sessions_| for the given key, which may
  // be equal to |sessions_.end()|.
  SpdySessionsMap::iterator FindSessionByKey(
      const SpdySessionKey& spdy_session_key);

  // Remove the session associated with |spdy_session_key|, which must
  // exist.
  void RemoveSession(const SpdySessionKey& spdy_session_key);

  // Does a DNS cache lookup for |spdy_session_key|, and returns
  // the |addresses| found.
  // Returns true if addresses found, false otherwise.
  bool LookupAddresses(const SpdySessionKey& spdy_session_key,
                       const BoundNetLog& net_log,
                       AddressList* addresses) const;

  // Remove all aliases for |spdy_session_key| from the aliases table.
  void RemoveAliases(const SpdySessionKey& spdy_session_key);

  HttpServerProperties* const http_server_properties_;

  // This is a map of session keys to sessions. A session may appear
  // more than once in this map if it has aliases.
  //
  // TODO(akalin): Have a map which owns the sessions and another one
  // for the aliased session cache.
  SpdySessionsMap sessions_;

  // A map of IPEndPoint aliases for sessions.
  SpdyAliasMap aliases_;

  static bool g_force_single_domain;

  const scoped_refptr<SSLConfigService> ssl_config_service_;
  HostResolver* const resolver_;

  // Defaults to true. May be controlled via SpdySessionPoolPeer for tests.
  bool verify_domain_authentication_;
  bool enable_sending_initial_settings_;
  bool force_single_domain_;
  bool enable_ip_pooling_;
  bool enable_credential_frames_;
  bool enable_compression_;
  bool enable_ping_based_connection_checking_;
  NextProto default_protocol_;
  size_t stream_initial_recv_window_size_;
  size_t initial_max_concurrent_streams_;
  size_t max_concurrent_streams_limit_;
  TimeFunc time_func_;

  // This SPDY proxy is allowed to push resources from origins that are
  // different from those of their associated streams.
  HostPortPair trusted_spdy_proxy_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPool);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_POOL_H_
