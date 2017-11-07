// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_

#include <stddef.h>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "extensions/common/extension_id.h"

namespace net {
class URLRequest;
}  // namespace net

namespace extensions {
class InfoMap;

namespace declarative_net_request {
class RulesetMatcher;

// Manages the set of active rulesets for the Declarative Net Request API. Can
// be constructed on any sequence but must be accessed and destroyed from the
// same sequence.
class RulesetManager {
 public:
  explicit RulesetManager(const InfoMap* info_map);
  ~RulesetManager();

  // An interface used for testing purposes.
  class TestObserver {
   public:
    virtual void OnShouldBlockRequest(const net::URLRequest& request,
                                      bool is_incognito_context) = 0;

   protected:
    virtual ~TestObserver() {}
  };

  // Adds the ruleset for the given |extension_id|. Should not be called twice
  // in succession for an extension.
  void AddRuleset(const ExtensionId& extension_id,
                  std::unique_ptr<RulesetMatcher> ruleset_matcher);

  // Removes the ruleset for |extension_id|. Should be called only after a
  // corresponding AddRuleset.
  void RemoveRuleset(const ExtensionId& extension_id);

  // Returns whether the given |request| should be blocked.
  bool ShouldBlockRequest(const net::URLRequest& request,
                          bool is_incognito_context) const;

  // Returns the number of RulesetMatcher currently being managed.
  size_t GetMatcherCountForTest() const { return rules_map_.size(); }

  // Sets the TestObserver. Client maintains ownership of |observer|.
  void SetObserverForTest(TestObserver* observer);

 private:
  std::map<ExtensionId, std::unique_ptr<RulesetMatcher>> rules_map_;

  // Non-owning pointer to InfoMap. Owns us.
  const InfoMap* const info_map_;

  // Non-owning pointer to TestObserver.
  TestObserver* test_observer_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(RulesetManager);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MANAGER_H_
