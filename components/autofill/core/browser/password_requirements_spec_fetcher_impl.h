// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_REQUIREMENTS_SPEC_FETCHER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_REQUIREMENTS_SPEC_FETCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace autofill {

class PasswordRequirementsSpec;

// Fetches PasswordRequirementsSpec for a specific origin.
class PasswordRequirementsSpecFetcherImpl
    : public PasswordRequirementsSpecFetcher {
 public:
  using FetchCallback =
      base::OnceCallback<void(const PasswordRequirementsSpec&)>;

  // This enum is used in histograms. Do not change or reuse values.
  enum class ResultCode {
    // Fetched spec file, parsed it, but found no entry for the origin.
    kFoundNoSpec = 0,
    // Fetched spec file, parsed it and found an entry.
    kFoundSpec = 1,
    // The origin is an IP address, not HTTP/HTTPS, or not a valid URL.
    kErrorInvalidOrigin = 2,
    // Server responded with an empty document or an error code.
    kErrorFailedToFetch = 3,
    // Server timed out.
    kErrorTimeout = 4,
    // Server responded with a document but it could not be parsed.
    kErrorFailedToParse = 5,
    kMaxValue = kErrorFailedToParse,
  };

  // See the member variables for explanations of these parameters.
  PasswordRequirementsSpecFetcherImpl(int version,
                                      size_t prefix_length,
                                      int timeout);
  ~PasswordRequirementsSpecFetcherImpl() override;

  // Implementation for PasswordRequirementsSpecFetcher:
  void Fetch(network::mojom::URLLoaderFactory* loader_factory,
             const GURL& origin,
             FetchCallback callback) override;

 private:
  void OnFetchComplete(std::unique_ptr<std::string> response_body);
  void OnFetchTimeout();
  void TriggerCallback(ResultCode result, const PasswordRequirementsSpec& spec);

  // A version counter for requirements specs. If data changes on the server,
  // a new version number is pushed out to prevent that clients continue
  // using old cached data. This allows setting the HTTP caching expiration to
  // infinity.
  int version_;

  // The fetcher determines the URL of the spec file by first hashing the eTLD+1
  // of |origin| and then taking the first |prefix_length_| bits of the hash
  // value as part of the file name. (See the code for details.)
  // |prefix_length_| should always be <= 32 as filenames are limited to the
  // first 4 bytes of the hash prefix.
  size_t prefix_length_;

  // Timeout in milliseconds after which any ongoing fetch operation is
  // canceled.
  int timeout_;

  // Origin for which a spec is fetched. Only set while fetching is in progress
  // and the callback has not been called.
  GURL origin_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Callback to be called if the network request resolves or is aborted.
  FetchCallback callback_;

  // Timer to kill pending downloads after |timeout_|.
  base::OneShotTimer download_timer_;

  DISALLOW_COPY_AND_ASSIGN(PasswordRequirementsSpecFetcherImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_REQUIREMENTS_SPEC_FETCHER_IMPL_H_
