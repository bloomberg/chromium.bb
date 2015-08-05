// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

namespace {

const char kInvalidTypeOfParameter[] = "Attribute '%s' has an invalid type";

}  // namespace

//
// DeclarativeContentCssPredicate
//

DeclarativeContentCssPredicate::~DeclarativeContentCssPredicate() {
}

bool DeclarativeContentCssPredicate::Evaluate(
    const base::hash_set<std::string>& matched_css_selectors) const {
  // All attributes must be fulfilled for a fulfilled condition.
  for (const std::string& css_selector : css_selectors_) {
    if (!ContainsKey(matched_css_selectors, css_selector))
      return false;
  }

  return true;
}

// static
scoped_ptr<DeclarativeContentCssPredicate>
DeclarativeContentCssPredicate::Create(
    const base::Value& value,
    std::string* error) {
  std::vector<std::string> css_rules;
  const base::ListValue* css_rules_value = nullptr;
  if (value.GetAsList(&css_rules_value)) {
    for (size_t i = 0; i < css_rules_value->GetSize(); ++i) {
      std::string css_rule;
      if (!css_rules_value->GetString(i, &css_rule)) {
        *error = base::StringPrintf(kInvalidTypeOfParameter,
                                    declarative_content_constants::kCss);
        return scoped_ptr<DeclarativeContentCssPredicate>();
      }
      css_rules.push_back(css_rule);
    }
  } else {
    *error = base::StringPrintf(kInvalidTypeOfParameter,
                                declarative_content_constants::kCss);
    return scoped_ptr<DeclarativeContentCssPredicate>();
  }

  return !css_rules.empty() ?
      make_scoped_ptr(new DeclarativeContentCssPredicate(css_rules)) :
      scoped_ptr<DeclarativeContentCssPredicate>();
}

DeclarativeContentCssPredicate::DeclarativeContentCssPredicate(
    const std::vector<std::string>& css_selectors)
    : css_selectors_(css_selectors) {
  DCHECK(!css_selectors.empty());
}

//
// PerWebContentsTracker
//

DeclarativeContentCssConditionTracker::PerWebContentsTracker::
PerWebContentsTracker(
    content::WebContents* contents,
    const RequestEvaluationCallback& request_evaluation,
    const WebContentsDestroyedCallback& web_contents_destroyed)
    : WebContentsObserver(contents),
      request_evaluation_(request_evaluation),
      web_contents_destroyed_(web_contents_destroyed) {
}

DeclarativeContentCssConditionTracker::PerWebContentsTracker::
~PerWebContentsTracker() {
}

scoped_ptr<DeclarativeContentCssPredicate>
DeclarativeContentCssConditionTracker::CreatePredicate(
    const Extension* extension,
    const base::Value& value,
    std::string* error) {
  return DeclarativeContentCssPredicate::Create(value, error);
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnWebContentsNavigation(const content::LoadCommittedDetails& details,
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
  matching_css_selectors_.clear();
  request_evaluation_.Run(web_contents());
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
UpdateMatchingCssSelectorsForTesting(
    const std::vector<std::string>& matching_css_selectors) {
  matching_css_selectors_ = matching_css_selectors;
  request_evaluation_.Run(web_contents());
}

bool
DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PerWebContentsTracker, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OnWatchedPageChange,
                        OnWatchedPageChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeclarativeContentCssConditionTracker::PerWebContentsTracker::
WebContentsDestroyed() {
  web_contents_destroyed_.Run(web_contents());
}

void
DeclarativeContentCssConditionTracker::PerWebContentsTracker::
OnWatchedPageChange(
    const std::vector<std::string>& css_selectors) {
  matching_css_selectors_ = css_selectors;
  request_evaluation_.Run(web_contents());
}

//
// DeclarativeContentCssConditionTracker
//

DeclarativeContentCssConditionTracker::DeclarativeContentCssConditionTracker(
    content::BrowserContext* context,
    DeclarativeContentConditionTrackerDelegate* delegate)
    : context_(context),
      delegate_(delegate) {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

DeclarativeContentCssConditionTracker::
~DeclarativeContentCssConditionTracker() {}

// We use the sorted propery of the set for equality checks with
// watched_css_selectors_, which is guaranteed to be sorted because it's set
// from the set contents.
void DeclarativeContentCssConditionTracker::SetWatchedCssSelectors(
    const std::set<std::string>& new_watched_css_selectors) {
  if (new_watched_css_selectors.size() != watched_css_selectors_.size() ||
      !std::equal(new_watched_css_selectors.begin(),
                  new_watched_css_selectors.end(),
                  watched_css_selectors_.begin())) {
    watched_css_selectors_.assign(new_watched_css_selectors.begin(),
                                  new_watched_css_selectors.end());

    for (content::RenderProcessHost::iterator it(
             content::RenderProcessHost::AllHostsIterator());
         !it.IsAtEnd();
         it.Advance()) {
      InstructRenderProcessIfManagingBrowserContext(it.GetCurrentValue());
    }
  }
}

void DeclarativeContentCssConditionTracker::TrackForWebContents(
    content::WebContents* contents) {
  per_web_contents_tracker_[contents] =
      make_linked_ptr(new PerWebContentsTracker(
          contents,
          base::Bind(&DeclarativeContentConditionTrackerDelegate::
                     RequestEvaluation,
                     base::Unretained(delegate_)),
          base::Bind(&DeclarativeContentCssConditionTracker::
                     DeletePerWebContentsTracker,
                     base::Unretained(this))));
  // Note: the condition is always false until we receive OnWatchedPageChange,
  // so there's no need to evaluate it here.
}

void DeclarativeContentCssConditionTracker::OnWebContentsNavigation(
    content::WebContents* contents,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  per_web_contents_tracker_[contents]->OnWebContentsNavigation(details, params);
}

void DeclarativeContentCssConditionTracker::GetMatchingCssSelectors(
    content::WebContents* contents,
    base::hash_set<std::string>* css_selectors) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  const std::vector<std::string>& matching_css_selectors =
      per_web_contents_tracker_[contents]->matching_css_selectors();
  css_selectors->insert(matching_css_selectors.begin(),
                        matching_css_selectors.end());
}

void DeclarativeContentCssConditionTracker::
UpdateMatchingCssSelectorsForTesting(
    content::WebContents* contents,
    const std::vector<std::string>& matching_css_selectors) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  per_web_contents_tracker_[contents]->
      UpdateMatchingCssSelectorsForTesting(matching_css_selectors);
}

void DeclarativeContentCssConditionTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      InstructRenderProcessIfManagingBrowserContext(process);
      break;
    }
  }
}

void DeclarativeContentCssConditionTracker::
InstructRenderProcessIfManagingBrowserContext(
    content::RenderProcessHost* process) {
  if (delegate_->ShouldManageConditionsForBrowserContext(
          process->GetBrowserContext())) {
    process->Send(new ExtensionMsg_WatchPages(watched_css_selectors_));
  }
}

void DeclarativeContentCssConditionTracker::DeletePerWebContentsTracker(
    content::WebContents* contents) {
  DCHECK(ContainsKey(per_web_contents_tracker_, contents));
  per_web_contents_tracker_.erase(contents);
}

}  // namespace extensions
