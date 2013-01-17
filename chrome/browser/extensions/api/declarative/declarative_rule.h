// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeclarativeRule<>, DeclarativeConditionSet<>, and DeclarativeActionSet<>
// templates usable with multiple different declarativeFoo systems.  These are
// templated on the Condition and Action types that define the behavior of a
// particular declarative event.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_RULE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_RULE_H__

#include <limits>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "chrome/common/extensions/api/events.h"
#include "chrome/common/extensions/matcher/url_matcher.h"

namespace base {
class Time;
class Value;
}

namespace extensions {

// This class stores a set of conditions that may be part of a DeclarativeRule.
// If any condition is fulfilled, the Actions of the DeclarativeRule can be
// triggered.
//
// ConditionT should be immutable after creation.  It must define the following
// members:
//
//   // Arguments passed through from ConditionSet::Create.
//   static scoped_ptr<ConditionT> Create(
//       URLMatcherConditionFactory*,
//       // Except this argument gets elements of the AnyVector.
//       const base::Value& definition,
//       std::string* error);
//   // If the Condition needs to be filtered by some
//   // URLMatcherConditionSets, append them to this argument.
//   // DeclarativeConditionSet::GetURLMatcherConditionSets forwards here.
//   void GetURLMatcherConditionSets(
//       URLMatcherConditionSet::Vector* condition_sets)
//   // True if GetURLMatcherConditionSets would append anything to its
//   // argument.
//   bool has_url_matcher_condition_set();
//   // |url_matches| and |match_data| passed through from
//   // ConditionSet::IsFulfilled.
//   bool IsFulfilled(
//       const std::set<URLMatcherConditionSet::ID>& url_matches,
//       const ConditionT::MatchData&);
template<typename ConditionT>
class DeclarativeConditionSet {
 public:
  typedef std::vector<linked_ptr<base::Value> > AnyVector;
  typedef std::vector<linked_ptr<const ConditionT> > Conditions;
  typedef typename Conditions::const_iterator const_iterator;

  // Factory method that creates an WebRequestConditionSet according to the JSON
  // array |conditions| passed by the extension API.
  // Sets |error| and returns NULL in case of an error.
  static scoped_ptr<DeclarativeConditionSet> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const AnyVector& conditions,
      std::string* error);

  const Conditions& conditions() const {
    return conditions_;
  }

  const_iterator begin() const { return conditions_.begin(); }
  const_iterator end() const { return conditions_.end(); }

  // If |url_match_trigger| is a member of |url_matches|, then this
  // returns whether the corresponding condition is fulfilled
  // wrt. |request_data|. If |url_match_trigger| is -1, this function
  // returns whether any of the conditions without URL attributes is
  // satisfied.
  //
  // Conditions for which has_url_matcher_condition_set() is false are always
  // checked (aside from short-circuiting when an earlier condition already
  // matched.)
  //
  // Conditions for which has_url_matcher_condition_set() is true are only
  // checked if one of the URLMatcherConditionSets returned by
  // GetURLMatcherConditionSets() has an id listed in url_matches.  That means
  // that if |match_data| contains URL matches for two pieces of a request,
  // their union should appear in |url_matches|.  For kinds of MatchData that
  // only have one type of URL, |url_matches| is forwarded on to
  // ConditionT::IsFulfilled(), so it doesn't need to appear in |match_data|.
  bool IsFulfilled(URLMatcherConditionSet::ID url_match_trigger,
                   const std::set<URLMatcherConditionSet::ID>& url_matches,
                   const typename ConditionT::MatchData& match_data) const;

  // Appends the URLMatcherConditionSet from all conditions to |condition_sets|.
  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const;

  // Returns whether there are some conditions without UrlFilter attributes.
  bool HasConditionsWithoutUrls() const {
    return !conditions_without_urls_.empty();
  }

 private:
  typedef std::map<URLMatcherConditionSet::ID, const ConditionT*>
      URLMatcherIdToCondition;

  DeclarativeConditionSet(
      const Conditions& conditions,
      const URLMatcherIdToCondition& match_id_to_condition,
      const std::vector<const ConditionT*>& conditions_without_urls);

  const URLMatcherIdToCondition match_id_to_condition_;
  const Conditions conditions_;
  const std::vector<const ConditionT*> conditions_without_urls_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeConditionSet);
};

