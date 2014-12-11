// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_util.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/sdch_manager.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/socket/ssl_client_socket.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net {

namespace {

// This should be incremented when significant changes are made that will
// invalidate the old loading code.
const int kLogFormatVersion = 1;

struct StringToConstant {
  const char* name;
  const int constant;
};

const StringToConstant kCertStatusFlags[] = {
#define CERT_STATUS_FLAG(label, value) { #label, value },
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG
};

const StringToConstant kLoadFlags[] = {
#define LOAD_FLAG(label, value) { #label, value },
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG
};

const StringToConstant kLoadStateTable[] = {
#define LOAD_STATE(label) { # label, net::LOAD_STATE_ ## label },
#include "net/base/load_states_list.h"
#undef LOAD_STATE
};

const short kNetErrors[] = {
#define NET_ERROR(label, value) value,
#include "net/base/net_error_list.h"
#undef NET_ERROR
};

const StringToConstant kSdchProblems[] = {
#define SDCH_PROBLEM_CODE(label, value) \
  { #label, value }                     \
  ,
#include "net/base/sdch_problem_code_list.h"
#undef SDCH_PROBLEM_CODE
};

const char* NetInfoSourceToString(NetInfoSource source) {
  switch (source) {
    #define NET_INFO_SOURCE(label, string, value) \
    case NET_INFO_ ## label: \
      return string;
#include "net/base/net_info_source_list.h"
    #undef NET_INFO_SOURCE
    case NET_INFO_ALL_SOURCES:
      return "All";
  }
  return "?";
}

// Returns the disk cache backend for |context| if there is one, or NULL.
// Despite the name, can return an in memory "disk cache".
disk_cache::Backend* GetDiskCacheBackend(net::URLRequestContext* context) {
  if (!context->http_transaction_factory())
    return NULL;

  net::HttpCache* http_cache = context->http_transaction_factory()->GetCache();
  if (!http_cache)
    return NULL;

  return http_cache->GetCurrentBackend();
}

// Returns true if |request1| was created before |request2|.
bool RequestCreatedBefore(const net::URLRequest* request1,
                          const net::URLRequest* request2) {
  if (request1->creation_time() < request2->creation_time())
    return true;
  if (request1->creation_time() > request2->creation_time())
    return false;
  // If requests were created at the same time, sort by ID.  Mostly matters for
  // testing purposes.
  return request1->identifier() < request2->identifier();
}

// Returns a Value representing the state of a pre-existing URLRequest when
// net-internals was opened.
base::Value* GetRequestStateAsValue(const net::URLRequest* request,
                                    net::NetLog::LogLevel log_level) {
  return request->GetStateAsValue();
}

}  // namespace

scoped_ptr<base::DictionaryValue> GetNetConstants() {
  scoped_ptr<base::DictionaryValue> constants_dict(new base::DictionaryValue());

  // Version of the file format.
  constants_dict->SetInteger("logFormatVersion", kLogFormatVersion);

  // Add a dictionary with information on the relationship between event type
  // enums and their symbolic names.
  constants_dict->Set("logEventTypes", net::NetLog::GetEventTypesAsValue());

  // Add a dictionary with information about the relationship between CertStatus
  // flags and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (size_t i = 0; i < arraysize(kCertStatusFlags); i++)
      dict->SetInteger(kCertStatusFlags[i].name, kCertStatusFlags[i].constant);

    constants_dict->Set("certStatusFlag", dict);
  }

  // Add a dictionary with information about the relationship between load flag
  // enums and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (size_t i = 0; i < arraysize(kLoadFlags); i++)
      dict->SetInteger(kLoadFlags[i].name, kLoadFlags[i].constant);

