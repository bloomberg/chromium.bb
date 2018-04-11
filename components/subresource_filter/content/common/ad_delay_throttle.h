// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_DELAY_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_DELAY_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/common/url_loader_throttle.h"
#include "services/network/public/cpp/resource_request.h"

class GURL;

namespace subresource_filter {

// This class delays ad requests satisfying certain conditions.
// - The ad is insecure (e.g. uses http).
// TODO(csharrison): Add delays for when the request is in a same-origin iframe.
class AdDelayThrottle : public content::URLLoaderThrottle {
 public:
  static constexpr base::TimeDelta kDefaultDelay =
      base::TimeDelta::FromMilliseconds(50);

  class MetadataProvider {
   public:
    virtual ~MetadataProvider() {}
    virtual bool IsAdRequest() = 0;
    // TODO(csharrison): Add an interface for querying same-origin iframe
    // status.
  };

  // Mainly used for caching values that we don't want to compute for every
  // resource request.
  class Factory {
   public:
    Factory();
    ~Factory();

    std::unique_ptr<AdDelayThrottle> MaybeCreate(
        std::unique_ptr<MetadataProvider> provider) const;

    base::TimeDelta insecure_delay() const { return insecure_delay_; }

   private:
    const base::TimeDelta insecure_delay_;
    const bool delay_enabled_ = false;

    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  // This enum backs a histogram. Make sure to only append elements, and update
  // enums.xml with new values.
  enum class SecureInfo {
    // Ad that was loaded securely (e.g. using https).
    kSecureAd = 0,

    // Ad that was loaded insecurely (e.g. at least one request through http).
    kInsecureAd = 1,

    kSecureNonAd = 2,
    kInsecureNonAd = 3,

    // Add new elements above kLast.
    kMaxValue = kInsecureNonAd
  };

  ~AdDelayThrottle() override;

 private:
  // content::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           const network::ResourceResponseHead& response_head,
                           bool* defer) override;

  // Returns whether the request to |url| should be deferred.
  bool MaybeDefer(const GURL& url);
  void Resume();

  AdDelayThrottle(std::unique_ptr<MetadataProvider> provider,
                  base::TimeDelta insecure_delay,
                  bool delay_enabled);

  // Will never be nullptr.
  std::unique_ptr<MetadataProvider> provider_;

  // How long to delay an ad request that is insecure.
  const base::TimeDelta insecure_delay_;

  // Only defer at most once per request.
  bool has_deferred_ = false;

  // Tracks whether this request was ever insecure, across all its redirects.
  bool was_ever_insecure_ = false;

  // Whether to actually delay the request. If set to false, will operate in a
  // dry-run style mode that only logs metrics.
  const bool delay_enabled_ = false;

  base::WeakPtrFactory<AdDelayThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdDelayThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_COMMON_AD_DELAY_THROTTLE_H_
