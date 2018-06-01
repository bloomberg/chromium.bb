// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"

#include "base/logging.h"
#include "base/md5.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace autofill {

PasswordRequirementsSpecFetcher::PasswordRequirementsSpecFetcher(
    int version,
    size_t prefix_length,
    int timeout)
    : version_(version), prefix_length_(prefix_length), timeout_(timeout) {
  DCHECK_GE(version_, 0);
  DCHECK_LE(prefix_length_, 32u);
  DCHECK_GE(timeout_, 0);
}

PasswordRequirementsSpecFetcher::~PasswordRequirementsSpecFetcher() = default;

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
      "https://www.gstatic.com/chrome/password_requirements/%d/%s", version,
      hash_prefix.c_str()));
}

}  // namespace

void PasswordRequirementsSpecFetcher::Fetch(
    network::mojom::URLLoaderFactory* loader_factory,
    const GURL& origin,
    FetchCallback callback) {
  DCHECK(origin.is_valid());
  DCHECK(origin_.is_empty());
  DCHECK(callback_.is_null());
  origin_ = origin;
  callback_ = std::move(callback);

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
      GetUrlForRequirementsSpec(origin, version_, prefix_length_);
  resource_request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SEND_AUTH_DATA;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory,
      base::BindOnce(&PasswordRequirementsSpecFetcher::OnFetchComplete,
                     base::Unretained(this)));

  download_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_),
                        this, &PasswordRequirementsSpecFetcher::OnFetchTimeout);
}

void PasswordRequirementsSpecFetcher::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  // Destroy the fetcher when this method returns.
  std::unique_ptr<network::SimpleURLLoader> loader(std::move(url_loader_));

  if (!response_body || loader->NetError() != net::Error::OK) {
    // TODO(crbug.com/846694): log error in UKM / UMA.
    std::move(callback_).Run(PasswordRequirementsSpec());
    return;
  }

  // TODO(crbug.com/846694): log statistics, process data.
  // For now this is just some dummy data for testing.
  PasswordRequirementsSpec spec;
  spec.set_min_length(17);
  std::move(callback_).Run(spec);
}

void PasswordRequirementsSpecFetcher::OnFetchTimeout() {
  url_loader_.reset();
  // TODO(crbug.com/846694): log error (abort)
  std::move(callback_).Run(PasswordRequirementsSpec());
}

}  // namespace autofill
