// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"

#include <algorithm>
#include <iterator>
#include <tuple>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/version_info/channel.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature_channel.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;
using PageAccess = PermissionsData::PageAccess;

void NotifyRequestWithheld(const ExtensionId& extension_id,
                           const WebRequestInfo& request) {
  DCHECK(ExtensionsAPIClient::Get());
  ExtensionsAPIClient::Get()->NotifyWebRequestWithheld(
      request.render_process_id, request.frame_id, extension_id);
}

// Helper to log the time taken in RulesetManager::EvaluateRequestInternal.
class ScopedEvaluateRequestTimer {
 public:
  ScopedEvaluateRequestTimer() = default;
  ~ScopedEvaluateRequestTimer() {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions3",
        timer_.Elapsed(), base::TimeDelta::FromMicroseconds(1),
        base::TimeDelta::FromMilliseconds(50), 50);
  }

 private:
  base::ElapsedTimer timer_;
};

}  // namespace

RulesetManager::RulesetManager(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      prefs_(ExtensionPrefs::Get(browser_context)),
      permission_helper_(PermissionHelper::Get(browser_context)) {
  DCHECK(browser_context_);

  // RulesetManager can be created on any sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RulesetManager::~RulesetManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RulesetManager::AddRuleset(const ExtensionId& extension_id,
                                std::unique_ptr<CompositeMatcher> matcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  bool inserted =
      rulesets_
          .emplace(extension_id, prefs_->GetInstallTime(extension_id),
                   std::move(matcher))
          .second;
  DCHECK(inserted) << "AddRuleset called twice in succession for "
                   << extension_id;

  if (test_observer_)
    test_observer_->OnRulesetCountChanged(rulesets_.size());

  // Clear the renderers' cache so that they take the new rules into account.
  ClearRendererCacheOnNavigation();
}

void RulesetManager::RemoveRuleset(const ExtensionId& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  auto compare_by_id =
      [&extension_id](const ExtensionRulesetData& ruleset_data) {
        return ruleset_data.extension_id == extension_id;
      };

  size_t erased_count = base::EraseIf(rulesets_, compare_by_id);
  DCHECK_EQ(1u, erased_count)
      << "RemoveRuleset called without a corresponding AddRuleset for "
      << extension_id;

  if (test_observer_)
    test_observer_->OnRulesetCountChanged(rulesets_.size());

  // Clear the renderers' cache so that they take the removed rules into
  // account.
  ClearRendererCacheOnNavigation();
}

std::set<ExtensionId> RulesetManager::GetExtensionsWithRulesets() const {
  std::set<ExtensionId> extension_ids;
  for (const ExtensionRulesetData& data : rulesets_)
    extension_ids.insert(data.extension_id);
  return extension_ids;
}

CompositeMatcher* RulesetManager::GetMatcherForExtension(
    const ExtensionId& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  // This is O(n) but it's ok since the number of extensions will be small and
  // we have to maintain the rulesets sorted in decreasing order of installation
  // time.
  auto iter =
      std::find_if(rulesets_.begin(), rulesets_.end(),
                   [&extension_id](const ExtensionRulesetData& ruleset) {
                     return ruleset.extension_id == extension_id;
                   });

  // There must be ExtensionRulesetData corresponding to this |extension_id|.
  if (iter == rulesets_.end())
    return nullptr;

  DCHECK(iter->matcher);
  return iter->matcher.get();
}

const std::vector<RequestAction>& RulesetManager::EvaluateRequest(
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: it's safe to cache the action on WebRequestInfo without worrying
  // about |is_incognito_context| since a WebRequestInfo object will not be
  // shared between different contexts. Hence the value of
  // |is_incognito_context| will stay the same for a given |request|. This also
  // assumes that the core state of the WebRequestInfo isn't changed between the
  // different EvaluateRequest invocations.
  if (!request.dnr_actions) {
    request.dnr_actions =
        EvaluateRequestInternal(request, is_incognito_context);
  }

  return *request.dnr_actions;
}

bool RulesetManager::HasAnyExtraHeadersMatcher() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& ruleset : rulesets_) {
    if (ruleset.matcher->HasAnyExtraHeadersMatcher())
      return true;
  }

  return false;
}

bool RulesetManager::HasExtraHeadersMatcherForRequest(
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  const std::vector<RequestAction>& actions =
      EvaluateRequest(request, is_incognito_context);

  static_assert(flat::ActionType_count == 6,
                "Modify this method to ensure HasExtraHeadersMatcherForRequest "
                "is updated as new actions are added.");

  return std::any_of(
      actions.begin(), actions.end(), [](const RequestAction& action) {
        return action.type == RequestAction::Type::MODIFY_HEADERS;
      });
}

