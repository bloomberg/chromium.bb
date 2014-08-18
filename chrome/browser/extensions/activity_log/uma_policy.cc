// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/uma_policy.h"

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/ad_network_database.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

// For convenience.
const int kNoStatus           = UmaPolicy::NONE;
const int kContentScript      = 1 << UmaPolicy::CONTENT_SCRIPT;
const int kReadDom            = 1 << UmaPolicy::READ_DOM;
const int kModifiedDom        = 1 << UmaPolicy::MODIFIED_DOM;
const int kDomMethod          = 1 << UmaPolicy::DOM_METHOD;
const int kDocumentWrite      = 1 << UmaPolicy::DOCUMENT_WRITE;
const int kInnerHtml          = 1 << UmaPolicy::INNER_HTML;
const int kCreatedScript      = 1 << UmaPolicy::CREATED_SCRIPT;
const int kCreatedIframe      = 1 << UmaPolicy::CREATED_IFRAME;
const int kCreatedDiv         = 1 << UmaPolicy::CREATED_DIV;
const int kCreatedLink        = 1 << UmaPolicy::CREATED_LINK;
const int kCreatedInput       = 1 << UmaPolicy::CREATED_INPUT;
const int kCreatedEmbed       = 1 << UmaPolicy::CREATED_EMBED;
const int kCreatedObject      = 1 << UmaPolicy::CREATED_OBJECT;
const int kAdInjected         = 1 << UmaPolicy::AD_INJECTED;
const int kAdRemoved          = 1 << UmaPolicy::AD_REMOVED;
const int kAdReplaced         = 1 << UmaPolicy::AD_REPLACED;
const int kAdLikelyInjected   = 1 << UmaPolicy::AD_LIKELY_INJECTED;
const int kAdLikelyReplaced   = 1 << UmaPolicy::AD_LIKELY_REPLACED;

// A mask of all the ad injection flags.
const int kAnyAdActivity = kAdInjected |
                           kAdRemoved |
                           kAdReplaced |
                           kAdLikelyInjected |
                           kAdLikelyReplaced;

}  // namespace

// Class constants, also used in testing. --------------------------------------

const char UmaPolicy::kNumberOfTabs[]       = "num_tabs";
const size_t UmaPolicy::kMaxTabsTracked     = 50;

// Setup and shutdown. ---------------------------------------------------------

UmaPolicy::UmaPolicy(Profile* profile)
    : ActivityLogPolicy(profile), profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
  BrowserList::AddObserver(this);
}

UmaPolicy::~UmaPolicy() {
  BrowserList::RemoveObserver(this);
}

// Unlike the other policies, UmaPolicy can commit suicide directly because it
// doesn't have a dependency on a database.
void UmaPolicy::Close() {
  delete this;
}

// Process actions. ------------------------------------------------------------

void UmaPolicy::ProcessAction(scoped_refptr<Action> action) {
  if (!action->page_url().is_valid() && !action->arg_url().is_valid())
    return;
  if (action->page_incognito() || action->arg_incognito())
    return;
  std::string url;
  int status = MatchActionToStatus(action);
  if (action->page_url().is_valid()) {
    url = CleanURL(action->page_url());
  } else if (status & kContentScript) {
    // This is for the tabs.executeScript case.
    url = CleanURL(action->arg_url());
  }
  if (url.empty())
    return;

  SiteMap::iterator site_lookup = url_status_.find(url);
  if (site_lookup != url_status_.end())
    site_lookup->second[action->extension_id()] |= status;
}

