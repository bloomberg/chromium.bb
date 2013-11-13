// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"

namespace extensions {

TestRulesRegistry::TestRulesRegistry(content::BrowserThread::ID owner_thread,
                                     const std::string& event_name,
                                     const WebViewKey& webview_key)
    : RulesRegistry(NULL /*profile*/,
                    event_name,
                    owner_thread,
                    NULL,
                    webview_key) {}

TestRulesRegistry::TestRulesRegistry(
    Profile* profile,
    const std::string& event_name,
    content::BrowserThread::ID owner_thread,
    RulesCacheDelegate* cache_delegate,
    const WebViewKey& webview_key)
    : RulesRegistry(profile,
                    event_name,
                    owner_thread,
                    cache_delegate,
                    webview_key) {}

std::string TestRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  return result_;
}

std::string TestRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  return result_;
}

std::string TestRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  return result_;
}

void TestRulesRegistry::SetResult(const std::string& result) {
  result_ = result;
}

TestRulesRegistry::~TestRulesRegistry() {}

}  // namespace extensions
