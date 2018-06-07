// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/password_requirements_spec_fetcher_impl.h"

#include "base/logging.h"
#include "base/md5.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/browser/proto/password_requirements_shard.pb.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/url_canon.h"

namespace autofill {

PasswordRequirementsSpecFetcherImpl::PasswordRequirementsSpecFetcherImpl(
    int version,
    size_t prefix_length,
    int timeout)
    : version_(version), prefix_length_(prefix_length), timeout_(timeout) {
  DCHECK_GE(version_, 0);
  DCHECK_LE(prefix_length_, 32u);
  DCHECK_GE(timeout_, 0);
}

PasswordRequirementsSpecFetcherImpl::~PasswordRequirementsSpecFetcherImpl() =
    default;

namespace {

// Hashes the eTLD+1 of |origin| via MD5 and returns a filename with the first
// |prefix_length| bits populated. The returned value corresponds to the first
// 4 bytes of the truncated MD5 prefix in hex notation.
// For example:
//   "https://www.example.com" has a eTLD+1 of "example.com".
//   The MD5SUM of that is 5ababd603b22780302dd8d83498e5172.
//   Stripping this to the first 8 bits (prefix_length = 8) gives
//   500000000000000000000000000000000. The file name is always cut to the first
//   four bytes, i.e. 5000 in this example.
std::string GetHashPrefix(const GURL& origin, size_t prefix_length) {
  DCHECK_LE(prefix_length, 32u);
  std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  base::MD5Digest digest;
  base::MD5Sum(domain_and_registry.data(), domain_and_registry.size(), &digest);

  for (size_t i = 0; i < base::size(digest.a); ++i) {
    if (prefix_length >= 8) {
      prefix_length -= 8;
      continue;
    } else {
      // Determine the |prefix_length| most significant bits by calculating
      // the 8 - |prefix_length| least significant bits and inverting the
      // result.
      digest.a[i] &= ~((1 << (8 - prefix_length)) - 1);
      prefix_length = 0;
    }
  }

  return base::MD5DigestToBase16(digest).substr(0, 4);
}

// Returns the URL on gstatic.com where the passwords spec file can be found
// that contains data for |origin|.
GURL GetUrlForRequirementsSpec(const GURL& origin,
                               int version,
                               size_t prefix_length) {
  std::string hash_prefix = GetHashPrefix(origin, prefix_length);
  return GURL(base::StringPrintf(
      "https://www.gstatic.com/chrome/autofill/password_generation_specs/%d/%s",
      version, hash_prefix.c_str()));
}

}  // namespace

void PasswordRequirementsSpecFetcherImpl::Fetch(
    network::mojom::URLLoaderFactory* loader_factory,
    const GURL& origin,
    FetchCallback callback) {
  DCHECK(origin.is_valid());
  DCHECK(origin_.is_empty());
  DCHECK(callback_.is_null());
  origin_ = origin;
  callback_ = std::move(callback);

  if (!origin.is_valid() || origin.HostIsIPAddress() ||
      !origin.SchemeIsHTTPOrHTTPS()) {
    TriggerCallback(ResultCode::kErrorInvalidOrigin,
                    PasswordRequirementsSpec());
    return;
  }
  // Canonicalize away trailing periods in hostname.
  while (!origin_.host().empty() && origin_.host().back() == '.') {
    std::string new_host =
        origin_.host().substr(0, origin_.host().length() - 1);
    url::Replacements<char> replacements;
    replacements.SetHost(new_host.c_str(),
                         url::Component(0, new_host.length()));
    origin_ = origin_.ReplaceComponents(replacements);
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("password_requirements_spec_fetch",
                                          R"(
      semantics {
        sender: "Password requirements specification fetcher"
        description:
          "Fetches the password requirements for a set of domains whose origin "
          "hash starts with a certain prefix."
        trigger:
          "When the user triggers a password generation (this can happen by "
          "just focussing a password field)."
        data:
          "The URL encodes a hash prefix from which it is not possible to "
          "derive the original origin. No user information is sent."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting: "Unconditionally enabled."
        policy_exception_justification:
          "Not implemented, considered not useful."
      })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GetUrlForRequirementsSpec(origin_, version_, prefix_length_);
  resource_request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SEND_AUTH_DATA;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory,
      base::BindOnce(&PasswordRequirementsSpecFetcherImpl::OnFetchComplete,
                     base::Unretained(this)));

  download_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_),
                        this,
                        &PasswordRequirementsSpecFetcherImpl::OnFetchTimeout);
}

void PasswordRequirementsSpecFetcherImpl::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  // Destroy the fetcher when this method returns.
  std::unique_ptr<network::SimpleURLLoader> loader(std::move(url_loader_));

  if (!response_body || loader->NetError() != net::Error::OK) {
    TriggerCallback(ResultCode::kErrorFailedToFetch,
                    PasswordRequirementsSpec());
    return;
  }

  PasswordRequirementsShard shard;
  if (!shard.ParseFromString(*response_body)) {
    TriggerCallback(ResultCode::kErrorFailedToParse,
                    PasswordRequirementsSpec());
    return;
  }

  // Search shard for matches for origin_ by looking up the (canonicalized)
  // host name and then stripping domain prefixes until the eTLD+1 is reached.
  DCHECK(!origin_.HostIsIPAddress());
  // |host| is a std::string instead of StringPiece as the protbuf::Map
  // implementation does not support StringPieces as parameters for find.
  std::string host = origin_.host();
  auto it = shard.specs().find(host);
  if (it != shard.specs().end()) {
    TriggerCallback(ResultCode::kFoundSpec, it->second);
    return;
  }

  const std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin_,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  while (host.length() > 0 && host != domain_and_registry) {
    size_t pos = host.find('.');
    if (pos != std::string::npos) {  // strip prefix
      host = host.substr(pos + 1);
    } else {
      break;
    }
    // If an entry has ben found exit with that.
    auto it = shard.specs().find(host);
    if (it != shard.specs().end()) {
      TriggerCallback(ResultCode::kFoundSpec, it->second);
      return;
    }
  }

  TriggerCallback(ResultCode::kFoundNoSpec, PasswordRequirementsSpec());
}

void PasswordRequirementsSpecFetcherImpl::OnFetchTimeout() {
  url_loader_.reset();
  TriggerCallback(ResultCode::kErrorTimeout, PasswordRequirementsSpec());
}

void PasswordRequirementsSpecFetcherImpl::TriggerCallback(
    ResultCode result,
    const PasswordRequirementsSpec& spec) {
  // TODO(crbug.com/846694) Return latencies.
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.RequirementsSpecFetcher.Result",
                            result);
  std::move(callback_).Run(spec);
}

}  // namespace autofill
