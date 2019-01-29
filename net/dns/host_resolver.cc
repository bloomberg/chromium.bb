// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_impl.h"

namespace net {

namespace {

// Maximum of 6 concurrent resolver threads (excluding retries).
// Some routers (or resolvers) appear to start to provide host-not-found if
// too many simultaneous resolutions are pending.  This number needs to be
// further optimized, but 8 is what FF currently does. We found some routers
// that limit this to 6, so we're temporarily holding it at that level.
const size_t kDefaultMaxProcTasks = 6u;

// Wraps a ResolveHostRequest to implement Request objects from the legacy
// Resolve() API. The wrapped request must not yet have been started.
//
// TODO(crbug.com/922699): Delete this class once all usage has been
// converted to the new CreateRequest() API.
class LegacyRequestImpl : public HostResolver::Request {
 public:
  LegacyRequestImpl(
      std::unique_ptr<HostResolver::ResolveHostRequest> inner_request,
      bool is_speculative)
      : inner_request_(std::move(inner_request)),
        is_speculative_(is_speculative) {}

  ~LegacyRequestImpl() override {}

  void ChangeRequestPriority(RequestPriority priority) override {
    inner_request_->ChangeRequestPriority(priority);
  }

  int Start() {
    return inner_request_->Start(base::BindOnce(
        &LegacyRequestImpl::LegacyApiCallback, base::Unretained(this)));
  }

  // Do not call to assign the callback until we are running an async job (after
  // Start() returns ERR_IO_PENDING) and before completion.  Until then, the
  // legacy HostResolverImpl::Resolve() needs to hang onto |callback| to ensure
  // it stays alive for the duration of the method call, as some callers may be
  // binding objects, eg the AddressList, with the callback.
  void AssignCallback(CompletionOnceCallback callback,
                      AddressList* addresses_result_ptr) {
    DCHECK(callback);
    DCHECK(addresses_result_ptr);

    callback_ = std::move(callback);
    addresses_result_ptr_ = addresses_result_ptr;
  }

  const HostResolver::ResolveHostRequest& inner_request() const {
    return *inner_request_;
  }

 private:
  // Result callback to bridge results handled entirely via ResolveHostRequest
  // to legacy API styles where AddressList was a separate method out parameter.
  void LegacyApiCallback(int error) {
    // Must call AssignCallback() before async results.
    DCHECK(callback_);

    if (error == OK && !is_speculative_) {
      // Legacy API does not allow non-address results (eg TXT), so AddressList
      // is always expected to be present on OK.
      DCHECK(inner_request_->GetAddressResults());
      *addresses_result_ptr_ = inner_request_->GetAddressResults().value();
    }
    addresses_result_ptr_ = nullptr;
    std::move(callback_).Run(error);
  }

  const std::unique_ptr<HostResolver::ResolveHostRequest> inner_request_;
  const bool is_speculative_;

  CompletionOnceCallback callback_;
  // This is a caller-provided pointer and should not be used once |callback_|
  // is invoked.
  AddressList* addresses_result_ptr_;

  DISALLOW_COPY_AND_ASSIGN(LegacyRequestImpl);
};

}  // namespace

PrioritizedDispatcher::Limits HostResolver::Options::GetDispatcherLimits()
    const {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, max_concurrent_resolves);

  // If not using default, do not use the field trial.
  if (limits.total_jobs != HostResolver::kDefaultParallelism)
    return limits;

  // Default, without trial is no reserved slots.
  limits.total_jobs = kDefaultMaxProcTasks;

  // Parallelism is determined by the field trial.
  std::string group =
      base::FieldTrialList::FindFullName("HostResolverDispatch");

  if (group.empty())
    return limits;

