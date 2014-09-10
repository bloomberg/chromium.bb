// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_TEST_RULES_REGISTRY_H__

#include "extensions/browser/api/declarative/rules_registry.h"

namespace extensions {

// This is a trivial test RulesRegistry that can only store and retrieve rules.
class TestRulesRegistry : public RulesRegistry {
 public:
  TestRulesRegistry(content::BrowserThread::ID owner_thread,
                    const std::string& event_name,
                    const WebViewKey& webview_key);
  TestRulesRegistry(
      content::BrowserContext* browser_context,
      const std::string& event_name,
      content::BrowserThread::ID owner_thread,
      RulesCacheDelegate* cache_delegate,
      const WebViewKey& webview_key);

  // RulesRegistry implementation:
  virtual std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) OVERRIDE;
  virtual std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) OVERRIDE;
  virtual std::string RemoveAllRulesImpl(
      const std::string& extension_id) OVERRIDE;

  // Sets the result message that will be returned by the next call of
  // AddRulesImpl, RemoveRulesImpl and RemoveAllRulesImpl.
  void SetResult(const std::string& result);

 protected:
  virtual ~TestRulesRegistry();

 private:
  // The string that gets returned by the implementation functions of
  // RulesRegistry. Defaults to "".
  std::string result_;

  DISALLOW_COPY_AND_ASSIGN(TestRulesRegistry);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