// Immutable container for multiple actions.
//
// ActionT should be immutable after creation.  It must define the following
// members:
//
//   // Arguments passed through from ActionSet::Create.
//   static scoped_ptr<ActionT> Create(
//       // Except this argument gets elements of the AnyVector.
//       const base::Value& definition,
//       std::string* error, bool* bad_message);
//   void Apply(const std::string& extension_id,
//              const base::Time& extension_install_time,
//              // Contains action-type-specific in/out parameters.
//              typename ActionT::ApplyInfo* apply_info) const;
//   // Only needed if the RulesRegistry calls DeclarativeActionSet::Revert().
//   void Revert(const std::string& extension_id,
//               const base::Time& extension_install_time,
//               // Contains action-type-specific in/out parameters.
//               typename ActionT::ApplyInfo* apply_info) const;
//   // Return the minimum priority of rules that can be evaluated after this
//   // action runs.  Return MIN_INT by default.
//   int GetMinimumPriority() const;
//
// TODO(battre): As DeclarativeActionSet can become the single owner of all
// actions, we can optimize here by making some of them singletons (e.g. Cancel
// actions).
template<typename ActionT>
class DeclarativeActionSet {
 public:
  typedef std::vector<linked_ptr<base::Value> > AnyVector;
  typedef std::vector<linked_ptr<const ActionT> > Actions;

  explicit DeclarativeActionSet(const Actions& actions);

  // Factory method that instantiates a DeclarativeActionSet according to
  // |actions| which represents the array of actions received from the
  // extension API.
  static scoped_ptr<DeclarativeActionSet> Create(const AnyVector& actions,
                                                 std::string* error,
                                                 bool* bad_message);

  // Rules call this method when their conditions are fulfilled.
  void Apply(const std::string& extension_id,
             const base::Time& extension_install_time,
             typename ActionT::ApplyInfo* apply_info) const;

  // Rules call this method when they have stateful conditions, and those
  // conditions stop being fulfilled.  Rules with event-based conditions (e.g. a
  // network request happened) will never Revert() an action.
  void Revert(const std::string& extension_id,
              const base::Time& extension_install_time,
              typename ActionT::ApplyInfo* apply_info) const;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT.
  int GetMinimumPriority() const;

  const Actions& actions() const { return actions_; }

 private:
  const Actions actions_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeActionSet);
};

// Representation of a rule of a declarative API:
// https://developer.chrome.com/beta/extensions/events.html#declarative.
// Generally a RulesRegistry will hold a collection of Rules for a given
// declarative API and contain the logic for matching and applying them.
//
// See DeclarativeConditionSet and DeclarativeActionSet for the requirements on
// ConditionT and ActionT.
template<typename ConditionT, typename ActionT>
class DeclarativeRule {
 public:
  typedef std::string ExtensionId;
  typedef std::string RuleId;
  typedef std::pair<ExtensionId, RuleId> GlobalRuleId;
  typedef int Priority;
  typedef DeclarativeConditionSet<ConditionT> ConditionSet;
  typedef DeclarativeActionSet<ActionT> ActionSet;
  typedef extensions::api::events::Rule JsonRule;

  // Checks whether the set of |conditions| and |actions| are consistent.
  // Returns true in case of consistency and MUST set |error| otherwise.
  typedef bool (*ConsistencyChecker)(const ConditionSet* conditions,
                                     const ActionSet* actions,
                                     std::string* error);

  DeclarativeRule(const GlobalRuleId& id,
                  base::Time extension_installation_time,
                  scoped_ptr<ConditionSet> conditions,
                  scoped_ptr<ActionSet> actions,
                  Priority priority);

  // Creates a DeclarativeRule for an extension given a json definition.  The
  // format of each condition and action's json is up to the specific ConditionT
  // and ActionT.
  //
  // Before constructing the final rule, calls check_consistency(conditions,
  // actions, error) and returns NULL if it fails.  Pass NULL if no consistency
  // check is needed.  If |error| is empty, the translation was successful and
  // the returned rule is internally consistent.
  static scoped_ptr<DeclarativeRule> Create(
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const std::string& extension_id,
      base::Time extension_installation_time,
      linked_ptr<JsonRule> rule,
      ConsistencyChecker check_consistency,
      std::string* error);

