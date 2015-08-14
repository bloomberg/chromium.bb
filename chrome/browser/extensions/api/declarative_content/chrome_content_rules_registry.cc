// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

using url_matcher::URLMatcherConditionSet;

namespace extensions {

namespace {

// TODO(jyasskin): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "declarativeContent.PageStateMatcher";
const char kUnknownConditionAttribute[] = "Unknown condition attribute '%s'";

}  // namespace

//
// ContentCondition
//

ContentCondition::ContentCondition(
    scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate,
    scoped_ptr<DeclarativeContentCssPredicate> css_predicate,
    scoped_ptr<DeclarativeContentIsBookmarkedPredicate>
        is_bookmarked_predicate)
    : page_url_predicate(page_url_predicate.Pass()),
      css_predicate(css_predicate.Pass()),
      is_bookmarked_predicate(is_bookmarked_predicate.Pass()) {
}

ContentCondition::~ContentCondition() {}

scoped_ptr<ContentCondition> CreateContentCondition(
    const Extension* extension,
    const PredicateFactory<DeclarativeContentCssPredicate>&
        css_predicate_factory,
    const PredicateFactory<DeclarativeContentIsBookmarkedPredicate>&
        is_bookmarked_predicate_factory,
    const PredicateFactory<DeclarativeContentPageUrlPredicate>&
        page_url_predicate_factory,
    const base::Value& condition,
    std::string* error) {
  const base::DictionaryValue* condition_dict = NULL;
  if (!condition.GetAsDictionary(&condition_dict)) {
    *error = kExpectedDictionary;
    return scoped_ptr<ContentCondition>();
  }

  // Verify that we are dealing with a Condition whose type we understand.
  std::string instance_type;
  if (!condition_dict->GetString(declarative_content_constants::kInstanceType,
                                 &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return scoped_ptr<ContentCondition>();
  }
  if (instance_type != declarative_content_constants::kPageStateMatcherType) {
    *error = kExpectedOtherConditionType;
    return scoped_ptr<ContentCondition>();
  }

  scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate;
  scoped_ptr<DeclarativeContentCssPredicate> css_predicate;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> is_bookmarked_predicate;

  for (base::DictionaryValue::Iterator iter(*condition_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& predicate_name = iter.key();
    const base::Value& predicate_value = iter.value();
    if (predicate_name == declarative_content_constants::kInstanceType) {
      // Skip this.
    } else if (predicate_name == declarative_content_constants::kPageUrl) {
      page_url_predicate = page_url_predicate_factory.Run(extension,
                                                          predicate_value,
                                                          error);
    } else if (predicate_name == declarative_content_constants::kCss) {
      css_predicate = css_predicate_factory.Run(extension, predicate_value,
                                                error);
    } else if (predicate_name == declarative_content_constants::kIsBookmarked) {
      is_bookmarked_predicate = is_bookmarked_predicate_factory.Run(
          extension,
          predicate_value,
          error);
    } else {
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  predicate_name.c_str());
    }
    if (!error->empty())
      return scoped_ptr<ContentCondition>();
  }

  return make_scoped_ptr(new ContentCondition(page_url_predicate.Pass(),
                                              css_predicate.Pass(),
                                              is_bookmarked_predicate.Pass()));
}

//
// EvaluationScope
//

// Used to coalesce multiple requests for evaluation into a zero or one actual
// evaluations (depending on the EvaluationDisposition).  This is required for
// correctness when multiple trackers respond to the same event. Otherwise,
// executing the request from the first tracker will be done before the tracked
// state has been updated for the other trackers.
class ChromeContentRulesRegistry::EvaluationScope {
 public:
  // Default disposition is PERFORM_EVALUATION.
  explicit EvaluationScope(ChromeContentRulesRegistry* registry);
  EvaluationScope(ChromeContentRulesRegistry* registry,
                  EvaluationDisposition disposition);
  ~EvaluationScope();

 private:
  ChromeContentRulesRegistry* const registry_;
  const EvaluationDisposition previous_disposition_;

  DISALLOW_COPY_AND_ASSIGN(EvaluationScope);
};

ChromeContentRulesRegistry::EvaluationScope::EvaluationScope(
    ChromeContentRulesRegistry* registry)
    : EvaluationScope(registry, DEFER_REQUESTS) {}

ChromeContentRulesRegistry::EvaluationScope::EvaluationScope(
    ChromeContentRulesRegistry* registry,
    EvaluationDisposition disposition)
    : registry_(registry),
      previous_disposition_(registry_->evaluation_disposition_) {
  DCHECK_NE(EVALUATE_REQUESTS, disposition);
  registry_->evaluation_disposition_ = disposition;
}

ChromeContentRulesRegistry::EvaluationScope::~EvaluationScope() {
  registry_->evaluation_disposition_ = previous_disposition_;
  if (registry_->evaluation_disposition_ == EVALUATE_REQUESTS) {
    for (content::WebContents* tab : registry_->evaluation_pending_)
      registry_->EvaluateConditionsForTab(tab);
    registry_->evaluation_pending_.clear();
  }
}

//
// ChromeContentRulesRegistry
//

ChromeContentRulesRegistry::ChromeContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate)
    : ContentRulesRegistry(browser_context,
                           declarative_content_constants::kOnPageChanged,
                           content::BrowserThread::UI,
                           cache_delegate,
                           RulesRegistryService::kDefaultRulesRegistryID),
      page_url_condition_tracker_(browser_context, this),
      css_condition_tracker_(browser_context, this),
      is_bookmarked_condition_tracker_(browser_context, this),
      evaluation_disposition_(EVALUATE_REQUESTS) {
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void ChromeContentRulesRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      content::WebContents* tab =
          content::Source<content::WebContents>(source).ptr();
      // Note that neither non-tab WebContents nor tabs from other browser
      // contexts will be in the map.
      active_rules_.erase(tab);
      break;
    }
  }
}

