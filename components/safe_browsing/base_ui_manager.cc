// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/base_ui_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;
using safe_browsing::SBThreatType;

namespace {

const void* const kWhitelistKey = &kWhitelistKey;

// A WhitelistUrlSet holds the set of URLs that have been whitelisted
// for a specific WebContents, along with pending entries that are still
// undecided. Each URL is associated with the first SBThreatType that
// was seen for that URL. The URLs in this set should come from
// GetWhitelistUrl() or GetMainFrameWhitelistUrlForResource() (in
// SafeBrowsingUIManager)
class WhitelistUrlSet : public base::SupportsUserData::Data {
 public:
  WhitelistUrlSet() {}
  bool Contains(const GURL url, SBThreatType* threat_type) {
    auto found = map_.find(url);
    if (found == map_.end())
      return false;
    if (threat_type)
      *threat_type = found->second;
    return true;
  }
  void RemovePending(const GURL& url) { pending_.erase(url); }
  void Insert(const GURL url, SBThreatType threat_type) {
    if (Contains(url, nullptr))
      return;
    map_[url] = threat_type;
    RemovePending(url);
  }
  bool ContainsPending(const GURL& url, SBThreatType* threat_type) {
    auto found = pending_.find(url);
    if (found == pending_.end())
      return false;
    if (threat_type)
      *threat_type = found->second;
    return true;
  }
  void InsertPending(const GURL url, SBThreatType threat_type) {
    if (ContainsPending(url, nullptr))
      return;
    pending_[url] = threat_type;
  }

 private:
  std::map<GURL, SBThreatType> map_;
  std::map<GURL, SBThreatType> pending_;
  DISALLOW_COPY_AND_ASSIGN(WhitelistUrlSet);
};

// Returns the URL that should be used in a WhitelistUrlSet for the
// resource loaded from |url| on a navigation |entry|.
GURL GetWhitelistUrl(const GURL& url,
                     bool is_subresource,
                     NavigationEntry* entry) {
  if (is_subresource) {
    if (!entry)
      return GURL();
    return entry->GetURL().GetWithEmptyPath();
  }
  return url.GetWithEmptyPath();
}

WhitelistUrlSet* GetOrCreateWhitelist(WebContents* web_contents) {
  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));
  if (!site_list) {
    site_list = new WhitelistUrlSet;
    web_contents->SetUserData(kWhitelistKey, site_list);
  }
  return site_list;
}

}  // namespace

namespace safe_browsing {

BaseSafeBrowsingUIManager::BaseSafeBrowsingUIManager() {}

void BaseSafeBrowsingUIManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(ntfschr): implement this once SafeBrowsingService is componentized
  return;
}

BaseSafeBrowsingUIManager::~BaseSafeBrowsingUIManager() {}

bool BaseSafeBrowsingUIManager::IsWhitelisted(const UnsafeResource& resource) {
  NavigationEntry* entry = nullptr;
  if (resource.is_subresource) {
    entry = resource.GetNavigationEntryForResource();
  }
  SBThreatType unused_threat_type;
  return IsUrlWhitelistedOrPendingForWebContents(
      resource.url, resource.is_subresource, entry,
      resource.web_contents_getter.Run(), true, &unused_threat_type);
}

// Check if the user has already seen and/or ignored a SB warning for this
// WebContents and top-level domain.
bool BaseSafeBrowsingUIManager::IsUrlWhitelistedOrPendingForWebContents(
    const GURL& url,
    bool is_subresource,
    NavigationEntry* entry,
    WebContents* web_contents,
    bool whitelist_only,
    SBThreatType* threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL lookup_url = GetWhitelistUrl(url, is_subresource, entry);
  if (lookup_url.is_empty())
    return false;

  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));
  if (!site_list)
    return false;

  bool whitelisted = site_list->Contains(lookup_url, threat_type);
  if (whitelist_only) {
    return whitelisted;
  } else {
    return whitelisted || site_list->ContainsPending(lookup_url, threat_type);
  }
}

void BaseSafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed,
    WebContents* web_contents,
    const GURL& main_frame_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& resource : resources) {
    if (!resource.callback.is_null()) {
      DCHECK(resource.callback_thread);
      resource.callback_thread->PostTask(
          FROM_HERE, base::Bind(resource.callback, proceed));
    }

    GURL whitelist_url = GetWhitelistUrl(
        main_frame_url, false /* is subresource */,
        nullptr /* no navigation entry needed for main resource */);
    if (proceed) {
      AddToWhitelistUrlSet(whitelist_url, web_contents,
                           false /* Pending -> permanent */,
                           resource.threat_type);
    } else if (web_contents) {
      // |web_contents| doesn't exist if the tab has been closed.
      RemoveFromPendingWhitelistUrlSet(whitelist_url, web_contents);
    }
  }
}

void BaseSafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(ntfschr): implement this once SafeBrowsingBlockingPage is
  // componentized
  return;
}

void BaseSafeBrowsingUIManager::EnsureWhitelistCreated(
    WebContents* web_contents) {
  GetOrCreateWhitelist(web_contents);
}

void BaseSafeBrowsingUIManager::LogPauseDelay(base::TimeDelta time) {
  return;
}

// A safebrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// UMA || extended_reporting users.
void BaseSafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(ntfschr): implement this once we support reporting in WebView
  return;
}

void BaseSafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread(
    const HitReport& hit_report) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(ntfschr): implement this once we support reporting in WebView
  return;
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void BaseSafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(ntfschr): implement this once we support reporting in WebView
  return;
}

// Record this domain in the given WebContents as either whitelisted or
// pending whitelisting (if an interstitial is currently displayed). If an
// existing WhitelistUrlSet does not yet exist, create a new WhitelistUrlSet.
void BaseSafeBrowsingUIManager::AddToWhitelistUrlSet(const GURL& whitelist_url,
                                                     WebContents* web_contents,
                                                     bool pending,
                                                     SBThreatType threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A WebContents might not exist if the tab has been closed.
  if (!web_contents)
    return;

  WhitelistUrlSet* site_list = GetOrCreateWhitelist(web_contents);

  if (whitelist_url.is_empty())
    return;

  if (pending) {
    site_list->InsertPending(whitelist_url, threat_type);
  } else {
    site_list->Insert(whitelist_url, threat_type);
  }

  // Notify security UI that security state has changed.
  web_contents->DidChangeVisibleSecurityState();
}

void BaseSafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void BaseSafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void BaseSafeBrowsingUIManager::RemoveFromPendingWhitelistUrlSet(
    const GURL& whitelist_url,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // A WebContents might not exist if the tab has been closed.
  if (!web_contents)
    return;

  // Use |web_contents| rather than |resource.web_contents_getter|
  // here. By this point, a "Back" navigation could have already been
  // committed, so the page loading |resource| might be gone and
  // |web_contents_getter| may no longer be valid.
  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));

  if (whitelist_url.is_empty())
    return;

  // Note that this function does not DCHECK that |whitelist_url|
  // appears in the pending whitelist. In the common case, it's expected
  // that a URL is in the pending whitelist when it is removed, but it's
  // not always the case. For example, if there are several blocking
  // pages queued up for different resources on the same page, and the
  // user goes back to dimiss the first one, the subsequent blocking
  // pages get dismissed as well (as if the user had clicked "Back to
  // safety" on each of them). In this case, the first dismissal will
  // remove the main-frame URL from the pending whitelist, so the
  // main-frame URL will have already been removed when the subsequent
  // blocking pages are dismissed.
  if (site_list->ContainsPending(whitelist_url, nullptr))
    site_list->RemovePending(whitelist_url);

  // Notify security UI that security state has changed.
  web_contents->DidChangeVisibleSecurityState();
}

}  // namespace safe_browsing
