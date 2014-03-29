// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_H__

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/events.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/one_shot_event.h"

class Profile;

namespace base {
class Value;
}  // namespace base

namespace extensions {

class RulesCacheDelegate;

// A base class for RulesRegistries that takes care of storing the
// RulesRegistry::Rule objects. It contains all the methods that need to run on
// the registry thread; methods that need to run on the UI thread are separated
// in the RulesCacheDelegate object.
class RulesRegistry : public base::RefCountedThreadSafe<RulesRegistry> {
 public:
  typedef extensions::api::events::Rule Rule;
  struct WebViewKey {
    int embedder_process_id;
    int webview_instance_id;
    WebViewKey(int embedder_process_id, int webview_instance_id)
        : embedder_process_id(embedder_process_id),
          webview_instance_id(webview_instance_id) {}
    bool operator<(const WebViewKey& other) const {
      return embedder_process_id < other.embedder_process_id ||
          ((embedder_process_id == other.embedder_process_id) &&
           (webview_instance_id < other.webview_instance_id));
    }
  };

  enum Defaults { DEFAULT_PRIORITY = 100 };
  // After the RulesCacheDelegate object (the part of the registry which runs on
  // the UI thread) is created, a pointer to it is passed to |*ui_part|.
  // In tests, |profile| and |ui_part| can be NULL (at the same time). In that
  // case the storage functionality disabled (no RulesCacheDelegate object
  // created).
  RulesRegistry(Profile* profile,
                const std::string& event_name,
                content::BrowserThread::ID owner_thread,
                RulesCacheDelegate* cache_delegate,
                const WebViewKey& webview_key);

  const OneShotEvent& ready() const {
    return ready_;
  }

  // RulesRegistry implementation:

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
  std::string AddRules(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules);

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
  std::string RemoveRules(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers);

  // Same as RemoveAllRules but acts on all rules owned by |extension_id|.
  std::string RemoveAllRules(const std::string& extension_id);

  // Returns all rules listed in |rule_identifiers| and owned by |extension_id|
  // registered in this RuleRegistry. Entries in |rule_identifiers| that
  // are unknown are ignored.
  //
  // The returned rules are stored in |out|. Ownership is passed to the caller.
  void GetRules(const std::string& extension_id,
                const std::vector<std::string>& rule_identifiers,
                std::vector<linked_ptr<RulesRegistry::Rule> >* out);

  // Same as GetRules but returns all rules owned by |extension_id|.
  void GetAllRules(const std::string& extension_id,
                   std::vector<linked_ptr<RulesRegistry::Rule> >* out);

  // Called to notify the RulesRegistry that the extension availability has
  // changed, so that the registry can update which rules are active.
  void OnExtensionUnloaded(const std::string& extension_id);
  void OnExtensionUninstalled(const std::string& extension_id);
  void OnExtensionLoaded(const std::string& extension_id);

  // Returns the number of entries in used_rule_identifiers_ for leak detection.
  // Every ExtensionId counts as one entry, even if it contains no rules.
  size_t GetNumberOfUsedRuleIdentifiersForTesting() const;

  // Returns the RulesCacheDelegate. This is used for testing.
  RulesCacheDelegate* rules_cache_delegate_for_testing() const {
    return cache_delegate_.get();
  }

  // Returns the profile where the rules registry lives.
  Profile* profile() const { return profile_; }

  // Returns the ID of the thread on which the rules registry lives.
  // It is safe to call this function from any thread.
  content::BrowserThread::ID owner_thread() const { return owner_thread_; }

  // The name of the event with which rules are registered.
  const std::string& event_name() const { return event_name_; }

  // The key that identifies the webview (or tabs) in which these rules apply.
  // If the rules apply to the main browser, then this returns the tuple (0, 0).
  const WebViewKey& webview_key() const {
    return webview_key_;
  }

 protected:
  virtual ~RulesRegistry();

