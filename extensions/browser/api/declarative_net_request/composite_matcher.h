// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

namespace extensions {
namespace declarative_net_request {

// Per extension instance which manages the different rulesets for an extension
// while respecting their priorities.
class CompositeMatcher {
 public:
  using MatcherList = std::vector<std::unique_ptr<RulesetMatcher>>;

  // Each RulesetMatcher should have a distinct ID and priority.
  explicit CompositeMatcher(MatcherList matchers);
  ~CompositeMatcher();

  // Returns whether the network request as specified by |params| should be
  // blocked.
  bool ShouldBlockRequest(const RequestParams& params) const;

  // Returns whether the network request as specified by |params| should be
  // redirected along with the |redirect_url|. |redirect_url| must not be null.
  bool ShouldRedirectRequest(const RequestParams& params,
                             GURL* redirect_url) const;

 private:
  // Sorted by priority in descending order.
  MatcherList matchers_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