int UmaPolicy::MatchActionToStatus(scoped_refptr<Action> action) {
  if (action->action_type() == Action::ACTION_CONTENT_SCRIPT)
    return kContentScript;
  if (action->action_type() == Action::ACTION_API_CALL &&
      action->api_name() == "tabs.executeScript")
    return kContentScript;
  if (action->action_type() != Action::ACTION_DOM_ACCESS)
    return kNoStatus;

  int dom_verb = DomActionType::MODIFIED;
  if (!action->other() ||
      !action->other()->GetIntegerWithoutPathExpansion(
          activity_log_constants::kActionDomVerb, &dom_verb))
    return kNoStatus;

  int ret_bit = kNoStatus;
  DomActionType::Type dom_type = static_cast<DomActionType::Type>(dom_verb);
  if (dom_type == DomActionType::GETTER)
    return kReadDom;
  if (dom_type == DomActionType::SETTER)
    ret_bit |= kModifiedDom;
  else if (dom_type == DomActionType::METHOD)
    ret_bit |= kDomMethod;
  else
    return kNoStatus;

  if (action->api_name() == "HTMLDocument.write" ||
      action->api_name() == "HTMLDocument.writeln") {
    ret_bit |= kDocumentWrite;
  } else if (action->api_name() == "Element.innerHTML") {
    ret_bit |= kInnerHtml;
  } else if (action->api_name() == "Document.createElement") {
    std::string arg;
    action->args()->GetString(0, &arg);
    if (arg == "script")
      ret_bit |= kCreatedScript;
    else if (arg == "iframe")
      ret_bit |= kCreatedIframe;
    else if (arg == "div")
      ret_bit |= kCreatedDiv;
    else if (arg == "a")
      ret_bit |= kCreatedLink;
    else if (arg == "input")
      ret_bit |= kCreatedInput;
    else if (arg == "embed")
      ret_bit |= kCreatedEmbed;
    else if (arg == "object")
      ret_bit |= kCreatedObject;
  }

  const Action::InjectionType ad_injection =
      action->DidInjectAd(g_browser_process->rappor_service());
  switch (ad_injection) {
    case Action::INJECTION_NEW_AD:
      ret_bit |= kAdInjected;
      break;
    case Action::INJECTION_REMOVED_AD:
      ret_bit |= kAdRemoved;
      break;
    case Action::INJECTION_REPLACED_AD:
      ret_bit |= kAdReplaced;
      break;
    case Action::INJECTION_LIKELY_NEW_AD:
      ret_bit |= kAdLikelyInjected;
      break;
    case Action::INJECTION_LIKELY_REPLACED_AD:
      ret_bit |= kAdLikelyReplaced;
      break;
    case Action::NO_AD_INJECTION:
      break;
    case Action::NUM_INJECTION_TYPES:
      NOTREACHED();
  }

  return ret_bit;
}

