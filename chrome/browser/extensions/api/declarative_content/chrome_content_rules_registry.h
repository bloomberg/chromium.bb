// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/content_condition.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/declarative/declarative_rule.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/info_map.h"

class ContentPermissions;

namespace content {
class BrowserContext;
class RenderProcessHost;
class WebContents;
struct FrameNavigateParams;
struct LoadCommittedDetails;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace net {
class URLRequest;
}

namespace extensions {

class RulesRegistryService;

typedef DeclarativeRule<ContentCondition, ContentAction> ContentRule;

// The ChromeContentRulesRegistry is responsible for managing
// the internal representation of rules for the Declarative Content API.
//
// Here is the high level overview of this functionality:
//
// RulesRegistry::Rule consists of Conditions and Actions, these are
// represented as a ContentRule with ContentConditions and
// ContentRuleActions.
//
// The evaluation of URL related condition attributes (host_suffix, path_prefix)
// is delegated to a URLMatcher, because this is capable of evaluating many
// of such URL related condition attributes in parallel.
//
// A note on incognito support: separate instances of ChromeContentRulesRegistry
// are created for incognito and non-incognito contexts. The incognito instance,
// however, is only responsible for applying rules registered by the incognito
// side of split-mode extensions to incognito tabs. The non-incognito instance
// handles incognito tabs for spanning-mode extensions, plus all non-incognito
// tabs.
class ChromeContentRulesRegistry
    : public ContentRulesRegistry,
      public content::NotificationObserver,
      public DeclarativeContentConditionTrackerDelegate {
 public:
  // For testing, |ui_part| can be NULL. In that case it constructs the
  // registry with storage functionality suspended.
  ChromeContentRulesRegistry(content::BrowserContext* browser_context,
                             RulesCacheDelegate* cache_delegate);

  // ContentRulesRegistry:
  void MonitorWebContentsForRuleEvaluation(
      content::WebContents* contents) override;
  void DidNavigateMainFrame(
      content::WebContents* tab,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // RulesRegistry:
  std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule>>& rules) override;
  std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) override;
  std::string RemoveAllRulesImpl(const std::string& extension_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // DeclarativeContentConditionTrackerDelegate:
  void RequestEvaluation(content::WebContents* contents) override;
  bool ShouldManageConditionsForBrowserContext(
      content::BrowserContext* context) override;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

  // TODO(wittman): Remove once DeclarativeChromeContentRulesRegistry no longer
  // depends on concrete condition implementations. At that point
  // DeclarativeChromeContentRulesRegistryTest.ActiveRulesDoesntGrow will be
  // able to use a test condition object and not need to depend on force setting
  // matching CSS seleectors.
  void UpdateMatchingCssSelectorsForTesting(
      content::WebContents* contents,
      const std::vector<std::string>& matching_css_selectors);

  // Returns the number of active rules.
  size_t GetActiveRulesCountForTesting();

 protected:
  ~ChromeContentRulesRegistry() override;

  // Virtual for testing:
  virtual base::Time GetExtensionInstallationTime(
      const std::string& extension_id) const;

 private:
  // Specifies what to do with evaluation requests.
  // TODO(wittman): Try to eliminate the need for IGNORE after refactoring to
  // treat all condition evaluation consistently. Currently RemoveRulesImpl only
  // updates the CSS selectors after the rules are removed, which is too late
  // for evaluation.
  enum EvaluationDisposition {
    EVALUATE_REQUESTS,  // Evaluate immediately.
    DEFER_REQUESTS,  // Defer for later evaluation.
    IGNORE_REQUESTS  // Ignore.
  };

  class EvaluationScope;

  // True if this object is managing the rules for |context|.
  bool ManagingRulesForBrowserContext(content::BrowserContext* context);

  std::set<const ContentRule*> GetMatches(
      const RendererContentMatchData& renderer_data,
      bool is_incognito_renderer) const;

  // Updates the condition evaluator with the current watched CSS selectors.
  void UpdateCssSelectorsFromRules();

  // Evaluates the conditions for |tab| based on the tab state and matching CSS
  // selectors.
  void EvaluateConditionsForTab(content::WebContents* tab);

  // Evaluates the conditions for tabs in each browser window.
  void EvaluateConditionsForAllTabs();

  typedef std::map<url_matcher::URLMatcherConditionSet::ID, const ContentRule*>
      URLMatcherIdToRule;
  typedef std::map<ContentRule::GlobalRuleId, linked_ptr<const ContentRule>>
      RulesMap;

  // Map that tells us which ContentRules may match under the condition that
  // the URLMatcherConditionSet::ID was returned by the |url_matcher_|.
  URLMatcherIdToRule match_id_to_rule_;

  RulesMap content_rules_;

  // Maps a WebContents to the set of rules that match on that WebContents.
  // This lets us call Revert as appropriate. Note that this is expected to have
  // a key-value pair for every WebContents the registry is tracking, even if
  // the value is the empty set.
  std::map<content::WebContents*, std::set<const ContentRule*>> active_rules_;

  // Responsible for tracking declarative content page URL condition state.
  DeclarativeContentPageUrlConditionTracker page_url_condition_tracker_;

  // Responsible for tracking declarative content CSS condition state.
  DeclarativeContentCssConditionTracker css_condition_tracker_;

  // Specifies what to do with evaluation requests.
  EvaluationDisposition evaluation_disposition_;

  // Contains WebContents which require rule evaluation. Only used while
  // |evaluation_disposition_| is DEFER.
  std::set<content::WebContents*> evaluation_pending_;

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  scoped_refptr<InfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentRulesRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_
