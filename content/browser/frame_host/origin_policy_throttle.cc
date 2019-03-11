// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"
#include "content/public/browser/origin_policy_commands.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_policy_error_reason.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace {
// Constants derived from the spec, https://github.com/WICG/origin-policy
static const char* kDefaultPolicy = "0";
static const char* kDeletePolicy = "0";
static const char* kWellKnown = "/.well-known/origin-policy/";
static const char* kReportTo = "report-to";
static const char* kPolicy = "policy";

// Marker for (temporarily) exempted origins.
// TODO(vogelheim): Make sure this is outside the value space for policy
//                  names. A name with a comma in it shouldn't be allowed, but
//                  I don't think we presently check this anywhere.
static const char* kExemptedOriginPolicy = "exception,";

// Maximum policy size (implementation-defined limit in bytes).
// (Limit copied from network::SimpleURLLoader::kMaxBoundedStringDownloadSize.)
static const size_t kMaxPolicySize = 1024 * 1024;
}  // namespace

namespace content {

// Implement the public "API" from
// content/public/browser/origin_policy_commands.h:
void OriginPolicyAddExceptionFor(const GURL& url) {
  OriginPolicyThrottle::AddExceptionFor(url);
}

// static
bool OriginPolicyThrottle::ShouldRequestOriginPolicy(
    const GURL& url,
    std::string* request_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool origin_policy_enabled =
      base::FeatureList::IsEnabled(features::kOriginPolicy) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
  if (!origin_policy_enabled)
    return false;

  if (!url.SchemeIs(url::kHttpsScheme))
    return false;

  if (request_version) {
    const KnownVersionMap& versions = GetKnownVersions();
    const auto iter = versions.find(url::Origin::Create(url));
    bool has_version = iter != versions.end();
    bool use_default = !has_version || iter->second == kExemptedOriginPolicy;
    *request_version = use_default ? std::string(kDefaultPolicy) : iter->second;
  }
  return true;
}

// static
std::unique_ptr<NavigationThrottle>
OriginPolicyThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle);

  // We use presence of the origin policy request header to determine
  // whether we should create the throttle.
  if (!handle->GetRequestHeaders().HasHeader(
          net::HttpRequestHeaders::kSecOriginPolicy))
    return nullptr;

  // TODO(vogelheim): Rewrite & hoist up this DCHECK to ensure that ..HasHeader
  //     and ShouldRequestOriginPolicy are always equal on entry to the method.
  //     This depends on https://crbug.com/881234 being fixed.
  DCHECK(OriginPolicyThrottle::ShouldRequestOriginPolicy(handle->GetURL(),
                                                         nullptr));
  return base::WrapUnique(new OriginPolicyThrottle(handle));
}

// static
void OriginPolicyThrottle::AddExceptionFor(const GURL& url) {
  GetKnownVersions()[url::Origin::Create(url)] = kExemptedOriginPolicy;
}

