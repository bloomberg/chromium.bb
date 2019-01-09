// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/hint_cache.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/host_filter.h"
#include "net/nqe/effective_connection_type.h"

class GURL;

namespace optimization_guide {
struct HintsComponentInfo;
}

namespace previews {

// Holds previews hints extracted from the configuration.
class PreviewsHints {
 public:
  ~PreviewsHints();

  // Creates a Hints instance from the provided hints component. This must be
  // called using a background task runner as it requires a significant amount
  // of processing.
  static std::unique_ptr<PreviewsHints> CreateFromHintsComponent(
      const optimization_guide::HintsComponentInfo& info);

  // Creates a Hints instance from the provided hints configuration. This must
  // be called using a background task runner as it requires a significant
  // amount of processing.
  static std::unique_ptr<PreviewsHints> CreateFromHintsConfiguration(
      const optimization_guide::proto::Configuration& config);

  // Returns the matching PageHint for |document_url| if found in |hint|.
  // TODO(dougarnett): Consider moving to some hint_util file.
  static const optimization_guide::proto::PageHint* FindPageHint(
      const GURL& document_url,
      const optimization_guide::proto::Hint& hint);

  // Whether the URL is whitelisted for the given previews type. If so,
  // |out_inflation_percent| and |out_ect_threshold| will be populated if
  // metadata is available for them.
  bool IsWhitelisted(const GURL& url,
                     PreviewsType type,
                     int* out_inflation_percent,
                     net::EffectiveConnectionType* out_ect_threshold) const;

  // Whether the URL is blacklisted for the given previews type.
  bool IsBlacklisted(const GURL& url, PreviewsType type) const;

  // Returns whether |url| may have PageHints and triggers asynchronous load
  // of such hints are not currently available synchronously. |callback| is
  // called if any applicable hint data is loaded and available for |url|.
  bool MaybeLoadOptimizationHints(const GURL& url,
                                  HintLoadedCallback callback) const;

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const;

  // Logs UMA for whether the HintCache has a matching Hint and also a matching
  // PageHint for |url|. Records the client's current |ect| as well. This is
  // useful for measuring the effectiveness of the page hints provided by Cacao.
  void LogHintCacheMatch(const GURL& url,
                         bool is_committed,
                         net::EffectiveConnectionType ect) const;

 private:
  friend class PreviewsHintsTest;

  PreviewsHints();

  // Parses optimization filters from |config| and populates corresponding
  // supported blacklists in this object.
  void ParseOptimizationFilters(
      const optimization_guide::proto::Configuration& config);

  // Holds the hint cache (if any optimizations using it are enabled).
  std::unique_ptr<HintCache> hint_cache_;

  // Blacklist of host suffixes for LITE_PAGE_REDIRECT Previews.
  std::unique_ptr<HostFilter> lite_page_redirect_blacklist_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsHints);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_H_
