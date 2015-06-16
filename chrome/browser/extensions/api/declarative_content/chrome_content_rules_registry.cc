// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/content_condition.h"
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

//
// ScopedEvaluationCoalescer
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

  DISALLOW_COPY_AND_ASSIGN(EvaluationScope);
};

ChromeContentRulesRegistry::EvaluationScope::EvaluationScope(
    ChromeContentRulesRegistry* registry)
    : EvaluationScope(registry, PERFORM_EVALUATION) {}

ChromeContentRulesRegistry::EvaluationScope::EvaluationScope(
    ChromeContentRulesRegistry* registry,
    EvaluationDisposition disposition)
    : registry_(registry) {
  // All coalescers on the stack should have the same disposition.
  DCHECK(registry_->record_rule_evaluation_requests_ == 0 ||
         registry_->evaluation_disposition_ == disposition);
  registry_->evaluation_disposition_ = disposition;
  ++registry_->record_rule_evaluation_requests_;
}

ChromeContentRulesRegistry::EvaluationScope::~EvaluationScope() {
  if (--registry_->record_rule_evaluation_requests_ == 0 &&
      registry_->evaluation_disposition_ == PERFORM_EVALUATION) {
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
      record_rule_evaluation_requests_(0),
      evaluation_disposition_(PERFORM_EVALUATION) {
  extension_info_map_ = ExtensionSystem::Get(browser_context)->info_map();

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
  if (record_rule_evaluation_requests_ > 0) {
    evaluation_pending_.insert(contents);
    return;
  }

  EvaluateConditionsForTab(contents);
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
  }
}

bool ChromeContentRulesRegistry::ManagingRulesForBrowserContext(
    content::BrowserContext* context) {
  // Manage both the normal context and incognito contexts associated with it.
  return Profile::FromBrowserContext(context)->GetOriginalProfile() ==
      Profile::FromBrowserContext(browser_context());
}

std::set<const ContentRule*> ChromeContentRulesRegistry::GetMatches(
    const RendererContentMatchData& renderer_data,
    bool is_incognito_renderer) const {
  std::set<const ContentRule*> result;

  // Then we need to check for each of these, whether the other
  // attributes are also fulfilled.
  for (URLMatcherConditionSet::ID url_match : renderer_data.page_url_matches) {
    URLMatcherIdToRule::const_iterator rule_iter =
        match_id_to_rule_.find(url_match);
    CHECK(rule_iter != match_id_to_rule_.end());

    const ContentRule* rule = rule_iter->second;
    if (is_incognito_renderer) {
      if (!util::IsIncognitoEnabled(rule->extension_id(), browser_context()))
        continue;

      // Split-mode incognito extensions register their rules with separate
      // RulesRegistries per Original/OffTheRecord browser contexts, whereas
      // spanning-mode extensions share the Original browser context.
      const Extension* extension = ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(rule->extension_id(),
                             ExtensionRegistry::EVERYTHING);
      if (util::CanCrossIncognito(extension, browser_context())) {
        // The extension uses spanning mode incognito. No rules should have been
        // registered for the extension in the OffTheRecord registry so
        // execution for that registry should never reach this point.
        CHECK(!browser_context()->IsOffTheRecord());
      } else {
        // The extension uses split mode incognito. Both the Original and
        // OffTheRecord registries may have (separate) rules for this extension.
        // We've established above that we are looking at an incognito renderer,
        // so only the OffTheRecord registry should process its rules.
        if (!browser_context()->IsOffTheRecord())
          continue;
      }
    }

    if (rule->conditions().IsFulfilled(url_match, renderer_data))
      result.insert(rule);
  }
  return result;
}

std::string ChromeContentRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  EvaluationScope evaluation_scope(this);
  const Extension* extension = ExtensionRegistry::Get(browser_context())
      ->GetInstalledExtension(extension_id);
  DCHECK(extension) << "Must have extension with id " << extension_id;

  base::Time extension_installation_time =
      GetExtensionInstallationTime(extension_id);

  std::string error;
  RulesMap new_content_rules;

  for (const linked_ptr<RulesRegistry::Rule>& rule : rules) {
    ContentRule::GlobalRuleId rule_id(extension_id, *rule->id);
    DCHECK(content_rules_.find(rule_id) == content_rules_.end());

    scoped_ptr<ContentRule> content_rule(
        ContentRule::Create(
            page_url_condition_tracker_.condition_factory(),
            browser_context(),
            extension,
            extension_installation_time,
            rule,
            ContentRule::ConsistencyChecker(),
            &error));
    if (!error.empty()) {
      // Clean up temporary condition sets created during rule creation.
      page_url_condition_tracker_.ClearUnusedConditionSets();
      return error;
    }
    DCHECK(content_rule);

    new_content_rules[rule_id] = make_linked_ptr(content_rule.release());
  }

  // Wohoo, everything worked fine.
  content_rules_.insert(new_content_rules.begin(), new_content_rules.end());

  // Create the triggers.
  for (const std::pair<ContentRule::GlobalRuleId,
                       linked_ptr<const ContentRule>>& rule_id_rule_pair :
       new_content_rules) {
    const linked_ptr<const ContentRule>& rule = rule_id_rule_pair.second;
    URLMatcherConditionSet::Vector url_condition_sets;
    const ContentConditionSet& conditions = rule->conditions();
    conditions.GetURLMatcherConditionSets(&url_condition_sets);
    for (const scoped_refptr<URLMatcherConditionSet>& condition_set :
         url_condition_sets) {
      match_id_to_rule_[condition_set->id()] = rule.get();
    }
  }

  // Register url patterns in the URL matcher.
  URLMatcherConditionSet::Vector all_new_condition_sets;
  for (const std::pair<ContentRule::GlobalRuleId,
                       linked_ptr<const ContentRule>>& rule_id_rule_pair :
       new_content_rules) {
    const linked_ptr<const ContentRule>& rule = rule_id_rule_pair.second;
    rule->conditions().GetURLMatcherConditionSets(&all_new_condition_sets);
  }
  page_url_condition_tracker_.AddConditionSets(
      all_new_condition_sets);

  UpdateCssSelectorsFromRules();

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // Ignore evaluation requests in this function because it reverts actions on
  // any active rules itself. Otherwise, we run the risk of reverting the same
  // rule multiple times.
  EvaluationScope evaluation_scope(this, IGNORE_EVALUATION);
  // URLMatcherConditionSet IDs that can be removed from URLMatcher.
  std::vector<URLMatcherConditionSet::ID> condition_set_ids_to_remove;

  for (const std::string& id : rule_identifiers) {
    ContentRule::GlobalRuleId rule_id(extension_id, id);

    // Skip unknown rules.
    RulesMap::iterator content_rules_entry = content_rules_.find(rule_id);
    if (content_rules_entry == content_rules_.end())
      continue;

    // Remove all triggers but collect their IDs.
    URLMatcherConditionSet::Vector condition_sets;
    const ContentRule* rule = content_rules_entry->second.get();
    rule->conditions().GetURLMatcherConditionSets(&condition_sets);
    for (const scoped_refptr<URLMatcherConditionSet>& condition_set :
         condition_sets) {
      condition_set_ids_to_remove.push_back(condition_set->id());
      match_id_to_rule_.erase(condition_set->id());
    }

    // Remove the ContentRule from active_rules_.
    for (auto& tab_rules_pair : active_rules_) {
      if (ContainsKey(tab_rules_pair.second, rule)) {
        ContentAction::ApplyInfo apply_info =
            {browser_context(), tab_rules_pair.first};
        rule->actions().Revert(rule->extension_id(), base::Time(), &apply_info);
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
  for (const std::pair<ContentRule::GlobalRuleId,
                       linked_ptr<const ContentRule>>& rule_id_rule_pair :
       content_rules_) {
    const ContentRule::GlobalRuleId& global_rule_id = rule_id_rule_pair.first;
    if (global_rule_id.first == extension_id)
      rule_identifiers.push_back(global_rule_id.second);
  }

  return RemoveRulesImpl(extension_id, rule_identifiers);
}

void ChromeContentRulesRegistry::UpdateCssSelectorsFromRules() {
  std::set<std::string> css_selectors;  // We rely on this being sorted.
  for (const std::pair<ContentRule::GlobalRuleId,
                       linked_ptr<const ContentRule>>& rule_id_rule_pair :
       content_rules_) {
    const ContentRule& rule = *rule_id_rule_pair.second;
    for (const linked_ptr<const ContentCondition>& condition :
         rule.conditions()) {
      const std::vector<std::string>& condition_css_selectors =
          condition->css_selectors();
      css_selectors.insert(condition_css_selectors.begin(),
                           condition_css_selectors.end());
    }
  }

  css_condition_tracker_.SetWatchedCssSelectors(css_selectors);
}

void ChromeContentRulesRegistry::EvaluateConditionsForTab(
    content::WebContents* tab) {
  extensions::RendererContentMatchData renderer_data;
  page_url_condition_tracker_.GetMatches(tab, &renderer_data.page_url_matches);
  css_condition_tracker_.GetMatchingCssSelectors(tab,
                                                 &renderer_data.css_selectors);
  std::set<const ContentRule*> matching_rules =
      GetMatches(renderer_data, tab->GetBrowserContext()->IsOffTheRecord());
  if (matching_rules.empty() && !ContainsKey(active_rules_, tab))
    return;

  std::set<const ContentRule*>& prev_matching_rules = active_rules_[tab];
  ContentAction::ApplyInfo apply_info = {browser_context(), tab};
  for (const ContentRule* rule : matching_rules) {
    apply_info.priority = rule->priority();
    if (!ContainsKey(prev_matching_rules, rule)) {
      rule->actions().Apply(rule->extension_id(), base::Time(), &apply_info);
    } else {
      rule->actions().Reapply(rule->extension_id(), base::Time(), &apply_info);
    }
  }
  for (const ContentRule* rule : prev_matching_rules) {
    if (!ContainsKey(matching_rules, rule)) {
      apply_info.priority = rule->priority();
      rule->actions().Revert(rule->extension_id(), base::Time(), &apply_info);
    }
  }

  if (matching_rules.empty())
    active_rules_[tab].clear();
  else
    swap(matching_rules, prev_matching_rules);
}

bool ChromeContentRulesRegistry::IsEmpty() const {
  return match_id_to_rule_.empty() && content_rules_.empty() &&
         page_url_condition_tracker_.IsEmpty();
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

base::Time ChromeContentRulesRegistry::GetExtensionInstallationTime(
    const std::string& extension_id) const {
  if (!extension_info_map_.get())  // May be NULL during testing.
    return base::Time();

  return extension_info_map_->GetInstallTime(extension_id);
}

}  // namespace extensions