  const GlobalRuleId& id() const { return id_; }
  const std::string& extension_id() const { return id_.first; }
  const ConditionSet& conditions() const { return *conditions_; }
  const ActionSet& actions() const { return *actions_; }
  Priority priority() const { return priority_; }

  // Calls actions().Apply(extension_id(), extension_installation_time_,
  // apply_info). This function should only be called when the conditions_ are
  // fulfilled (from a semantic point of view; no harm is done if this function
  // is called at other times for testing purposes).
  void Apply(typename ActionT::ApplyInfo* apply_info) const;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT. Only valid if the conditions of this rule
  // are fulfilled.
  Priority GetMinimumPriority() const;

 private:
  GlobalRuleId id_;
  base::Time extension_installation_time_;  // For precedences of rules.
  scoped_ptr<ConditionSet> conditions_;
  scoped_ptr<ActionSet> actions_;
  Priority priority_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeRule);
};

// Implementation details below here.

//
// DeclarativeConditionSet
//

template<typename ConditionT>
bool DeclarativeConditionSet<ConditionT>::IsFulfilled(
    URLMatcherConditionSet::ID url_match_trigger,
    const std::set<URLMatcherConditionSet::ID>& url_matches,
    const typename ConditionT::MatchData& match_data) const {
  if (url_match_trigger == -1) {
    // Invalid trigger -- indication that we should only check conditions
    // without URL attributes.
    for (typename std::vector<const ConditionT*>::const_iterator it =
             conditions_without_urls_.begin();
         it != conditions_without_urls_.end(); ++it) {
      if ((*it)->IsFulfilled(url_matches, match_data))
        return true;
    }
    return false;
  }

  typename URLMatcherIdToCondition::const_iterator triggered =
      match_id_to_condition_.find(url_match_trigger);
  return (triggered != match_id_to_condition_.end() &&
          triggered->second->IsFulfilled(url_matches, match_data));
}

template<typename ConditionT>
void DeclarativeConditionSet<ConditionT>::GetURLMatcherConditionSets(
    URLMatcherConditionSet::Vector* condition_sets) const {
  for (typename Conditions::const_iterator i = conditions_.begin();
       i != conditions_.end(); ++i) {
    (*i)->GetURLMatcherConditionSets(condition_sets);
  }
}

// static
template<typename ConditionT>
scoped_ptr<DeclarativeConditionSet<ConditionT> >
DeclarativeConditionSet<ConditionT>::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const AnyVector& conditions,
    std::string* error) {
  Conditions result;

  for (AnyVector::const_iterator i = conditions.begin();
       i != conditions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<ConditionT> condition =
        ConditionT::Create(url_matcher_condition_factory, **i, error);
    if (!error->empty())
      return scoped_ptr<DeclarativeConditionSet>(NULL);
    result.push_back(make_linked_ptr(condition.release()));
  }

  URLMatcherIdToCondition match_id_to_condition;
  std::vector<const ConditionT*> conditions_without_urls;
  URLMatcherConditionSet::Vector condition_sets;

  for (typename Conditions::const_iterator i = result.begin();
       i != result.end(); ++i) {
    condition_sets.clear();
    (*i)->GetURLMatcherConditionSets(&condition_sets);
    if (condition_sets.empty()) {
      conditions_without_urls.push_back(i->get());
    } else {
      for (URLMatcherConditionSet::Vector::const_iterator
               match_set = condition_sets.begin();
           match_set != condition_sets.end(); ++match_set)
        match_id_to_condition[(*match_set)->id()] = i->get();
    }
  }

  return make_scoped_ptr(new DeclarativeConditionSet(
      result, match_id_to_condition, conditions_without_urls));
}

template<typename ConditionT>
DeclarativeConditionSet<ConditionT>::DeclarativeConditionSet(
    const Conditions& conditions,
    const URLMatcherIdToCondition& match_id_to_condition,
    const std::vector<const ConditionT*>& conditions_without_urls)
    : match_id_to_condition_(match_id_to_condition),
      conditions_(conditions),
      conditions_without_urls_(conditions_without_urls) {}

//
// DeclarativeActionSet
//

template<typename ActionT>
DeclarativeActionSet<ActionT>::DeclarativeActionSet(const Actions& actions)
    : actions_(actions) {}