void ChromeContentRulesRegistry::RequestEvaluation(
    content::WebContents* contents) {
  switch (evaluation_disposition_) {
    case EVALUATE_REQUESTS:
      EvaluateConditionsForTab(contents);
      break;
    case DEFER_REQUESTS:
      evaluation_pending_.insert(contents);
      break;
    case IGNORE_REQUESTS:
      break;
  }
}

bool ChromeContentRulesRegistry::ShouldManageConditionsForBrowserContext(
    content::BrowserContext* context) {
  return ManagingRulesForBrowserContext(context);
}

void ChromeContentRulesRegistry::MonitorWebContentsForRuleEvaluation(
    content::WebContents* contents) {
  // We rely on active_rules_ to have a key-value pair for |contents| to know
  // which WebContents we are working with.
  active_rules_[contents] = std::set<const ContentRule*>();

  EvaluationScope evaluation_scope(this);
  page_url_condition_tracker_.TrackForWebContents(contents);
  css_condition_tracker_.TrackForWebContents(contents);
  is_bookmarked_condition_tracker_.TrackForWebContents(contents);
}

void ChromeContentRulesRegistry::DidNavigateMainFrame(
    content::WebContents* contents,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (ContainsKey(active_rules_, contents)) {
    EvaluationScope evaluation_scope(this);
    page_url_condition_tracker_.OnWebContentsNavigation(contents, details,
                                                        params);
    css_condition_tracker_.OnWebContentsNavigation(contents, details, params);
    is_bookmarked_condition_tracker_.OnWebContentsNavigation(contents, details,
                                                             params);
  }
}

ChromeContentRulesRegistry::ContentRule::ContentRule(
    const Extension* extension,
    ScopedVector<const ContentCondition> conditions,
    ScopedVector<const ContentAction> actions,
    int priority)
    : extension(extension),
      conditions(conditions.Pass()),
      actions(actions.Pass()),
      priority(priority) {
}

ChromeContentRulesRegistry::ContentRule::~ContentRule() {}

