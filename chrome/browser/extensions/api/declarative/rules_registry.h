// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_H__
#pragma once

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/api/events.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

class RulesRegistry;

// Traits that describe how RulesRegistry should be deleted. This just deletes
// the RulesRegistry on its owner thread.
struct RulesRegistryDeleteTraits {
 public:
  static void Destruct(const RulesRegistry* rules_registry);
};

// Interface for rule registries.
//
// All functions except GetOwnerThread() are only called on the thread
// indicated by GetOwnerThread(). The object is destroyed on the owner thread.
class RulesRegistry
    : public base::RefCountedThreadSafe<RulesRegistry,
                                        RulesRegistryDeleteTraits> {
 public:
  typedef extensions::api::events::Rule Rule;

  // Registers |rules|, owned by |extension_id| to this RulesRegistry.
  // If a concrete RuleRegistry does not support some of the rules,
  // it may ignore them.
  //
  // |rules| is a list of Rule instances following the definition of the
  // declarative extension APIs. It is guaranteed that each rule in |rules| has
  // a unique name within the scope of |extension_id| that has not been
  // registered before, unless it has been removed again.
  // The ownership of rules remains with the caller.
  //
  // Returns an empty string if the function is successful or an error
  // message otherwise.
  //
  // IMPORTANT: This function is atomic. Either all rules that are deemed
  // relevant are added or none.
  virtual std::string AddRules(
      const std::string& extension_id,
      const std::vector<linked_ptr<Rule> >& rules) = 0;

  // Unregisters all rules listed in |rule_identifiers| and owned by
  // |extension_id| from this RulesRegistry.
  // Some or all IDs in |rule_identifiers| may not be stored in this
  // RulesRegistry and are ignored.
  //
  // Returns an empty string if the function is successful or an error
  // message otherwise.
  //
  // IMPORTANT: This function is atomic. Either all rules that are deemed
  // relevant are removed or none.
  virtual std::string RemoveRules(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) = 0;

  // Same as RemoveAllRules but acts on all rules owned by |extension_id|.
  virtual std::string RemoveAllRules(const std::string& extension_id) = 0;

  // Returns all rules listed in |rule_identifiers| and owned by |extension_id|
  // registered in this RuleRegistry. Entries in |rule_identifiers| that
  // are unknown are ignored.
  //
  // The returned rules are stored in |out|. Ownership is passed to the caller.
  //
  // Returns an empty string if the function is successful or an error
  // message otherwise.
  virtual std::string GetRules(const std::string& extension_id,
                               const std::vector<std::string>& rule_identifiers,
                               std::vector<linked_ptr<Rule> >* out) = 0;

  // Same as GetRules but returns all rules owned by |extension_id|.
  virtual std::string GetAllRules(const std::string& extension_id,
                                  std::vector<linked_ptr<Rule> >* out) = 0;

  // Called to notify the RulesRegistry that an extension has been unloaded
  // and all rules of this extension need to be removed.
  virtual void OnExtensionUnloaded(const std::string& extension_id) = 0;

  // Returns the ID of the thread on which the rules registry lives.
  // It must be safe to call this function from any thread.
  virtual content::BrowserThread::ID GetOwnerThread() const = 0;

 protected:
  friend struct RulesRegistryDeleteTraits;
  friend class base::DeleteHelper<RulesRegistry>;
  virtual ~RulesRegistry() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_H__