    constants_dict->Set("loadFlag", dict);
  }

  // Add a dictionary with information about the relationship between load state
  // enums and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (size_t i = 0; i < arraysize(kLoadStateTable); i++)
      dict->SetInteger(kLoadStateTable[i].name, kLoadStateTable[i].constant);

    constants_dict->Set("loadState", dict);
  }

  {
    base::DictionaryValue* dict = new base::DictionaryValue();
    #define NET_INFO_SOURCE(label, string, value) \
        dict->SetInteger(string, NET_INFO_ ## label);
#include "net/base/net_info_source_list.h"
    #undef NET_INFO_SOURCE
    constants_dict->Set("netInfoSources", dict);
  }

  // Add information on the relationship between net error codes and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (size_t i = 0; i < arraysize(kNetErrors); i++)
      dict->SetInteger(ErrorToShortString(kNetErrors[i]), kNetErrors[i]);

    constants_dict->Set("netError", dict);
  }

  // Add information on the relationship between QUIC error codes and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (net::QuicErrorCode error = net::QUIC_NO_ERROR;
         error < net::QUIC_LAST_ERROR;
         error = static_cast<net::QuicErrorCode>(error + 1)) {
      dict->SetInteger(net::QuicUtils::ErrorToString(error),
                       static_cast<int>(error));
    }

    constants_dict->Set("quicError", dict);
  }

  // Add information on the relationship between QUIC RST_STREAM error codes
  // and their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (net::QuicRstStreamErrorCode error = net::QUIC_STREAM_NO_ERROR;
         error < net::QUIC_STREAM_LAST_ERROR;
         error = static_cast<net::QuicRstStreamErrorCode>(error + 1)) {
      dict->SetInteger(net::QuicUtils::StreamErrorToString(error),
                       static_cast<int>(error));
    }

    constants_dict->Set("quicRstStreamError", dict);
  }

  // Add information on the relationship between SDCH problem codes and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    for (size_t i = 0; i < arraysize(kSdchProblems); i++)
      dict->SetInteger(kSdchProblems[i].name, kSdchProblems[i].constant);

    constants_dict->Set("sdchProblemCode", dict);
  }

  // Information about the relationship between event phase enums and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    dict->SetInteger("PHASE_BEGIN", net::NetLog::PHASE_BEGIN);
    dict->SetInteger("PHASE_END", net::NetLog::PHASE_END);
    dict->SetInteger("PHASE_NONE", net::NetLog::PHASE_NONE);

    constants_dict->Set("logEventPhase", dict);
  }

  // Information about the relationship between source type enums and
  // their symbolic names.
  constants_dict->Set("logSourceType", net::NetLog::GetSourceTypesAsValue());

  // Information about the relationship between LogLevel enums and their
  // symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    dict->SetInteger("LOG_ALL", net::NetLog::LOG_ALL);
    dict->SetInteger("LOG_ALL_BUT_BYTES", net::NetLog::LOG_ALL_BUT_BYTES);
    dict->SetInteger("LOG_STRIP_PRIVATE_DATA",
                     net::NetLog::LOG_STRIP_PRIVATE_DATA);

    constants_dict->Set("logLevelType", dict);
  }

  // Information about the relationship between address family enums and
  // their symbolic names.
  {
    base::DictionaryValue* dict = new base::DictionaryValue();

    dict->SetInteger("ADDRESS_FAMILY_UNSPECIFIED",
                     net::ADDRESS_FAMILY_UNSPECIFIED);
    dict->SetInteger("ADDRESS_FAMILY_IPV4",
                     net::ADDRESS_FAMILY_IPV4);
    dict->SetInteger("ADDRESS_FAMILY_IPV6",
                     net::ADDRESS_FAMILY_IPV6);

    constants_dict->Set("addressFamily", dict);
  }

  // Information about how the "time ticks" values we have given it relate to
  // actual system times. (We used time ticks throughout since they are stable
  // across system clock changes).
  {
    int64 cur_time_ms = (base::Time::Now() - base::Time()).InMilliseconds();

    int64 cur_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();

    // If we add this number to a time tick value, it gives the timestamp.
    int64 tick_to_time_ms = cur_time_ms - cur_time_ticks_ms;

    // Chrome on all platforms stores times using the Windows epoch
    // (Jan 1 1601), but the javascript wants a unix epoch.
    // TODO(eroman): Getting the timestamp relative to the unix epoch should
    //               be part of the time library.
    const int64 kUnixEpochMs = 11644473600000LL;
    int64 tick_to_unix_time_ms = tick_to_time_ms - kUnixEpochMs;

    // Pass it as a string, since it may be too large to fit in an integer.
    constants_dict->SetString("timeTickOffset",
                              base::Int64ToString(tick_to_unix_time_ms));
  }

  // "clientInfo" key is required for some NetLogLogger log readers.
  // Provide a default empty value for compatibility.
  constants_dict->Set("clientInfo", new base::DictionaryValue());

  // Add a list of active field experiments.
  {
    base::FieldTrial::ActiveGroups active_groups;
    base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
    base::ListValue* field_trial_groups = new base::ListValue();
    for (base::FieldTrial::ActiveGroups::const_iterator it =
             active_groups.begin();
         it != active_groups.end(); ++it) {
      field_trial_groups->AppendString(it->trial_name + ":" +
                                       it->group_name);
    }
    constants_dict->Set("activeFieldTrialGroups", field_trial_groups);
  }

  return constants_dict.Pass();
}