scoped_ptr<const ChromeContentRulesRegistry::ContentRule>
ChromeContentRulesRegistry::CreateRule(
    const Extension* extension,
    const PredicateFactory<DeclarativeContentCssPredicate>&
        css_predicate_factory,
    const PredicateFactory<DeclarativeContentIsBookmarkedPredicate>&
        is_bookmarked_predicate_factory,
    const PredicateFactory<DeclarativeContentPageUrlPredicate>&
        page_url_predicate_factory,
    const api::events::Rule& api_rule,
    std::string* error) {
  ScopedVector<const ContentCondition> conditions;
  for (const linked_ptr<base::Value>& value : api_rule.conditions) {
    conditions.push_back(
        CreateContentCondition(extension, css_predicate_factory,
                               is_bookmarked_predicate_factory,
                               page_url_predicate_factory, *value, error));
    if (!error->empty())
      return scoped_ptr<ContentRule>();
  }

  ScopedVector<const ContentAction> actions;
  for (const linked_ptr<base::Value>& value : api_rule.actions) {
    actions.push_back(ContentAction::Create(browser_context(), extension,
                                            *value, error));
    if (!error->empty())
      return scoped_ptr<ContentRule>();
  }

  // Note: |api_rule| may contain tags, but these are ignored.

  return make_scoped_ptr(
      new ContentRule(extension, conditions.Pass(), actions.Pass(),
                      *api_rule.priority));
}

bool ChromeContentRulesRegistry::ManagingRulesForBrowserContext(
    content::BrowserContext* context) {
  // Manage both the normal context and incognito contexts associated with it.
  return Profile::FromBrowserContext(context)->GetOriginalProfile() ==
      Profile::FromBrowserContext(browser_context());
}

std::set<const ChromeContentRulesRegistry::ContentRule*>
ChromeContentRulesRegistry::GetMatchingRules(content::WebContents* tab) const {
  const bool is_incognito_tab = tab->GetBrowserContext()->IsOffTheRecord();
  std::set<const ContentRule*> matching_rules;
  for (const RulesMap::value_type& rule_id_rule_pair : content_rules_) {
    const ContentRule* rule = rule_id_rule_pair.second.get();
    if (is_incognito_tab &&
        !ShouldEvaluateExtensionRulesForIncognitoRenderer(rule->extension))
      continue;

    for (const ContentCondition* condition : rule->conditions) {
      if (condition->page_url_predicate &&
          !page_url_condition_tracker_.EvaluatePredicate(
              condition->page_url_predicate.get(),
              tab)) {
        continue;
      }

      if (condition->css_predicate &&
          !css_condition_tracker_.EvaluatePredicate(
              condition->css_predicate.get(),
              tab)) {
        continue;
      }

      if (condition->is_bookmarked_predicate &&
          !condition->is_bookmarked_predicate->IsIgnored() &&
          !is_bookmarked_condition_tracker_.EvaluatePredicate(
              condition->is_bookmarked_predicate.get(),
              tab)) {
        continue;
      }

      matching_rules.insert(rule);
    }
  }
  return matching_rules;
}