void RulesetManager::OnRenderFrameCreated(content::RenderFrameHost* host) {
  for (ExtensionRulesetData& ruleset : rulesets_)
    ruleset.matcher->OnRenderFrameCreated(host);
}

void RulesetManager::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  for (ExtensionRulesetData& ruleset : rulesets_)
    ruleset.matcher->OnRenderFrameDeleted(host);
}

void RulesetManager::OnDidFinishNavigation(content::RenderFrameHost* host) {
  for (ExtensionRulesetData& ruleset : rulesets_)
    ruleset.matcher->OnDidFinishNavigation(host);
}

void RulesetManager::SetObserverForTest(TestObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!(test_observer_ && observer))
      << "Multiple test observers are not supported";
  test_observer_ = observer;
}

RulesetManager::ExtensionRulesetData::ExtensionRulesetData(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time,
    std::unique_ptr<CompositeMatcher> matcher)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      matcher(std::move(matcher)) {}
RulesetManager::ExtensionRulesetData::~ExtensionRulesetData() = default;
RulesetManager::ExtensionRulesetData::ExtensionRulesetData(
    ExtensionRulesetData&& other) = default;
RulesetManager::ExtensionRulesetData& RulesetManager::ExtensionRulesetData::
operator=(ExtensionRulesetData&& other) = default;

bool RulesetManager::ExtensionRulesetData::operator<(
    const ExtensionRulesetData& other) const {
  // Sort based on *descending* installation time, using extension id to break
  // ties.
  return std::tie(extension_install_time, extension_id) >
         std::tie(other.extension_install_time, other.extension_id);
}

base::Optional<RequestAction> RulesetManager::GetBeforeRequestAction(
    const std::vector<RulesetAndPageAccess>& rulesets,
    const WebRequestInfo& request,
    const RequestParams& params) const {
  DCHECK(std::is_sorted(rulesets.begin(), rulesets.end(),
                        [](RulesetAndPageAccess a, RulesetAndPageAccess b) {
                          return *a.first < *b.first;
                        }));

  // The priorities of actions between different extensions is different from
  // the priorities of actions within an extension.
  const auto action_priority = [](const base::Optional<RequestAction>& action) {
    if (!action.has_value())
      return 0;
    switch (action->type) {
      case RequestAction::Type::BLOCK:
      case RequestAction::Type::COLLAPSE:
        return 3;
      case RequestAction::Type::REDIRECT:
      case RequestAction::Type::UPGRADE:
        return 2;
      case RequestAction::Type::ALLOW:
      case RequestAction::Type::ALLOW_ALL_REQUESTS:
        return 1;
      case RequestAction::Type::MODIFY_HEADERS:
        NOTREACHED();
        return 0;
    }
  };

  base::Optional<RequestAction> action;

  // This iterates in decreasing order of extension installation time. Hence
  // more recently installed extensions get higher priority in choosing the
  // action for the request.
  for (const RulesetAndPageAccess& ruleset_and_access : rulesets) {
    const ExtensionRulesetData* ruleset = ruleset_and_access.first;

    CompositeMatcher::ActionInfo action_info =
        ruleset->matcher->GetBeforeRequestAction(params,
                                                 ruleset_and_access.second);

    DCHECK(!(action_info.action && action_info.notify_request_withheld));
    if (action_info.notify_request_withheld) {
      NotifyRequestWithheld(ruleset->extension_id, request);
      continue;
    }

    if (action_priority(action_info.action) > action_priority(action))
      action = std::move(action_info.action);
  }

  return action;
}

std::vector<RequestAction> RulesetManager::GetModifyHeadersActions(
    const std::vector<RulesetAndPageAccess>& rulesets,
    const WebRequestInfo& request,
    const RequestParams& params) const {
  DCHECK(std::is_sorted(rulesets.begin(), rulesets.end(),
                        [](RulesetAndPageAccess a, RulesetAndPageAccess b) {
                          return *a.first < *b.first;
                        }));

  std::vector<RequestAction> modify_headers_actions;

  for (const RulesetAndPageAccess& ruleset_and_access : rulesets) {
    PageAccess page_access = ruleset_and_access.second;
    // Skip the evaluation of modifyHeaders rules for this extension if its
    // access to the request is denied.
    if (page_access == PageAccess::kDenied)
      continue;

    const ExtensionRulesetData* ruleset = ruleset_and_access.first;
    std::vector<RequestAction> actions_for_matcher =
        ruleset->matcher->GetModifyHeadersActions(params);

    // Evaluate modifyHeaders rules for this extension if and only if it has
    // host permissions for the request url and initiator.
    if (page_access == PageAccess::kAllowed) {
      modify_headers_actions.insert(
          modify_headers_actions.end(),
          std::make_move_iterator(actions_for_matcher.begin()),
          std::make_move_iterator(actions_for_matcher.end()));
    } else if (page_access == PageAccess::kWithheld &&
               !actions_for_matcher.empty()) {
      // Notify the extension that it could not modify the request's headers if
      // it had at least one matching modifyHeaders rule and its access to the
      // request was withheld.
      NotifyRequestWithheld(ruleset->extension_id, request);
    }
  }

  // |modify_headers_actions| is implicitly sorted in descreasing order by
  // priority.
  //  - Within an extension: each CompositeMatcher returns a vector sorted by
  //  priority.
  //  - Between extensions: |rulesets| is sorder in descending order of
  //  extension priority.
  return modify_headers_actions;
}