  // The precondition for calling this method is that all rules have unique IDs.
  // AddRules establishes this precondition and calls into this method.
  // Stored rules already meet this precondition and so they avoid calling
  // CheckAndFillInOptionalRules for improved performance.
  //
  // Returns an empty string if the function is successful or an error
  // message otherwise.
  std::string AddRulesNoFill(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules);

  // These functions need to apply the rules to the browser, while the base
  // class will handle defaulting empty fields before calling *Impl, and will
  // automatically cache the rules and re-call *Impl on browser startup.
  virtual std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) = 0;
  virtual std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) = 0;
  virtual std::string RemoveAllRulesImpl(
      const std::string& extension_id) = 0;

 private:
  friend class base::RefCountedThreadSafe<RulesRegistry>;
  friend class RulesCacheDelegate;

  typedef std::string ExtensionId;
  typedef std::string RuleId;
  typedef std::pair<ExtensionId, RuleId> RulesDictionaryKey;
  typedef std::map<RulesDictionaryKey, linked_ptr<RulesRegistry::Rule> >
      RulesDictionary;
  enum ProcessChangedRulesState {
    // ProcessChangedRules can never be called, |cache_delegate_| is NULL.
    NEVER_PROCESS,
    // A task to call ProcessChangedRules is scheduled for future execution.
    SCHEDULED_FOR_PROCESSING,
    // No task to call ProcessChangedRules is scheduled yet, but it is possible
    // to schedule one.
    NOT_SCHEDULED_FOR_PROCESSING
  };
  typedef std::map<ExtensionId, ProcessChangedRulesState> ProcessStateMap;

  base::WeakPtr<RulesRegistry> GetWeakPtr() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Common processing after extension's rules have changed.
  void ProcessChangedRules(const std::string& extension_id);

  // Calls ProcessChangedRules if
  // |process_changed_rules_requested_(extension_id)| ==
  // NOT_SCHEDULED_FOR_PROCESSING.
  void MaybeProcessChangedRules(const std::string& extension_id);

  // This method implements the functionality of RemoveAllRules, except for not
  // calling MaybeProcessChangedRules. That way updating the rules store and
  // extension prefs is avoided. This method is called when an extension is
  // uninstalled, that way there is no clash with the preferences being wiped.
  std::string RemoveAllRulesNoStoreUpdate(const std::string& extension_id);

  void MarkReady(base::Time storage_init_time);

  // Deserialize the rules from the given Value object and add them to the
  // RulesRegistry.
  void DeserializeAndAddRules(const std::string& extension_id,
                              scoped_ptr<base::Value> rules);

  // The profile to which this rules registry belongs.
  Profile* profile_;

  // The ID of the thread on which the rules registry lives.
  const content::BrowserThread::ID owner_thread_;

  // The name of the event with which rules are registered.
  const std::string event_name_;

  // The key that identifies the context in which these rules apply.
  WebViewKey webview_key_;

  RulesDictionary rules_;

  // Signaled when we have finished reading from storage for all extensions that
  // are loaded on startup.
  OneShotEvent ready_;

  // The factory needs to be declared before |cache_delegate_|, so that it can
  // produce a pointer as a construction argument for |cache_delegate_|.
  base::WeakPtrFactory<RulesRegistry> weak_ptr_factory_;

  // |cache_delegate_| is owned by the registry service. If |cache_delegate_| is
  // NULL, then the storage functionality is disabled (this is used in tests).
  // This registry cannot own |cache_delegate_| because during the time after
  // rules registry service shuts down on UI thread, and the registry is
  // destroyed on its thread, the use of the |cache_delegate_| would not be
  // safe. The registry only ever associates with one RulesCacheDelegate
  // instance.
  base::WeakPtr<RulesCacheDelegate> cache_delegate_;

  ProcessStateMap process_changed_rules_requested_;

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

  typedef std::string RuleIdentifier;
  typedef std::map<ExtensionId, std::set<RuleIdentifier> > RuleIdentifiersMap;
  RuleIdentifiersMap used_rule_identifiers_;
  int last_generated_rule_identifier_id_;

  DISALLOW_COPY_AND_ASSIGN(RulesRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_H__