std::string ChromeContentRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<api::events::Rule>>& api_rules) {
  EvaluationScope evaluation_scope(this);
  const Extension* extension = ExtensionRegistry::Get(browser_context())
      ->GetInstalledExtension(extension_id);
  DCHECK(extension);

  std::string error;
  RulesMap new_rules;

  // These callbacks are only used during the CreateRule call and not stored, so
  // it's safe to supply an Unretained tracker pointer.
  const PredicateFactory<DeclarativeContentCssPredicate>
      css_predicate_factory =
          base::Bind(&DeclarativeContentCssConditionTracker::CreatePredicate,
                     base::Unretained(&css_condition_tracker_));
  const PredicateFactory<DeclarativeContentIsBookmarkedPredicate>
      is_bookmarked_predicate_factory =
          base::Bind(
              &DeclarativeContentIsBookmarkedConditionTracker::CreatePredicate,
              base::Unretained(&is_bookmarked_condition_tracker_));
  const PredicateFactory<DeclarativeContentPageUrlPredicate>
      page_url_predicate_factory =
          base::Bind(
              &DeclarativeContentPageUrlConditionTracker::CreatePredicate,
              base::Unretained(&page_url_condition_tracker_));

  for (const linked_ptr<api::events::Rule>& api_rule : api_rules) {
    ExtensionIdRuleIdPair rule_id(extension_id, *api_rule->id);
    DCHECK(content_rules_.find(rule_id) == content_rules_.end());

    scoped_ptr<const ContentRule> rule(
        CreateRule(extension, css_predicate_factory,
                   is_bookmarked_predicate_factory, page_url_predicate_factory,
                   *api_rule, &error));
    if (!error.empty()) {
      // Clean up temporary condition sets created during rule creation.
      page_url_condition_tracker_.ClearUnusedConditionSets();
      return error;
    }
    DCHECK(rule);

    new_rules[rule_id] = make_linked_ptr(rule.release());
  }

  // Wohoo, everything worked fine.
  content_rules_.insert(new_rules.begin(), new_rules.end());

  // Record the URL matcher condition set to rule condition pair mappings, and
  // register URL patterns in the URL matcher.
  URLMatcherConditionSet::Vector all_new_condition_sets;
  for (const RulesMap::value_type& rule_id_rule_pair : new_rules) {
    const linked_ptr<const ContentRule>& rule = rule_id_rule_pair.second;
    for (const ContentCondition* condition : rule->conditions) {
      if (condition->page_url_predicate) {
        all_new_condition_sets.push_back(
            condition->page_url_predicate->url_matcher_condition_set());
      }
    }
  }
  page_url_condition_tracker_.AddConditionSets(all_new_condition_sets);

  UpdateCssSelectorsFromRules();

  // Request evaluation for all WebContents, under the assumption that a
  // non-empty condition has been added.
  for (auto web_contents_rules_pair : active_rules_)
    RequestEvaluation(web_contents_rules_pair.first);

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // Ignore evaluation requests in this function because it reverts actions on
  // any active rules itself. Otherwise, we run the risk of reverting the same
  // rule multiple times.
  EvaluationScope evaluation_scope(this, IGNORE_REQUESTS);
  // URLMatcherConditionSet IDs that can be removed from URLMatcher.
  std::vector<URLMatcherConditionSet::ID> condition_set_ids_to_remove;

  for (const std::string& id : rule_identifiers) {
    // Skip unknown rules.
    RulesMap::iterator content_rules_entry =
        content_rules_.find(std::make_pair(extension_id, id));
    if (content_rules_entry == content_rules_.end())
      continue;

    // Remove state associated with URL matcher conditions, and collect the
    // URLMatcherConditionSet::IDs to remove later.
    URLMatcherConditionSet::Vector condition_sets;
    const ContentRule* rule = content_rules_entry->second.get();
    for (const ContentCondition* condition : rule->conditions) {
      if (condition->page_url_predicate) {
        URLMatcherConditionSet::ID condition_set_id =
            condition->page_url_predicate->url_matcher_condition_set()->id();
        condition_set_ids_to_remove.push_back(condition_set_id);
      }
    }

    // Remove the ContentRule from active_rules_.
    for (auto& tab_rules_pair : active_rules_) {
      if (ContainsKey(tab_rules_pair.second, rule)) {
        ContentAction::ApplyInfo apply_info =
            {rule->extension, browser_context(), tab_rules_pair.first,
             rule->priority};
        for (const ContentAction* action : rule->actions)
          action->Revert(apply_info);
        tab_rules_pair.second.erase(rule);
      }
    }

    // Remove reference to actual rule.
    content_rules_.erase(content_rules_entry);
  }

  // Clear URLMatcher of condition sets that are not needed any more.
  page_url_condition_tracker_.RemoveConditionSets(
      condition_set_ids_to_remove);

  UpdateCssSelectorsFromRules();

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  // Search all identifiers of rules that belong to extension |extension_id|.
  std::vector<std::string> rule_identifiers;
  for (const RulesMap::value_type& id_rule_pair : content_rules_) {
    const ExtensionIdRuleIdPair& extension_id_rule_id_pair = id_rule_pair.first;
    if (extension_id_rule_id_pair.first == extension_id)
      rule_identifiers.push_back(extension_id_rule_id_pair.second);
  }

  return RemoveRulesImpl(extension_id, rule_identifiers);
}

