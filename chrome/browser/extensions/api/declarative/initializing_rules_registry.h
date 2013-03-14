// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_INITIALIZING_RULES_REGISTRY_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_INITIALIZING_RULES_REGISTRY_H__

#include "chrome/browser/extensions/api/declarative/rules_registry.h"

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"

namespace extensions {

// Wrapper class for RulesRegistry objects that takes care that all optional
// fields of rules are filled with valid values.
class InitializingRulesRegistry : public RulesRegistry {
 public:
  enum Defaults { DEFAULT_PRIORITY = 100 };

  explicit InitializingRulesRegistry(scoped_refptr<RulesRegistry> delegate);

  // Implementation for RulesRegistry:
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
  virtual content::BrowserThread::ID GetOwnerThread() const OVERRIDE;

 private:
  virtual ~InitializingRulesRegistry();

  // Returns whether any existing rule is registered with identifier |rule_id|
  // for extension |extension_id|.
  bool IsUniqueId(const std::string& extension_id,
                  const std::string& rule_id) const;

  // Creates an ID that is unique within the scope of|extension_id|.
  std::string GenerateUniqueId(const std::string& extension_id);

  // Verifies that all |rules| have unique IDs or initializes them with
  // unique IDs if they don't have one. In case of duplicate IDs, this function
  // returns a non-empty error message.
  std::string CheckAndFillInOptionalRules(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules);

  // Initializes the priority fields in case they have not been set.
  void FillInOptionalPriorities(
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules);

  // Removes all |identifiers| of |extension_id| from |used_rule_identifiers_|.
  void RemoveUsedRuleIdentifiers(const std::string& extension_id,
                                 const std::vector<std::string>& identifiers);

  // Same as RemoveUsedRuleIdentifiers but operates on all rules of
  // |extension_id|.
  void RemoveAllUsedRuleIdentifiers(const std::string& extension_id);

  scoped_refptr<RulesRegistry> delegate_;

  typedef std::map<std::string, std::set<std::string> > RuleIdentifiersMap;
  RuleIdentifiersMap used_rule_identifiers_;
  int last_generated_rule_identifier_id_;

  DISALLOW_COPY_AND_ASSIGN(InitializingRulesRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_INITIALIZING_RULES_REGISTRY_H__
