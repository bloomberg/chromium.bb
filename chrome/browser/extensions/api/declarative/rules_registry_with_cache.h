// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_WITH_CACHE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_WITH_CACHE_H__

#include "chrome/browser/extensions/api/declarative/rules_registry.h"

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/callback_forward.h"

namespace extensions {

// A base class for RulesRegistries that takes care of storing the
// RulesRegistry::Rule objects.
class RulesRegistryWithCache : public RulesRegistry {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true if the registry is ready and ready to start processing
    // rules.
    virtual bool IsReady() = 0;

    // Called to notify the Delegate that the rules for the given extension
    // have changed.
    virtual void OnRulesChanged(RulesRegistryWithCache* rules_registry,
                                const std::string& extension_id) = 0;
  };

  explicit RulesRegistryWithCache(Delegate* delegate);

  // Returns true if we are ready to process rules.
  bool IsReady() {
    return !delegate_.get() || delegate_->IsReady();
  }

  // Add a callback to call when we transition to Ready.
  void AddReadyCallback(const base::Closure& callback);

  // Called by our delegate when we are ready. This is called exactly once,
  // if we have a delegate.
  void OnReady();

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
  virtual content::BrowserThread::ID GetOwnerThread() const OVERRIDE = 0;

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

  // Notify our delegate that the given extension's rules have changed.
  void NotifyRulesChanged(const std::string& extension_id);

  RulesDictionary rules_;

  scoped_ptr<Delegate> delegate_;
  std::vector<base::Closure> ready_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RulesRegistryWithCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_WITH_CACHE_H__