OriginPolicyThrottle::~OriginPolicyThrottle() {}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillStartRequest() {
  // TODO(vogelheim): It might be faster in the common case to optimistically
  //     fetch the policy indicated in the request already here. This would
  //     be faster if the last known version is the current version, but
  //     slower (and wasteful of bandwidth) if the server sends us a new
  //     version. It's unclear how much the upside is, though.
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillProcessResponse() {
  DCHECK(navigation_handle());

  // Per spec, Origin Policies are only fetched for https:-requests. So we
  // should always have HTTP headers at this point.
  // However, some unit tests generate responses without headers, so we still
  // need to check.
  if (!navigation_handle()->GetResponseHeaders())
    return NavigationThrottle::PROCEED;

  // This determines whether and which policy version applies and fetches it.
  //
  // Inputs are the kSecOriginPolicy HTTP header, and the version
  // we've last seen from this particular origin.
  //
  // - header with kDeletePolicy received: No policy applies, and delete the
  //       last-known policy for this origin.
  // - header received: Use header version and update last-known policy.
  // - no header received, last-known version exists: Use last-known version
  // - no header, no last-known version: No policy applies.

  std::string response_version =
      GetRequestedPolicyAndReportGroupFromHeader().policy_version;
  bool header_found = !response_version.empty();

  url::Origin origin = GetRequestOrigin();
  DCHECK(!origin.Serialize().empty());
  DCHECK(!origin.opaque());
  KnownVersionMap& versions = GetKnownVersions();
  auto iter = versions.find(origin);

  // Process policy deletion first!
  if (header_found && response_version == kDeletePolicy) {
    if (iter != versions.end())
      versions.erase(iter);
    return NavigationThrottle::PROCEED;
  }

  // Process policy exceptions.
  if (iter != versions.end() && iter->second == kExemptedOriginPolicy) {
    return NavigationThrottle::PROCEED;
  }

  // No policy applies to this request?
  if (!header_found && iter == versions.end()) {
    return NavigationThrottle::PROCEED;
  }

  if (!header_found)
    response_version = iter->second;
  else if (iter == versions.end())
    versions.insert(std::make_pair(origin, response_version));
  else
    iter->second = response_version;

  FetchCallback done =
      base::BindOnce(&OriginPolicyThrottle::OnTheGloriousPolicyHasArrived,
                     base::Unretained(this));
  RedirectCallback redirect = base::BindRepeating(
      &OriginPolicyThrottle::OnRedirect, base::Unretained(this));
  FetchPolicy(GetPolicyURL(response_version), std::move(done),
              std::move(redirect));
  return NavigationThrottle::DEFER;
}

const char* OriginPolicyThrottle::GetNameForLogging() {
  return "OriginPolicyThrottle";
}

// static
OriginPolicyThrottle::KnownVersionMap&
OriginPolicyThrottle::GetKnownVersionsForTesting() {
  return GetKnownVersions();
}

OriginPolicyThrottle::OriginPolicyThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

OriginPolicyThrottle::KnownVersionMap&
OriginPolicyThrottle::GetKnownVersions() {
  static base::NoDestructor<KnownVersionMap> map_instance;
  return *map_instance;
}

OriginPolicyThrottle::PolicyVersionAndReportTo OriginPolicyThrottle::
    GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(
        const std::string& header) {
  return GetRequestedPolicyAndReportGroupFromHeaderString(header);
}

OriginPolicyThrottle::PolicyVersionAndReportTo
OriginPolicyThrottle::GetRequestedPolicyAndReportGroupFromHeader() const {
  std::string header;
  navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
      net::HttpRequestHeaders::kSecOriginPolicy, &header);
  return GetRequestedPolicyAndReportGroupFromHeaderString(header);
}

OriginPolicyThrottle::PolicyVersionAndReportTo
OriginPolicyThrottle::GetRequestedPolicyAndReportGroupFromHeaderString(
    const std::string& header) {
  // Compatibility with early spec drafts, for safety reasons:
  // A lonely "0" will be recognized, so that deletion of a policy always works.
  if (net::HttpUtil::TrimLWS(header) == kDeletePolicy)
    return PolicyVersionAndReportTo({kDeletePolicy, ""});

  std::string policy;
  std::string report_to;
  bool valid = true;
  net::HttpUtil::NameValuePairsIterator iter(header.cbegin(), header.cend(),
                                             ',');
  while (iter.GetNext()) {
    std::string token_value = net::HttpUtil::TrimLWS(iter.value()).as_string();
    bool is_token = net::HttpUtil::IsToken(token_value);
    if (iter.name() == kPolicy) {
      valid &= is_token && policy.empty();
      policy = token_value;
    } else if (iter.name() == kReportTo) {
      valid &= is_token && report_to.empty();
      report_to = token_value;
    }
  }
  valid &= iter.valid();

  if (!valid)
    return PolicyVersionAndReportTo();
  return PolicyVersionAndReportTo({policy, report_to});
}

const url::Origin OriginPolicyThrottle::GetRequestOrigin() const {
  return url::Origin::Create(navigation_handle()->GetURL());
}

const GURL OriginPolicyThrottle::GetPolicyURL(
    const std::string& version) const {
  return GURL(GetRequestOrigin().Serialize() + kWellKnown + version);
}

