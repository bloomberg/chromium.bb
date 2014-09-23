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
#include "components/url_matcher/url_matcher.h"
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
class ChromeContentRulesRegistry : public ContentRulesRegistry,
                                   public content::NotificationObserver {
 public:
  // For testing, |ui_part| can be NULL. In that case it constructs the
  // registry with storage functionality suspended.
  ChromeContentRulesRegistry(content::BrowserContext* browser_context,
                             RulesCacheDelegate* cache_delegate);

  // ChromeContentRulesRegistry implementation:
  // Applies all content rules given an update (CSS match change or
  // page navigation, for now) from the renderer.
  virtual void Apply(
      content::WebContents* contents,
      const std::vector<std::string>& matching_css_selectors) OVERRIDE;

  // Applies all content rules given that a tab was just navigated.
  virtual void DidNavigateMainFrame(
      content::WebContents* tab,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Implementation of RulesRegistry:
  virtual std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<RulesRegistry::Rule> >& rules) OVERRIDE;
  virtual std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) OVERRIDE;
  virtual std::string RemoveAllRulesImpl(
      const std::string& extension_id) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 protected:
  virtual ~ChromeContentRulesRegistry();

  // Virtual for testing:
  virtual base::Time GetExtensionInstallationTime(
      const std::string& extension_id) const;

 private:
  friend class DeclarativeChromeContentRulesRegistryTest;

  std::set<ContentRule*> GetMatches(
      const RendererContentMatchData& renderer_data) const;

  // Scans the rules for the set of conditions they're watching.  If the set has
  // changed, calls InstructRenderProcess() for each RenderProcessHost in the
  // current browser_context.
  void UpdateConditionCache();

  // Tells a renderer what page attributes to watch for using an
  // ExtensionMsg_WatchPages.
  void InstructRenderProcess(content::RenderProcessHost* process);

  typedef std::map<url_matcher::URLMatcherConditionSet::ID, ContentRule*>
      URLMatcherIdToRule;
  typedef std::map<ContentRule::GlobalRuleId, linked_ptr<ContentRule> >
      RulesMap;

  // Map that tells us which ContentRules may match under the condition that
  // the URLMatcherConditionSet::ID was returned by the |url_matcher_|.
  URLMatcherIdToRule match_id_to_rule_;

  RulesMap content_rules_;

  // Maps tab_id to the set of rules that match on that tab.  This
  // lets us call Revert as appropriate.
  std::map<int, std::set<ContentRule*> > active_rules_;

  // Matches URLs for the page_url condition.
  url_matcher::URLMatcher url_matcher_;

  // All CSS selectors any rule's conditions watch for.
  std::vector<std::string> watched_css_selectors_;

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  scoped_refptr<InfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentRulesRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_
