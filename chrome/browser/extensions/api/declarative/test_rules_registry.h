// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
#pragma once

#include "chrome/browser/extensions/api/declarative/rules_registry.h"

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"

namespace extensions {

// This is currently a trivial stub that can only store and retrieve rules.
//
// TODO(battre): Generalize this into a StoringRulesRegistry from which
// other concrete RulesRegistries can derive. The purpose of this class
// would then be to handle just the storage of rules.
class TestRulesRegistry : public RulesRegistry {
 public:
  TestRulesRegistry();
  virtual ~TestRulesRegistry();

  // RulesRegistry implementation:
  virtual std::string AddRules(
      const std::string& extension_id,
      const std::vector<base::DictionaryValue*>& rules) OVERRIDE;
  virtual std::string RemoveRules(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) OVERRIDE;
  virtual std::string RemoveAllRules(
      const std::string& extension_id) OVERRIDE;
  virtual std::string GetRules(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers,
      std::vector<base::DictionaryValue*>* out) OVERRIDE;
  virtual std::string GetAllRules(
      const std::string& extension_id,
      std::vector<base::DictionaryValue*>* out) OVERRIDE;
  virtual void OnExtensionUnloaded(const std::string& extension_id) OVERRIDE;

 private:
  // Map of rule identifier to actual rule.
  // TODO(battre): consider the extension_ids as part of the key.
  typedef std::map<std::string, linked_ptr<base::DictionaryValue> >
      RulesDictionary;
  RulesDictionary rules_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