  // The format of the group name is a list of non-negative integers separated
  // by ':'. Each of the elements in the list corresponds to an element in
  // |reserved_slots|, except the last one which is the |total_jobs|.
  std::vector<base::StringPiece> group_parts = base::SplitStringPiece(
      group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (group_parts.size() != NUM_PRIORITIES + 1) {
    NOTREACHED();
    return limits;
  }

  std::vector<size_t> parsed(group_parts.size());
  size_t total_reserved_slots = 0;

  for (size_t i = 0; i < group_parts.size(); ++i) {
    if (!base::StringToSizeT(group_parts[i], &parsed[i])) {
      NOTREACHED();
      return limits;
    }
  }

  size_t total_jobs = parsed.back();
  parsed.pop_back();
  for (size_t i = 0; i < parsed.size(); ++i) {
    total_reserved_slots += parsed[i];
  }

  // There must be some unreserved slots available for the all priorities.
  if (total_reserved_slots > total_jobs ||
      (total_reserved_slots == total_jobs && parsed[MINIMUM_PRIORITY] == 0)) {
    NOTREACHED();
    return limits;
  }

  limits.total_jobs = total_jobs;
  limits.reserved_slots = parsed;
  return limits;
}

HostResolver::Options::Options()
    : max_concurrent_resolves(kDefaultParallelism),
      max_retry_attempts(kDefaultRetryAttempts),
      enable_caching(true) {}

std::unique_ptr<HostResolver> HostResolver::Factory::CreateResolver(
    const Options& options,
    NetLog* net_log) {
  return CreateSystemResolver(options, net_log);
}

HostResolver::RequestInfo::RequestInfo(const HostPortPair& host_port_pair)
    : RequestInfo() {
  host_port_pair_ = host_port_pair;
}

HostResolver::RequestInfo::RequestInfo(const RequestInfo& request_info) =
    default;

HostResolver::RequestInfo::~RequestInfo() = default;

HostResolver::RequestInfo::RequestInfo()
    : address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      host_resolver_flags_(0),
      allow_cached_response_(true),
      is_speculative_(false),
      is_my_ip_address_(false) {}

HostResolver::~HostResolver() = default;

std::unique_ptr<HostResolver::MdnsListener> HostResolver::CreateMdnsListener(
    const HostPortPair& host,
    DnsQueryType query_type) {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
  return nullptr;
}

void HostResolver::SetDnsClientEnabled(bool enabled) {}

HostCache* HostResolver::GetHostCache() {
  return nullptr;
}

std::unique_ptr<base::Value> HostResolver::GetDnsConfigAsValue() const {
  return nullptr;
}

void HostResolver::SetNoIPv6OnWifi(bool no_ipv6_on_wifi) {
  NOTREACHED();
}

bool HostResolver::GetNoIPv6OnWifi() {
  return false;
}

void HostResolver::SetDnsConfigOverrides(const DnsConfigOverrides& overrides) {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
}

const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
HostResolver::GetDnsOverHttpsServersForTesting() const {
  return nullptr;
}

// static
std::unique_ptr<HostResolver> HostResolver::CreateSystemResolver(
    const Options& options,
    NetLog* net_log) {
  return std::unique_ptr<HostResolver>(
      CreateSystemResolverImpl(options, net_log).release());
}

// static
std::unique_ptr<HostResolverImpl> HostResolver::CreateSystemResolverImpl(
    const Options& options,
    NetLog* net_log) {
  return std::make_unique<HostResolverImpl>(options, net_log);
}

// static
std::unique_ptr<HostResolver> HostResolver::CreateDefaultResolver(
    NetLog* net_log) {
  return CreateSystemResolver(Options(), net_log);
}

// static
std::unique_ptr<HostResolverImpl> HostResolver::CreateDefaultResolverImpl(
    NetLog* net_log) {
  return CreateSystemResolverImpl(Options(), net_log);
}

// static
AddressFamily HostResolver::DnsQueryTypeToAddressFamily(
    DnsQueryType dns_query_type) {
  switch (dns_query_type) {
    case DnsQueryType::UNSPECIFIED:
      return ADDRESS_FAMILY_UNSPECIFIED;
    case DnsQueryType::A:
      return ADDRESS_FAMILY_IPV4;
    case DnsQueryType::AAAA:
      return ADDRESS_FAMILY_IPV6;
    default:
      // |dns_query_type| should be an address type (A or AAAA) or UNSPECIFIED.
      NOTREACHED();
      return ADDRESS_FAMILY_UNSPECIFIED;
  }
}

// static
HostResolver::ResolveHostParameters
HostResolver::RequestInfoToResolveHostParameters(
    const HostResolver::RequestInfo& request_info,
    RequestPriority priority) {
  // Flag that should only be set internally, not used in input.
  DCHECK(!(request_info.host_resolver_flags() &
           HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6));

  ResolveHostParameters parameters;

  parameters.dns_query_type =
      AddressFamilyToDnsQueryType(request_info.address_family());
  parameters.initial_priority = priority;
  parameters.source = FlagsToSource(request_info.host_resolver_flags());
  parameters.cache_usage = request_info.allow_cached_response()
                               ? ResolveHostParameters::CacheUsage::ALLOWED
                               : ResolveHostParameters::CacheUsage::DISALLOWED;
  parameters.include_canonical_name =
      request_info.host_resolver_flags() & HOST_RESOLVER_CANONNAME;
  parameters.loopback_only =
      request_info.host_resolver_flags() & HOST_RESOLVER_LOOPBACK_ONLY;
  parameters.is_speculative = request_info.is_speculative();

  return parameters;
}

// static
HostResolverSource HostResolver::FlagsToSource(HostResolverFlags flags) {
  // To counter the lack of CNAME support in the async host resolver, SYSTEM is
  // forced when CANONNAME flags is present. This restriction can be removed
  // once CNAME support is added to the async resolver.  See
  // https://crbug.com/872665
  //
  // It is intentional that the presence of either flag forces SYSTEM.
  if (flags & (HOST_RESOLVER_SYSTEM_ONLY | HOST_RESOLVER_CANONNAME))
    return HostResolverSource::SYSTEM;

  return HostResolverSource::ANY;
}

// static
HostResolverFlags HostResolver::ParametersToHostResolverFlags(
    const ResolveHostParameters& parameters) {
  HostResolverFlags flags = 0;
  if (parameters.source == HostResolverSource::SYSTEM)
    flags |= HOST_RESOLVER_SYSTEM_ONLY;
  if (parameters.include_canonical_name)
    flags |= HOST_RESOLVER_CANONNAME;
  if (parameters.loopback_only)
    flags |= HOST_RESOLVER_LOOPBACK_ONLY;
  return flags;
}

// static
int HostResolver::LegacyResolve(
    std::unique_ptr<ResolveHostRequest> inner_request,
    bool is_speculative,
    AddressList* addresses,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* out_req) {
  auto wrapped_request = std::make_unique<LegacyRequestImpl>(
      std::move(inner_request), is_speculative);

  int rv = wrapped_request->Start();

  if (rv == OK && !is_speculative) {
    DCHECK(addresses);
    DCHECK(wrapped_request->inner_request().GetAddressResults());
    *addresses = wrapped_request->inner_request().GetAddressResults().value();
  } else if (rv == ERR_IO_PENDING) {
    DCHECK(addresses);
    DCHECK(callback);
    DCHECK(out_req);
    wrapped_request->AssignCallback(std::move(callback), addresses);
    *out_req = std::move(wrapped_request);
  }

  return rv;
}

HostResolver::HostResolver() = default;

}  // namespace net
