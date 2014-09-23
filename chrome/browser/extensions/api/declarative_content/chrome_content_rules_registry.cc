// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/content_condition.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_messages.h"

using url_matcher::URLMatcherConditionSet;

namespace extensions {

ChromeContentRulesRegistry::ChromeContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate)
    : ContentRulesRegistry(browser_context,
                           declarative_content_constants::kOnPageChanged,
                           content::BrowserThread::UI,
                           cache_delegate,
                           WebViewKey(0, 0)) {
  extension_info_map_ = ExtensionSystem::Get(browser_context)->info_map();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void ChromeContentRulesRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      if (process->GetBrowserContext() == browser_context())
        InstructRenderProcess(process);
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      content::WebContents* tab =
          content::Source<content::WebContents>(source).ptr();
      // GetTabId() returns -1 for non-tab WebContents, which won't be
      // in the map.  Similarly, tabs from other browser_contexts won't be in
      // the map.
      active_rules_.erase(ExtensionTabUtil::GetTabId(tab));
      break;
    }
  }
}

void ChromeContentRulesRegistry::Apply(
    content::WebContents* contents,
    const std::vector<std::string>& matching_css_selectors) {
  const int tab_id = ExtensionTabUtil::GetTabId(contents);
  RendererContentMatchData renderer_data;
  renderer_data.page_url_matches = url_matcher_.MatchURL(contents->GetURL());
  renderer_data.css_selectors.insert(matching_css_selectors.begin(),
                                     matching_css_selectors.end());
  std::set<ContentRule*> matching_rules = GetMatches(renderer_data);
  if (matching_rules.empty() && !ContainsKey(active_rules_, tab_id))
    return;

  std::set<ContentRule*>& prev_matching_rules = active_rules_[tab_id];
  ContentAction::ApplyInfo apply_info = {browser_context(), contents};
  for (std::set<ContentRule*>::const_iterator it = matching_rules.begin();
       it != matching_rules.end();
       ++it) {
    apply_info.priority = (*it)->priority();
    if (!ContainsKey(prev_matching_rules, *it)) {
      (*it)->actions().Apply((*it)->extension_id(), base::Time(), &apply_info);
    } else {
      (*it)->actions().Reapply(
          (*it)->extension_id(), base::Time(), &apply_info);
    }
  }
  for (std::set<ContentRule*>::const_iterator it = prev_matching_rules.begin();
       it != prev_matching_rules.end();
       ++it) {
    if (!ContainsKey(matching_rules, *it)) {
      apply_info.priority = (*it)->priority();
      (*it)->actions().Revert((*it)->extension_id(), base::Time(), &apply_info);
    }
  }

  if (matching_rules.empty())
    active_rules_.erase(tab_id);
  else
    swap(matching_rules, prev_matching_rules);
}

void ChromeContentRulesRegistry::DidNavigateMainFrame(
    content::WebContents* contents,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page) {
    // Within-page navigations don't change the set of elements that
    // exist, and we only support filtering on the top-level URL, so
    // this can't change which rules match.
    return;
  }

  // Top-level navigation produces a new document. Initially, the
  // document's empty, so no CSS rules match.  The renderer will send
  // an ExtensionHostMsg_OnWatchedPageChange later if any CSS rules
  // match.
  std::vector<std::string> no_css_selectors;
  Apply(contents, no_css_selectors);
}

std::set<ContentRule*> ChromeContentRulesRegistry::GetMatches(
    const RendererContentMatchData& renderer_data) const {
  std::set<ContentRule*> result;

  // Then we need to check for each of these, whether the other
  // attributes are also fulfilled.
  for (std::set<URLMatcherConditionSet::ID>::iterator url_match =
           renderer_data.page_url_matches.begin();
       url_match != renderer_data.page_url_matches.end();
       ++url_match) {
    URLMatcherIdToRule::const_iterator rule_iter =
        match_id_to_rule_.find(*url_match);
    CHECK(rule_iter != match_id_to_rule_.end());

    ContentRule* rule = rule_iter->second;
    if (rule->conditions().IsFulfilled(*url_match, renderer_data))
      result.insert(rule);
  }
  return result;
}