void ChromeContentRulesRegistry::UpdateCssSelectorsFromRules() {
  std::set<std::string> css_selectors;  // We rely on this being sorted.
  for (const RulesMap::value_type& id_rule_pair : content_rules_) {
    const ContentRule* rule = id_rule_pair.second.get();
    for (const ContentCondition* condition : rule->conditions) {
      if (condition->css_predicate) {
        const std::vector<std::string>& condition_css_selectors =
            condition->css_predicate->css_selectors();
        css_selectors.insert(condition_css_selectors.begin(),
                             condition_css_selectors.end());
      }
    }
  }

  css_condition_tracker_.SetWatchedCssSelectors(css_selectors);
}

void ChromeContentRulesRegistry::EvaluateConditionsForTab(
    content::WebContents* tab) {
  std::set<const ContentRule*> matching_rules = GetMatchingRules(tab);
  if (matching_rules.empty() && !ContainsKey(active_rules_, tab))
    return;

  std::set<const ContentRule*>& prev_matching_rules = active_rules_[tab];
  for (const ContentRule* rule : matching_rules) {
    ContentAction::ApplyInfo apply_info =
        {rule->extension, browser_context(), tab, rule->priority};
    if (!ContainsKey(prev_matching_rules, rule)) {
      for (const ContentAction* action : rule->actions)
        action->Apply(apply_info);
    } else {
      for (const ContentAction* action : rule->actions)
        action->Reapply(apply_info);
    }
  }
  for (const ContentRule* rule : prev_matching_rules) {
    if (!ContainsKey(matching_rules, rule)) {
      ContentAction::ApplyInfo apply_info =
          {rule->extension, browser_context(), tab, rule->priority};
      for (const ContentAction* action : rule->actions)
        action->Revert(apply_info);
    }
  }

  if (matching_rules.empty())
    active_rules_[tab].clear();
  else
    swap(matching_rules, prev_matching_rules);
}

bool
ChromeContentRulesRegistry::ShouldEvaluateExtensionRulesForIncognitoRenderer(
    const Extension* extension) const {
  if (!util::IsIncognitoEnabled(extension->id(), browser_context()))
    return false;

  // Split-mode incognito extensions register their rules with separate
  // RulesRegistries per Original/OffTheRecord browser contexts, whereas
  // spanning-mode extensions share the Original browser context.
  if (util::CanCrossIncognito(extension, browser_context())) {
    // The extension uses spanning mode incognito. No rules should have been
    // registered for the extension in the OffTheRecord registry so
    // execution for that registry should never reach this point.
    CHECK(!browser_context()->IsOffTheRecord());
  } else {
    // The extension uses split mode incognito. Both the Original and
    // OffTheRecord registries may have (separate) rules for this extension.
    // Since we're looking at an incognito renderer, so only the OffTheRecord
    // registry should process its rules.
    if (!browser_context()->IsOffTheRecord())
      return false;
  }

  return true;
}

bool ChromeContentRulesRegistry::IsEmpty() const {
  return content_rules_.empty() && page_url_condition_tracker_.IsEmpty();
}

void ChromeContentRulesRegistry::UpdateMatchingCssSelectorsForTesting(
    content::WebContents* contents,
    const std::vector<std::string>& matching_css_selectors) {
  css_condition_tracker_.UpdateMatchingCssSelectorsForTesting(
      contents,
      matching_css_selectors);
}

size_t ChromeContentRulesRegistry::GetActiveRulesCountForTesting() {
  size_t count = 0;
  for (auto web_contents_rules_pair : active_rules_)
    count += web_contents_rules_pair.second.size();
  return count;
}

ChromeContentRulesRegistry::~ChromeContentRulesRegistry() {
}

}  // namespace extensions
