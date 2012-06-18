// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"

namespace extensions {

TestRulesRegistry::TestRulesRegistry()
    : RulesRegistryWithCache(NULL),
      owner_thread_(content::BrowserThread::UI) {}

void TestRulesRegistry::SetOwnerThread(
    content::BrowserThread::ID owner_thread) {
  owner_thread_ = owner_thread;
}

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

content::BrowserThread::ID TestRulesRegistry::GetOwnerThread() const {
  return owner_thread_;
}

void TestRulesRegistry::SetResult(const std::string& result) {
  result_ = result;
}

TestRulesRegistry::~TestRulesRegistry() {}

}  // namespace extensions