std::string ChromeContentRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) {
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  DCHECK(extension) << "Must have extension with id " << extension_id;

  base::Time extension_installation_time =
      GetExtensionInstallationTime(extension_id);

  std::string error;
  RulesMap new_content_rules;

  for (std::vector<linked_ptr<RulesRegistry::Rule> >::const_iterator rule =
           rules.begin();
       rule != rules.end();
       ++rule) {
    ContentRule::GlobalRuleId rule_id(extension_id, *(*rule)->id);
    DCHECK(content_rules_.find(rule_id) == content_rules_.end());

    scoped_ptr<ContentRule> content_rule(
        ContentRule::Create(url_matcher_.condition_factory(),
                            browser_context(),
                            extension,
                            extension_installation_time,
                            *rule,
                            ContentRule::ConsistencyChecker(),
                            &error));
    if (!error.empty()) {
      // Clean up temporary condition sets created during rule creation.
      url_matcher_.ClearUnusedConditionSets();
      return error;
    }
    DCHECK(content_rule);

    new_content_rules[rule_id] = make_linked_ptr(content_rule.release());
  }

  // Wohoo, everything worked fine.
  content_rules_.insert(new_content_rules.begin(), new_content_rules.end());

  // Create the triggers.
  for (RulesMap::iterator i = new_content_rules.begin();
       i != new_content_rules.end();
       ++i) {
    URLMatcherConditionSet::Vector url_condition_sets;
    const ContentConditionSet& conditions = i->second->conditions();
    conditions.GetURLMatcherConditionSets(&url_condition_sets);
    for (URLMatcherConditionSet::Vector::iterator j =
             url_condition_sets.begin();
         j != url_condition_sets.end();
         ++j) {
      match_id_to_rule_[(*j)->id()] = i->second.get();
    }
  }

  // Register url patterns in url_matcher_.
  URLMatcherConditionSet::Vector all_new_condition_sets;
  for (RulesMap::iterator i = new_content_rules.begin();
       i != new_content_rules.end();
       ++i) {
    i->second->conditions().GetURLMatcherConditionSets(&all_new_condition_sets);
  }
  url_matcher_.AddConditionSets(all_new_condition_sets);

  UpdateConditionCache();

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  // URLMatcherConditionSet IDs that can be removed from URLMatcher.
  std::vector<URLMatcherConditionSet::ID> remove_from_url_matcher;

  for (std::vector<std::string>::const_iterator i = rule_identifiers.begin();
       i != rule_identifiers.end();
       ++i) {
    ContentRule::GlobalRuleId rule_id(extension_id, *i);

    // Skip unknown rules.
    RulesMap::iterator content_rules_entry = content_rules_.find(rule_id);
    if (content_rules_entry == content_rules_.end())
      continue;

    // Remove all triggers but collect their IDs.
    URLMatcherConditionSet::Vector condition_sets;
    ContentRule* rule = content_rules_entry->second.get();
    rule->conditions().GetURLMatcherConditionSets(&condition_sets);
    for (URLMatcherConditionSet::Vector::iterator j = condition_sets.begin();
         j != condition_sets.end();
         ++j) {
      remove_from_url_matcher.push_back((*j)->id());
      match_id_to_rule_.erase((*j)->id());
    }

    // Remove the ContentRule from active_rules_.
    for (std::map<int, std::set<ContentRule*> >::iterator it =
             active_rules_.begin();
         it != active_rules_.end();
         ++it) {
      if (ContainsKey(it->second, rule)) {
        content::WebContents* tab;
        if (!ExtensionTabUtil::GetTabById(
                it->first, browser_context(), true, NULL, NULL, &tab, NULL)) {
          LOG(DFATAL) << "Tab id " << it->first
                      << " still in active_rules_, but tab has been destroyed";
          continue;
        }
        ContentAction::ApplyInfo apply_info = {browser_context(), tab};
        rule->actions().Revert(rule->extension_id(), base::Time(), &apply_info);
        it->second.erase(rule);
      }
    }

    // Remove reference to actual rule.
    content_rules_.erase(content_rules_entry);
  }

  // Clear URLMatcher based on condition_set_ids that are not needed any more.
  url_matcher_.RemoveConditionSets(remove_from_url_matcher);

  UpdateConditionCache();

  return std::string();
}

std::string ChromeContentRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  // Search all identifiers of rules that belong to extension |extension_id|.
  std::vector<std::string> rule_identifiers;
  for (RulesMap::iterator i = content_rules_.begin(); i != content_rules_.end();
       ++i) {
    const ContentRule::GlobalRuleId& global_rule_id = i->first;
    if (global_rule_id.first == extension_id)
      rule_identifiers.push_back(global_rule_id.second);
  }

  return RemoveRulesImpl(extension_id, rule_identifiers);
}

void ChromeContentRulesRegistry::UpdateConditionCache() {
  std::set<std::string> css_selectors;  // We rely on this being sorted.
  for (RulesMap::const_iterator i = content_rules_.begin();
       i != content_rules_.end();
       ++i) {
    ContentRule& rule = *i->second;
    for (ContentConditionSet::const_iterator condition =
             rule.conditions().begin();
         condition != rule.conditions().end();
         ++condition) {
      const std::vector<std::string>& condition_css_selectors =
          (*condition)->css_selectors();
      css_selectors.insert(condition_css_selectors.begin(),
                           condition_css_selectors.end());
    }
  }

  if (css_selectors.size() != watched_css_selectors_.size() ||
      !std::equal(css_selectors.begin(),
                  css_selectors.end(),
                  watched_css_selectors_.begin())) {
    watched_css_selectors_.assign(css_selectors.begin(), css_selectors.end());

    for (content::RenderProcessHost::iterator it(
             content::RenderProcessHost::AllHostsIterator());
         !it.IsAtEnd();
         it.Advance()) {
      content::RenderProcessHost* process = it.GetCurrentValue();
      if (process->GetBrowserContext() == browser_context())
        InstructRenderProcess(process);
    }
  }
}

void ChromeContentRulesRegistry::InstructRenderProcess(
    content::RenderProcessHost* process) {
  process->Send(new ExtensionMsg_WatchPages(watched_css_selectors_));
}

bool ChromeContentRulesRegistry::IsEmpty() const {
  return match_id_to_rule_.empty() && content_rules_.empty() &&
         url_matcher_.IsEmpty();
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
