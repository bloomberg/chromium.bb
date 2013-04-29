// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"

namespace extensions {

TestRulesRegistry::TestRulesRegistry(content::BrowserThread::ID owner_thread,
                                     const char* event_name)
    : RulesRegistryWithCache(NULL /*profile*/,
                             event_name,
                             owner_thread,
                             false /*log_storage_init_delay, this is ignored*/,
                             NULL /*ui_part*/) {}

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
