// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_

#include <memory>

#include "base/files/memory_mapped_file.h"
#include "components/url_pattern_index/url_pattern_index.h"

namespace base {
class FilePath;
}  // namespace base

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace extensions {
namespace declarative_net_request {

namespace flat {
struct ExtensionIndexedRuleset;
}  // namespace flat

// RulesetMatcher encapsulates the Declarative Net Request API ruleset
// corresponding to a single extension. The ruleset file is memory mapped. This
// uses the url_pattern_index component to achieve fast matching of network
// requests against declarative rules. Since this class is immutable, it is
// thread-safe. In practice it is accessed on the IO thread but created on a
// sequence where file IO is allowed.
class RulesetMatcher {
 public:
  // Describes the result of creating a RulesetMatcher instance.
  // This is logged as part of UMA. Hence existing values should not be re-
  // numbered or deleted. New values should be added before kLoadRulesetMax.
  enum LoadRulesetResult {
    kLoadSuccess = 0,
    kLoadErrorInvalidPath = 1,
    kLoadErrorMemoryMap = 2,
    kLoadErrorRulesetVerification = 3,
    kLoadResultMax
  };

  // Factory function to create a verified RulesetMatcher.
  // |indexed_ruleset_path| is the path of the extension ruleset in flatbuffer
  // format. Must be called on a sequence where file IO is allowed. Returns
  // kLoadSuccess on success along with the ruleset |matcher|.
  static LoadRulesetResult CreateVerifiedMatcher(
      const base::FilePath& indexed_ruleset_path,
      int expected_ruleset_checksum,
      std::unique_ptr<RulesetMatcher>* matcher);

  ~RulesetMatcher();

  // Returns whether the network request as specified by the passed parameters
  // should be blocked.
  bool ShouldBlockRequest(const GURL& url,
                          const url::Origin& first_party_origin,
                          url_pattern_index::flat::ElementType element_type,
                          bool is_third_party) const;

 private:
  using UrlPatternIndexMatcher = url_pattern_index::UrlPatternIndexMatcher;

  explicit RulesetMatcher(std::unique_ptr<base::MemoryMappedFile> ruleset_file);

  // The memory mapped ruleset file.
  std::unique_ptr<base::MemoryMappedFile> ruleset_;

  const flat::ExtensionIndexedRuleset* root_;
  const UrlPatternIndexMatcher blacklist_matcher_;
  const UrlPatternIndexMatcher whitelist_matcher_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