void UmaPolicy::HistogramOnClose(const std::string& cleaned_url,
                                 content::WebContents* web_contents) {
  // Let's try to avoid histogramming useless URLs.
  if (cleaned_url.empty() || cleaned_url == url::kAboutBlankURL ||
      cleaned_url == chrome::kChromeUINewTabURL)
    return;

  int statuses[MAX_STATUS - 1];
  std::memset(statuses, 0, sizeof(statuses));

  ActiveScriptController* active_script_controller =
      ActiveScriptController::GetForWebContents(web_contents);
  SiteMap::iterator site_lookup = url_status_.find(cleaned_url);
  const ExtensionMap& exts = site_lookup->second;
  std::set<std::string> ad_injectors;
  for (ExtensionMap::const_iterator ext_iter = exts.begin();
       ext_iter != exts.end();
       ++ext_iter) {
    if (ext_iter->first == kNumberOfTabs)
      continue;
    for (int i = NONE + 1; i < MAX_STATUS; ++i) {
      if (ext_iter->second & (1 << i))
        statuses[i-1]++;
    }

    if (ext_iter->second & kAnyAdActivity)
      ad_injectors.insert(ext_iter->first);
  }
  if (active_script_controller)
    active_script_controller->OnAdInjectionDetected(ad_injectors);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  for (std::set<std::string>::const_iterator iter = ad_injectors.begin();
       iter != ad_injectors.end();
       ++iter) {
    const Extension* extension =
        registry->GetExtensionById(*iter, ExtensionRegistry::EVERYTHING);
    if (extension) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.AdInjection.InstallLocation",
                                extension->location(),
                                Manifest::NUM_LOCATIONS);
    }
  }

  std::string prefix = "ExtensionActivity.";
  if (GURL(cleaned_url).host() != "www.google.com") {
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CONTENT_SCRIPT),
                             statuses[CONTENT_SCRIPT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(READ_DOM),
                             statuses[READ_DOM - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(MODIFIED_DOM),
                             statuses[MODIFIED_DOM - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(DOM_METHOD),
                             statuses[DOM_METHOD - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(DOCUMENT_WRITE),
                             statuses[DOCUMENT_WRITE - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(INNER_HTML),
                             statuses[INNER_HTML - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_SCRIPT),
                             statuses[CREATED_SCRIPT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_IFRAME),
                             statuses[CREATED_IFRAME - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_DIV),
                             statuses[CREATED_DIV - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_LINK),
                             statuses[CREATED_LINK - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_INPUT),
                             statuses[CREATED_INPUT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_EMBED),
                             statuses[CREATED_EMBED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_OBJECT),
                             statuses[CREATED_OBJECT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_INJECTED),
                             statuses[AD_INJECTED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_REMOVED),
                             statuses[AD_REMOVED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_REPLACED),
                             statuses[AD_REPLACED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_LIKELY_INJECTED),
                             statuses[AD_LIKELY_INJECTED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_LIKELY_REPLACED),
                             statuses[AD_LIKELY_REPLACED - 1]);
  } else {
    prefix += "Google.";
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CONTENT_SCRIPT),
                             statuses[CONTENT_SCRIPT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(READ_DOM),
                             statuses[READ_DOM - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(MODIFIED_DOM),
                             statuses[MODIFIED_DOM - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(DOM_METHOD),
                             statuses[DOM_METHOD - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(DOCUMENT_WRITE),
                             statuses[DOCUMENT_WRITE - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(INNER_HTML),
                             statuses[INNER_HTML - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_SCRIPT),
                             statuses[CREATED_SCRIPT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_IFRAME),
                             statuses[CREATED_IFRAME - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_DIV),
                             statuses[CREATED_DIV - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_LINK),
                             statuses[CREATED_LINK - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_INPUT),
                             statuses[CREATED_INPUT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_EMBED),
                             statuses[CREATED_EMBED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(CREATED_OBJECT),
                             statuses[CREATED_OBJECT - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_INJECTED),
                             statuses[AD_INJECTED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_REMOVED),
                             statuses[AD_REMOVED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_REPLACED),
                             statuses[AD_REPLACED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_LIKELY_INJECTED),
                             statuses[AD_LIKELY_INJECTED - 1]);
    UMA_HISTOGRAM_COUNTS_100(prefix + GetHistogramName(AD_LIKELY_REPLACED),
                             statuses[AD_LIKELY_REPLACED - 1]);
  }
}

// Handle tab tracking. --------------------------------------------------------

void UmaPolicy::OnBrowserAdded(Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;
  browser->tab_strip_model()->AddObserver(this);
}

void UmaPolicy::OnBrowserRemoved(Browser* browser) {
  if (!profile_->IsSameProfile(browser->profile()))
    return;
  browser->tab_strip_model()->RemoveObserver(this);
}

// Use the value from SessionTabHelper::IdForTab, *not* |index|. |index| will be
// duplicated across tabs in a session, whereas IdForTab uniquely identifies
// each tab.
void UmaPolicy::TabChangedAt(content::WebContents* contents,
                             int index,
                             TabChangeType change_type) {
  if (change_type != TabStripModelObserver::LOADING_ONLY)
    return;
  if (!contents)
    return;

  std::string url = CleanURL(contents->GetLastCommittedURL());
  int32 tab_id = SessionTabHelper::IdForTab(contents);

  std::map<int32, std::string>::iterator tab_it = tab_list_.find(tab_id);

  // Ignore tabs that haven't changed status.
  if (tab_it != tab_list_.end() && tab_it->second == url)
    return;

  // Is this an existing tab whose URL has changed.
  if (tab_it != tab_list_.end()) {
    CleanupClosedPage(tab_it->second, contents);
    tab_list_.erase(tab_id);
  }

  // Check that tab_list_ isn't over the kMaxTabsTracked budget.
  if (tab_list_.size() >= kMaxTabsTracked)
    return;

  // Set up the new entries.
  tab_list_[tab_id] = url;
  SetupOpenedPage(url);
}

// Use the value from SessionTabHelper::IdForTab, *not* |index|. |index| will be
// duplicated across tabs in a session, whereas IdForTab uniquely identifies
// each tab.
void UmaPolicy::TabClosingAt(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) {
  if (!contents)
    return;
  std::string url = CleanURL(contents->GetLastCommittedURL());
  int32 tab_id = SessionTabHelper::IdForTab(contents);
  std::map<int, std::string>::iterator tab_it = tab_list_.find(tab_id);
  if (tab_it != tab_list_.end())
    tab_list_.erase(tab_id);

  CleanupClosedPage(url, contents);
}

void UmaPolicy::SetupOpenedPage(const std::string& url) {
  url_status_[url][kNumberOfTabs]++;
}

void UmaPolicy::CleanupClosedPage(const std::string& cleaned_url,
                                  content::WebContents* web_contents) {
  SiteMap::iterator old_site_lookup = url_status_.find(cleaned_url);
  if (old_site_lookup == url_status_.end())
    return;
  old_site_lookup->second[kNumberOfTabs]--;
  if (old_site_lookup->second[kNumberOfTabs] == 0) {
    HistogramOnClose(cleaned_url, web_contents);
    url_status_.erase(cleaned_url);
  }
}

// Helpers. --------------------------------------------------------------------

// We don't want to treat # ref navigations as if they were new pageloads.
// So we get rid of the ref if it has it.
// We convert to a string in the hopes that this is faster than Replacements.
std::string UmaPolicy::CleanURL(const GURL& gurl) {
  if (gurl.spec().empty())
    return GURL(url::kAboutBlankURL).spec();
  if (!gurl.is_valid())
    return gurl.spec();
  if (!gurl.has_ref())
    return gurl.spec();
  std::string port = "";
  if (gurl.has_port())
    port = ":" + gurl.port();
  std::string query = "";
  if (gurl.has_query())
    query = "?" + gurl.query();
  return base::StringPrintf("%s://%s%s%s%s",
                            gurl.scheme().c_str(),
                            gurl.host().c_str(),
                            port.c_str(),
                            gurl.path().c_str(),
                            query.c_str());
}

const char* UmaPolicy::GetHistogramName(PageStatus status) {
  switch (status) {
    case CONTENT_SCRIPT:
      return "ContentScript";
    case READ_DOM:
      return "ReadDom";
    case MODIFIED_DOM:
      return "ModifiedDom";
    case DOM_METHOD:
      return "InvokedDomMethod";
    case DOCUMENT_WRITE:
      return "DocumentWrite";
    case INNER_HTML:
      return "InnerHtml";
    case CREATED_SCRIPT:
      return "CreatedScript";
    case CREATED_IFRAME:
      return "CreatedIframe";
    case CREATED_DIV:
      return "CreatedDiv";
    case CREATED_LINK:
      return "CreatedLink";
    case CREATED_INPUT:
      return "CreatedInput";
    case CREATED_EMBED:
      return "CreatedEmbed";
    case CREATED_OBJECT:
      return "CreatedObject";
    case AD_INJECTED:
      return "AdInjected";
    case AD_REMOVED:
      return "AdRemoved";
    case AD_REPLACED:
      return "AdReplaced";
    case AD_LIKELY_INJECTED:
      return "AdLikelyInjected";
    case AD_LIKELY_REPLACED:
      return "AdLikelyReplaced";
    case NONE:
    case MAX_STATUS:
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace extensions