NET_EXPORT scoped_ptr<base::DictionaryValue> GetNetInfo(
    URLRequestContext* context, int info_sources) {
  // May only be called on the context's thread.
  DCHECK(context->CalledOnValidThread());

  scoped_ptr<base::DictionaryValue> net_info_dict(new base::DictionaryValue());

  // TODO(mmenke):  The code for most of these sources should probably be moved
  // into the sources themselves.
  if (info_sources & NET_INFO_PROXY_SETTINGS) {
    net::ProxyService* proxy_service = context->proxy_service();

    base::DictionaryValue* dict = new base::DictionaryValue();
    if (proxy_service->fetched_config().is_valid())
      dict->Set("original", proxy_service->fetched_config().ToValue());
    if (proxy_service->config().is_valid())
      dict->Set("effective", proxy_service->config().ToValue());

    net_info_dict->Set(NetInfoSourceToString(NET_INFO_PROXY_SETTINGS), dict);
  }

  if (info_sources & NET_INFO_BAD_PROXIES) {
    const net::ProxyRetryInfoMap& bad_proxies_map =
        context->proxy_service()->proxy_retry_info();

    base::ListValue* list = new base::ListValue();

    for (net::ProxyRetryInfoMap::const_iterator it = bad_proxies_map.begin();
         it != bad_proxies_map.end(); ++it) {
      const std::string& proxy_uri = it->first;
      const net::ProxyRetryInfo& retry_info = it->second;

      base::DictionaryValue* dict = new base::DictionaryValue();
      dict->SetString("proxy_uri", proxy_uri);
      dict->SetString("bad_until",
                      net::NetLog::TickCountToString(retry_info.bad_until));

      list->Append(dict);
    }

    net_info_dict->Set(NetInfoSourceToString(NET_INFO_BAD_PROXIES), list);
  }

  if (info_sources & NET_INFO_HOST_RESOLVER) {
    net::HostResolver* host_resolver = context->host_resolver();
    DCHECK(host_resolver);
    net::HostCache* cache = host_resolver->GetHostCache();
    if (cache) {
      base::DictionaryValue* dict = new base::DictionaryValue();
      base::Value* dns_config = host_resolver->GetDnsConfigAsValue();
      if (dns_config)
        dict->Set("dns_config", dns_config);

      dict->SetInteger(
          "default_address_family",
          static_cast<int>(host_resolver->GetDefaultAddressFamily()));

      base::DictionaryValue* cache_info_dict = new base::DictionaryValue();

      cache_info_dict->SetInteger(
          "capacity",
          static_cast<int>(cache->max_entries()));

      base::ListValue* entry_list = new base::ListValue();

      net::HostCache::EntryMap::Iterator it(cache->entries());
      for (; it.HasNext(); it.Advance()) {
        const net::HostCache::Key& key = it.key();
        const net::HostCache::Entry& entry = it.value();

        base::DictionaryValue* entry_dict = new base::DictionaryValue();

        entry_dict->SetString("hostname", key.hostname);
        entry_dict->SetInteger("address_family",
            static_cast<int>(key.address_family));
        entry_dict->SetString("expiration",
                              net::NetLog::TickCountToString(it.expiration()));

        if (entry.error != net::OK) {
          entry_dict->SetInteger("error", entry.error);
        } else {
          // Append all of the resolved addresses.
          base::ListValue* address_list = new base::ListValue();
          for (size_t i = 0; i < entry.addrlist.size(); ++i) {
            address_list->AppendString(entry.addrlist[i].ToStringWithoutPort());
          }
          entry_dict->Set("addresses", address_list);
        }

        entry_list->Append(entry_dict);
      }

      cache_info_dict->Set("entries", entry_list);
      dict->Set("cache", cache_info_dict);
      net_info_dict->Set(NetInfoSourceToString(NET_INFO_HOST_RESOLVER), dict);
    }
  }

  net::HttpNetworkSession* http_network_session =
      context->http_transaction_factory()->GetSession();

  if (info_sources & NET_INFO_SOCKET_POOL) {
    net_info_dict->Set(NetInfoSourceToString(NET_INFO_SOCKET_POOL),
                        http_network_session->SocketPoolInfoToValue());
  }

  if (info_sources & NET_INFO_SPDY_SESSIONS) {
    net_info_dict->Set(NetInfoSourceToString(NET_INFO_SPDY_SESSIONS),
                        http_network_session->SpdySessionPoolInfoToValue());
  }

  if (info_sources & NET_INFO_SPDY_STATUS) {
    base::DictionaryValue* status_dict = new base::DictionaryValue();

    status_dict->SetBoolean("spdy_enabled",
                            net::HttpStreamFactory::spdy_enabled());
    status_dict->SetBoolean(
        "use_alternate_protocols",
        http_network_session->params().use_alternate_protocols);
    status_dict->SetBoolean(
        "force_spdy_over_ssl",
        http_network_session->params().force_spdy_over_ssl);
    status_dict->SetBoolean(
        "force_spdy_always",
        http_network_session->params().force_spdy_always);

    NextProtoVector next_protos;
    http_network_session->GetNextProtos(&next_protos);
    if (!next_protos.empty()) {
      std::string next_protos_string;
      for (const NextProto proto : next_protos) {
        if (!next_protos_string.empty())
          next_protos_string.append(",");
        next_protos_string.append(SSLClientSocket::NextProtoToString(proto));
      }
      status_dict->SetString("next_protos", next_protos_string);
    }

    net_info_dict->Set(NetInfoSourceToString(NET_INFO_SPDY_STATUS),
                        status_dict);
  }

  if (info_sources & NET_INFO_SPDY_ALT_PROTO_MAPPINGS) {
    base::ListValue* dict_list = new base::ListValue();

    const net::HttpServerProperties& http_server_properties =
        *context->http_server_properties();

    const net::AlternateProtocolMap& map =
        http_server_properties.alternate_protocol_map();

    for (net::AlternateProtocolMap::const_iterator it = map.begin();
          it != map.end(); ++it) {
      base::DictionaryValue* dict = new base::DictionaryValue();
      dict->SetString("host_port_pair", it->first.ToString());
      dict->SetString("alternate_protocol", it->second.ToString());
      dict_list->Append(dict);
    }

    net_info_dict->Set(NetInfoSourceToString(NET_INFO_SPDY_ALT_PROTO_MAPPINGS),
                       dict_list);
  }

  if (info_sources & NET_INFO_QUIC) {
    net_info_dict->Set(NetInfoSourceToString(NET_INFO_QUIC),
                        http_network_session->QuicInfoToValue());
  }

  if (info_sources & NET_INFO_HTTP_CACHE) {
    base::DictionaryValue* info_dict = new base::DictionaryValue();
    base::DictionaryValue* stats_dict = new base::DictionaryValue();

    disk_cache::Backend* disk_cache = GetDiskCacheBackend(context);

    if (disk_cache) {
      // Extract the statistics key/value pairs from the backend.
      base::StringPairs stats;
      disk_cache->GetStats(&stats);
      for (size_t i = 0; i < stats.size(); ++i) {
        stats_dict->SetStringWithoutPathExpansion(
            stats[i].first, stats[i].second);
      }
    }
    info_dict->Set("stats", stats_dict);

    net_info_dict->Set(NetInfoSourceToString(NET_INFO_HTTP_CACHE),
                       info_dict);
  }

  if (info_sources & NET_INFO_SDCH) {
    base::Value* info_dict;
    SdchManager* sdch_manager = context->sdch_manager();
    if (sdch_manager) {
      info_dict = sdch_manager->SdchInfoToValue();
    } else {
      info_dict = new base::DictionaryValue();
    }
    net_info_dict->Set(NetInfoSourceToString(NET_INFO_SDCH), info_dict);
  }

  return net_info_dict.Pass();
}