void OriginPolicyThrottle::FetchPolicy(const GURL& url,
                                       FetchCallback done,
                                       RedirectCallback redirect) {
  // Create the traffic annotation
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("origin_policy_loader", R"(
        semantics {
          sender: "Origin Policy URL Loader Throttle"
          description:
            "Fetches the Origin Policy with a given version from an origin."
          trigger:
            "In case the Origin Policy with a given version does not "
            "exist in the cache, it is fetched from the origin at a "
            "well-known location."
          data:
            "None, the URL itself contains the origin and Origin Policy "
            "version."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings. Server "
            "opt-in or out of this mechanism."
          policy_exception_justification:
            "Not implemented, considered not useful."})");

  // Create and configure the SimpleURLLoader for the policy.
  std::unique_ptr<network::ResourceRequest> policy_request =
      std::make_unique<network::ResourceRequest>();
  policy_request->url = url;
  policy_request->request_initiator = url::Origin::Create(url);
  policy_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                               net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_AUTH_DATA;
  url_loader_ = network::SimpleURLLoader::Create(std::move(policy_request),
                                                 traffic_annotation);
  url_loader_->SetOnRedirectCallback(std::move(redirect));

  // Obtain the URLLoaderFactory from the NavigationHandle.
  SiteInstance* site_instance = navigation_handle()->GetStartingSiteInstance();
  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      site_instance->GetBrowserContext(), site_instance);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      storage_partition->GetURLLoaderFactoryForBrowserProcess();

  network::mojom::URLLoaderFactory* factory =
      url_loader_factory_for_testing_ ? url_loader_factory_for_testing_.get()
                                      : url_loader_factory.get();

  // Start the download, and pass the callback for when we're finished.
  url_loader_->DownloadToString(factory, std::move(done), kMaxPolicySize);
}

void OriginPolicyThrottle::InjectPolicyForTesting(
    const std::string& policy_content) {
  OnTheGloriousPolicyHasArrived(std::make_unique<std::string>(policy_content));
}

void OriginPolicyThrottle::SetURLLoaderFactoryForTesting(
    std::unique_ptr<network::mojom::URLLoaderFactory>
        url_loader_factory_for_testing) {
  url_loader_factory_for_testing_ = std::move(url_loader_factory_for_testing);
}

void OriginPolicyThrottle::OnTheGloriousPolicyHasArrived(
    std::unique_ptr<std::string> policy_content) {
  // Release resources associated with the loading.
  url_loader_.reset();

  // Fail hard if the policy could not be loaded.
  if (!policy_content) {
    CancelNavigation(OriginPolicyErrorReason::kCannotLoadPolicy);
    return;
  }

  // TODO(vogelheim): Determine whether we need to parse or sanity check
  //                  the policy content at this point.

  static_cast<NavigationHandleImpl*>(navigation_handle())
      ->navigation_request()
      ->SetOriginPolicy(*policy_content);
  Resume();
}

void OriginPolicyThrottle::OnRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  // Fail hard if the policy response follows a redirect.
  url_loader_.reset();  // Cancel the request while it's ongoing.
  CancelNavigation(OriginPolicyErrorReason::kPolicyShouldNotRedirect);
}

void OriginPolicyThrottle::CancelNavigation(OriginPolicyErrorReason reason) {
  base::Optional<std::string> error_page =
      GetContentClient()->browser()->GetOriginPolicyErrorPage(
          reason, navigation_handle());
  CancelDeferredNavigation(NavigationThrottle::ThrottleCheckResult(
      NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page));
  Report(reason);
}

#if BUILDFLAG(ENABLE_REPORTING)
void OriginPolicyThrottle::Report(OriginPolicyErrorReason reason) {
  const PolicyVersionAndReportTo header_values =
      GetRequestedPolicyAndReportGroupFromHeader();
  if (header_values.report_to.empty())
    return;

  std::string user_agent;
  navigation_handle()->GetRequestHeaders().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);

  std::string origin_policy_header;
  navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
      net::HttpRequestHeaders::kSecOriginPolicy, &origin_policy_header);
  const char* reason_str = nullptr;
  switch (reason) {
    case OriginPolicyErrorReason::kCannotLoadPolicy:
      reason_str = "CANNOT_LOAD";
      break;
    case OriginPolicyErrorReason::kPolicyShouldNotRedirect:
      reason_str = "REDIRECT";
      break;
    case OriginPolicyErrorReason::kOther:
      reason_str = "OTHER";
      break;
  }

  base::DictionaryValue report_body;
  report_body.SetKey(
      "origin_policy_url",
      base::Value(GetPolicyURL(header_values.policy_version).spec()));
  report_body.SetKey("policy", base::Value(origin_policy_header));
  report_body.SetKey("policy_error_reason", base::Value(reason_str));

  SiteInstance* site_instance = navigation_handle()->GetStartingSiteInstance();
  BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                      site_instance)
      ->GetNetworkContext()
      ->QueueReport("origin-policy", header_values.report_to,
                    navigation_handle()->GetURL(), user_agent,
                    std::move(report_body));
}
#else
void OriginPolicyThrottle::Report(OriginPolicyErrorReason reason) {}
#endif  // BUILDFLAG(ENABLE_REPORTING)

}  // namespace content
