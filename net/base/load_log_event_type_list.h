// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly into load_log.h. DO NOT include this file anywhere else.

// --------------------------------------------------------------------------
// General pseudo-events
// --------------------------------------------------------------------------

// If a log is terminated by a kLogTruncated event, then it grew too large
// and any subsequent events logged to it were discarded.
EVENT_TYPE(LOG_TRUNCATED)

// Something got cancelled (we determine what is cancelled based on the
// log context around it.)
EVENT_TYPE(CANCELLED)

// ------------------------------------------------------------------------
// HostResolverImpl
// ------------------------------------------------------------------------

// The start/end of a host resolve (DNS) request.
EVENT_TYPE(HOST_RESOLVER_IMPL)

// The start/end of HostResolver::Observer::OnStartResolution.
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONSTART)

// The start/end of HostResolver::Observer::OnFinishResolutionWithStatus
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONFINISH)

// The start/end of HostResolver::Observer::OnCancelResolution.
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONCANCEL)

// ------------------------------------------------------------------------
// InitProxyResolver
// ------------------------------------------------------------------------

// The start/end of auto-detect + custom PAC URL configuration.
EVENT_TYPE(INIT_PROXY_RESOLVER)

// The start/end of download of a PAC script. This could be the well-known
// WPAD URL (if testing auto-detect), or a custom PAC URL.
EVENT_TYPE(INIT_PROXY_RESOLVER_FETCH_PAC_SCRIPT)

// The start/end of the testing of a PAC script (trying to parse the fetched
// file as javascript).
EVENT_TYPE(INIT_PROXY_RESOLVER_SET_PAC_SCRIPT)

// ------------------------------------------------------------------------
// ProxyService
// ------------------------------------------------------------------------

// The start/end of a proxy resolve request.
EVENT_TYPE(PROXY_SERVICE)

// The time while a request is waiting on InitProxyResolver to configure
// against either WPAD or custom PAC URL. The specifics on this time
// are found from ProxyService::init_proxy_resolver_log().
EVENT_TYPE(PROXY_SERVICE_WAITING_FOR_INIT_PAC)

// ------------------------------------------------------------------------
// ProxyResolverV8
// ------------------------------------------------------------------------

// Measures the time taken to execute the "myIpAddress()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_MY_IP_ADDRESS)

// Measures the time taken to execute the "myIpAddressEx()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_MY_IP_ADDRESS_EX)

// Measures the time taken to execute the "dnsResolve()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_DNS_RESOLVE)

// Measures the time taken to execute the "dnsResolveEx()" javascript binding.
EVENT_TYPE(PROXY_RESOLVER_V8_DNS_RESOLVE_EX)

// ------------------------------------------------------------------------
// ClientSocketPoolBase::ConnectJob
// ------------------------------------------------------------------------

// The start/end of a ConnectJob.
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB)

// Whether the connect job timed out.
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB_TIMED_OUT)

// ------------------------------------------------------------------------
// ClientSocketPoolBaseHelper
// ------------------------------------------------------------------------

// The start/end of a client socket pool request for a socket.
EVENT_TYPE(SOCKET_POOL)

// The start/end of when a request is sitting in the queue waiting for
// a connect job to finish. (Only applies to late_binding).
EVENT_TYPE(SOCKET_POOL_WAITING_IN_QUEUE)

// The request stalled because there are too many sockets in the pool.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS)

// The request stalled because there are too many sockets in the group.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP)

// ------------------------------------------------------------------------
// URLRequest
// ------------------------------------------------------------------------

// Measures the time between URLRequest::Start() and
// URLRequest::ResponseStarted().
EVENT_TYPE(URL_REQUEST_START)

// ------------------------------------------------------------------------
// HttpCache
// ------------------------------------------------------------------------

// Measures the time while opening a disk cache entry.
EVENT_TYPE(HTTP_CACHE_OPEN_ENTRY)

// Measures the time while creating a disk cache entry.
EVENT_TYPE(HTTP_CACHE_CREATE_ENTRY)

// Measures the time while reading the response info from a disk cache entry.
EVENT_TYPE(HTTP_CACHE_READ_INFO)

// Measures the time that an HttpCache::Transaction is stalled waiting for
// the cache entry to become available (for example if we are waiting for
// exclusive access to an existing entry).
EVENT_TYPE(HTTP_CACHE_WAITING)

