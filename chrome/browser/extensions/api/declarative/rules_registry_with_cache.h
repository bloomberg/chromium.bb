// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_WITH_CACHE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_WITH_CACHE_H__
#pragma once

#include "chrome/browser/extensions/api/declarative/rules_registry.h"

#include <map>
#include <string>

#include "base/compiler_specific.h"

namespace extensions {

// A base class for RulesRegistries that takes care of storing the
// RulesRegistry::Rule objects.
class RulesRegistryWithCache : public RulesRegistry {
 public:
  RulesRegistryWithCache();

  // RulesRegistry implementation:
  virtual std::string AddRules(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) OVERRIDE;
  virtual std::string RemoveRules(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) OVERRIDE;
  virtual std::string RemoveAllRules(
      const std::string& extension_id) OVERRIDE;
  virtual std::string GetRules(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers,
      std::vector<linked_ptr<RulesRegistry::Rule> >* out) OVERRIDE;
  virtual std::string GetAllRules(
      const std::string& extension_id,
      std::vector<linked_ptr<RulesRegistry::Rule> >* out) OVERRIDE;
  virtual void OnExtensionUnloaded(const std::string& extension_id) OVERRIDE;
  virtual content::BrowserThread::ID GetOwnerThread() const = 0;

 protected:
  virtual ~RulesRegistryWithCache();

  // These functions need to provide the same functionality as their
  // RulesRegistry counterparts. They need to be atomic.
  virtual std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) = 0;
  virtual std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) = 0;
  virtual std::string RemoveAllRulesImpl(
      const std::string& extension_id) = 0;

 private:
  typedef std::string ExtensionId;
  typedef std::string RuleId;
  typedef std::pair<ExtensionId, RuleId> RulesDictionaryKey;
  typedef std::map<RulesDictionaryKey, linked_ptr<RulesRegistry::Rule> >
      RulesDictionary;
  RulesDictionary rules_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_WITH_CACHE_H__