NET_EXPORT void CreateNetLogEntriesForActiveObjects(
    const std::set<URLRequestContext*>& contexts,
    NetLog::ThreadSafeObserver* observer) {
  // Not safe to call this when the observer is watching a NetLog.
  DCHECK(!observer->net_log());

  // Put together the list of all requests.
  std::vector<const URLRequest*> requests;
  for (const auto& context : contexts) {
    // May only be called on the context's thread.
    DCHECK(context->CalledOnValidThread());
    // Contexts should all be using the same NetLog.
    DCHECK_EQ((*contexts.begin())->net_log(), context->net_log());
    for (const auto& request : *context->url_requests()) {
      requests.push_back(request);
    }
  }

  // Sort by creation time.
  std::sort(requests.begin(), requests.end(), RequestCreatedBefore);

  // Create fake events.
  ScopedVector<NetLog::Entry> entries;
  for (const auto& request : requests) {
    net::NetLog::ParametersCallback callback =
        base::Bind(&GetRequestStateAsValue, base::Unretained(request));

    net::NetLog::EntryData entry_data(net::NetLog::TYPE_REQUEST_ALIVE,
                                      request->net_log().source(),
                                      net::NetLog::PHASE_BEGIN,
                                      request->creation_time(),
                                      &callback);
    NetLog::Entry entry(&entry_data, request->net_log().GetLogLevel());
    observer->OnAddEntry(entry);
  }
}

}  // namespace net