std::vector<RequestAction> RulesetManager::EvaluateRequestInternal(
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!request.dnr_actions);

  std::vector<RequestAction> actions;

  if (!ShouldEvaluateRequest(request))
    return actions;

  if (test_observer_)
    test_observer_->OnEvaluateRequest(request, is_incognito_context);

  if (rulesets_.empty())
    return actions;

  ScopedEvaluateRequestTimer timer;

  const RequestParams params(request);
  const int tab_id = request.frame_data.tab_id;

  // |crosses_incognito| is used to ensure that a split mode extension process
  // can't intercept requests from a cross browser context. Since declarative
  // net request API doesn't use event listeners in a background process, it is
  // irrelevant here.
  const bool crosses_incognito = false;

  // Filter the rulesets to evaluate along with their host permissions based
  // page access for the current request being evaluated.
  std::vector<RulesetAndPageAccess> rulesets_to_evaluate;
  for (const ExtensionRulesetData& ruleset : rulesets_) {
    if (!ShouldEvaluateRulesetForRequest(ruleset, request,
                                         is_incognito_context)) {
      continue;
    }

    // If the extension doesn't have permission to the request, then skip this
    // ruleset. Note: we are not checking for host permissions here.
    // DO_NOT_CHECK_HOST is strictly less restrictive than
    // REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR.
    PageAccess page_access = WebRequestPermissions::CanExtensionAccessURL(
        permission_helper_, ruleset.extension_id, request.url, tab_id,
        crosses_incognito, WebRequestPermissions::DO_NOT_CHECK_HOST,
        request.initiator, request.type);
    DCHECK_NE(PageAccess::kWithheld, page_access);
    if (page_access != PageAccess::kAllowed)
      continue;

    // Precompute the host permissions access the extension has for this
    // request.
    PageAccess host_permissions_access =
        WebRequestPermissions::CanExtensionAccessURL(
            permission_helper_, ruleset.extension_id, request.url, tab_id,
            crosses_incognito,
            WebRequestPermissions::
                REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
            request.initiator, request.type);

    rulesets_to_evaluate.push_back(
        std::make_pair(&ruleset, host_permissions_access));
  }

  // If the request is blocked/allowed/redirected, no further modifications can
  // happen. A new request will be created and subsequently evaluated.
  base::Optional<RequestAction> action =
      GetBeforeRequestAction(rulesets_to_evaluate, request, params);
  if (action) {
    actions.push_back(std::move(std::move(*action)));
    return actions;
  }

  // TODO(crbug.com/947591): Remove the channel check once implementation of
  // modifyHeaders action is complete.
  if (GetCurrentChannel() != version_info::Channel::STABLE) {
    std::vector<RequestAction> modify_headers_actions =
        GetModifyHeadersActions(rulesets_to_evaluate, request, params);

    if (!modify_headers_actions.empty())
      return modify_headers_actions;
  }

  return actions;
}

bool RulesetManager::ShouldEvaluateRequest(
    const WebRequestInfo& request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensure clients filter out sensitive requests.
  DCHECK(!WebRequestPermissions::HideRequest(permission_helper_, request));

  if (!IsAPIAvailable()) {
    DCHECK(rulesets_.empty());
    return false;
  }

  // Prevent extensions from modifying any resources on the chrome-extension
  // scheme. Practically, this has the effect of not allowing an extension to
  // modify its own resources (The extension wouldn't have the permission to
  // other extension origins anyway).
  if (request.url.SchemeIs(kExtensionScheme))
    return false;

  return true;
}

bool RulesetManager::ShouldEvaluateRulesetForRequest(
    const ExtensionRulesetData& ruleset,
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  // Only extensions enabled in incognito should have access to requests in an
  // incognito context.
  if (is_incognito_context &&
      !util::IsIncognitoEnabled(ruleset.extension_id, browser_context_)) {
    return false;
  }

  return true;
}

}  // namespace declarative_net_request
}  // namespace extensions