// static
template<typename ActionT>
scoped_ptr<DeclarativeActionSet<ActionT> >
DeclarativeActionSet<ActionT>::Create(
    const AnyVector& actions,
    std::string* error,
    bool* bad_message) {
  *error = "";
  *bad_message = false;
  Actions result;

  for (AnyVector::const_iterator i = actions.begin();
       i != actions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<ActionT> action = ActionT::Create(**i, error, bad_message);
    if (!error->empty() || *bad_message)
      return scoped_ptr<DeclarativeActionSet>(NULL);
    result.push_back(make_linked_ptr(action.release()));
  }

  return scoped_ptr<DeclarativeActionSet>(new DeclarativeActionSet(result));
}

template<typename ActionT>
void DeclarativeActionSet<ActionT>::Apply(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    typename ActionT::ApplyInfo* apply_info) const {
  for (typename Actions::const_iterator i = actions_.begin();
       i != actions_.end(); ++i)
    (*i)->Apply(extension_id, extension_install_time, apply_info);
}

template<typename ActionT>
void DeclarativeActionSet<ActionT>::Revert(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    typename ActionT::ApplyInfo* apply_info) const {
  for (typename Actions::const_iterator i = actions_.begin();
       i != actions_.end(); ++i)
    (*i)->Revert(extension_id, extension_install_time, apply_info);
}

template<typename ActionT>
int DeclarativeActionSet<ActionT>::GetMinimumPriority() const {
  int minimum_priority = std::numeric_limits<int>::min();
  for (typename Actions::const_iterator i = actions_.begin();
       i != actions_.end(); ++i) {
    minimum_priority = std::max(minimum_priority, (*i)->GetMinimumPriority());
  }
  return minimum_priority;
}

//
// DeclarativeRule
//

template<typename ConditionT, typename ActionT>
DeclarativeRule<ConditionT, ActionT>::DeclarativeRule(
    const GlobalRuleId& id,
    base::Time extension_installation_time,
    scoped_ptr<ConditionSet> conditions,
    scoped_ptr<ActionSet> actions,
    Priority priority)
    : id_(id),
      extension_installation_time_(extension_installation_time),
      conditions_(conditions.release()),
      actions_(actions.release()),
      priority_(priority) {
  CHECK(conditions_.get());
  CHECK(actions_.get());
}

// static
template<typename ConditionT, typename ActionT>
scoped_ptr<DeclarativeRule<ConditionT, ActionT> >
DeclarativeRule<ConditionT, ActionT>::Create(
    URLMatcherConditionFactory* url_matcher_condition_factory,
    const std::string& extension_id,
    base::Time extension_installation_time,
    linked_ptr<JsonRule> rule,
    ConsistencyChecker check_consistency,
    std::string* error) {
  scoped_ptr<DeclarativeRule> error_result;

  scoped_ptr<ConditionSet> conditions = ConditionSet::Create(
      url_matcher_condition_factory, rule->conditions, error);
  if (!error->empty())
    return error_result.Pass();
  CHECK(conditions.get());

  bool bad_message = false;
  scoped_ptr<ActionSet> actions =
      ActionSet::Create(rule->actions, error, &bad_message);
  if (bad_message) {
    // TODO(battre) Export concept of bad_message to caller, the extension
    // should be killed in case it is true.
    *error = "An action of a rule set had an invalid "
        "structure that should have been caught by the JSON validator.";
    return error_result.Pass();
  }
  if (!error->empty() || bad_message)
    return error_result.Pass();
  CHECK(actions.get());

  if (check_consistency &&
      !check_consistency(conditions.get(), actions.get(), error)) {
    DCHECK(!error->empty());
    return error_result.Pass();
  }

  CHECK(rule->priority.get());
  int priority = *(rule->priority);

  GlobalRuleId rule_id(extension_id, *(rule->id));
  return scoped_ptr<DeclarativeRule>(
      new DeclarativeRule(rule_id, extension_installation_time,
                         conditions.Pass(), actions.Pass(), priority));
}

template<typename ConditionT, typename ActionT>
void DeclarativeRule<ConditionT, ActionT>::Apply(
    typename ActionT::ApplyInfo* apply_info) const {
  return actions_->Apply(extension_id(),
                         extension_installation_time_,
                         apply_info);
}

template<typename ConditionT, typename ActionT>
int DeclarativeRule<ConditionT, ActionT>::GetMinimumPriority() const {
  return actions_->GetMinimumPriority();
}

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_RULE_H__
