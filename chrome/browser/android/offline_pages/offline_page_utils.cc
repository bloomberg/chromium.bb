// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"

namespace offline_pages {
namespace {

void OnGetPagesByURLDone(
    const GURL& url,
    int tab_id,
    const std::vector<std::string>& namespaces_to_show_in_original_tab,
    const base::Callback<void(const OfflinePageItem*)>& callback,
    const MultipleOfflinePageItemResult& pages) {
  const OfflinePageItem* selected_page_for_final_url = nullptr;
  const OfflinePageItem* selected_page_for_original_url = nullptr;
  std::string tab_id_str = base::IntToString(tab_id);

  for (const auto& page : pages) {
    if (base::ContainsValue(namespaces_to_show_in_original_tab,
                            page.client_id.name_space) &&
        page.client_id.id != tab_id_str) {
      continue;
    }

    if (OfflinePageUtils::EqualsIgnoringFragment(url, page.url)) {
      if (!selected_page_for_final_url ||
          page.creation_time > selected_page_for_final_url->creation_time) {
        selected_page_for_final_url = &page;
      }
    } else {
      // This is consistent with exact match against original url done in
      // OfflinePageModelImpl.
      DCHECK(url == page.original_url);
      if (!selected_page_for_original_url ||
          page.creation_time > selected_page_for_original_url->creation_time) {
        selected_page_for_original_url = &page;
      }
    }
  }

  // Match for final URL should take high priority than matching for original
  // URL.
  callback.Run(selected_page_for_final_url ? selected_page_for_final_url
                                           : selected_page_for_original_url);
}

bool IsSupportedByDownload(content::BrowserContext* browser_context,
                           const std::string& name_space) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  DCHECK(offline_page_model);
  ClientPolicyController* policy_controller =
      offline_page_model->GetPolicyController();
  DCHECK(policy_controller);
  return policy_controller->IsSupportedByDownload(name_space);
}

void CheckDuplicateOngoingDownloads(
    content::BrowserContext* browser_context,
    const GURL& url,
    const OfflinePageUtils::DuplicateCheckCallback& callback) {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context);
  if (!request_coordinator)
    return;

  auto request_coordinator_continuation =
      [](content::BrowserContext* browser_context, const GURL& url,
         const OfflinePageUtils::DuplicateCheckCallback& callback,
         std::vector<std::unique_ptr<SavePageRequest>> requests) {
        base::Time latest_request_time;
        for (auto& request : requests) {
          if (IsSupportedByDownload(browser_context,
                                    request->client_id().name_space) &&
              request->url() == url &&
              latest_request_time < request->creation_time()) {
            latest_request_time = request->creation_time();
          }
        }

        if (latest_request_time.is_null()) {
          callback.Run(OfflinePageUtils::DuplicateCheckResult::NOT_FOUND);
        } else {
          // Using CUSTOM_COUNTS instead of time-oriented histogram to record
          // samples in seconds rather than milliseconds.
          UMA_HISTOGRAM_CUSTOM_COUNTS(
              "OfflinePages.DownloadRequestTimeSinceDuplicateRequested",
              (base::Time::Now() - latest_request_time).InSeconds(),
              base::TimeDelta::FromSeconds(1).InSeconds(),
              base::TimeDelta::FromDays(7).InSeconds(), 50);

          callback.Run(
              OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND);
        }
      };

  request_coordinator->GetAllRequests(base::Bind(
      request_coordinator_continuation, browser_context, url, callback));
}

void DoCalculateSizeBetween(
    const offline_pages::SizeInBytesCallback& callback,
    const base::Time& begin_time,
    const base::Time& end_time,
    const offline_pages::MultipleOfflinePageItemResult& result) {
  int64_t total_size = 0;
  for (auto& page : result) {
    if (begin_time <= page.creation_time && page.creation_time < end_time)
      total_size += page.file_size;
  }
  callback.Run(total_size);
}

}  // namespace

// static
void OfflinePageUtils::SelectPageForURL(
    content::BrowserContext* browser_context,
    const GURL& url,
    OfflinePageModel::URLSearchMode url_search_mode,
    int tab_id,
    const base::Callback<void(const OfflinePageItem*)>& callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  offline_page_model->GetPagesByURL(
      url,
      url_search_mode,
      base::Bind(&OnGetPagesByURLDone, url, tab_id,
                 offline_page_model->GetPolicyController()
                     ->GetNamespacesRestrictedToOriginalTab(),
                 callback));
}

const OfflinePageItem* OfflinePageUtils::GetOfflinePageFromWebContents(
    content::WebContents* web_contents) {
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return nullptr;
  const OfflinePageItem* offline_page = tab_helper->offline_page();
  if (!offline_page)
    return nullptr;

  // Returns the cached offline page only if the offline URL matches with
  // current tab URL (skipping fragment identifier part). This is to prevent
  // from returning the wrong offline page if DidStartNavigation is never called
  // to clear it up.
  GURL::Replacements remove_params;
  remove_params.ClearRef();
  GURL offline_url = offline_page->url.ReplaceComponents(remove_params);
  GURL web_contents_url =
      web_contents->GetVisibleURL().ReplaceComponents(remove_params);
  return offline_url == web_contents_url ? offline_page : nullptr;
}

// static
const OfflinePageHeader* OfflinePageUtils::GetOfflineHeaderFromWebContents(
    content::WebContents* web_contents) {
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper ? &(tab_helper->offline_header()) : nullptr;
}

// static
bool OfflinePageUtils::IsShowingOfflinePreview(
    content::WebContents* web_contents) {
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->IsShowingOfflinePreview();
}

// static
bool OfflinePageUtils::IsShowingDownloadButtonInErrorPage(
    content::WebContents* web_contents) {
  chrome_browser_net::NetErrorTabHelper* tab_helper =
      chrome_browser_net::NetErrorTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->is_showing_download_button_in_error_page();
}

// static
bool OfflinePageUtils::GetTabId(content::WebContents* web_contents,
                                int* tab_id) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  if (!tab_android)
    return false;
  *tab_id = tab_android->GetAndroidId();
  return true;
}

// static
bool OfflinePageUtils::CurrentlyShownInCustomTab(
    content::WebContents* web_contents) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);
  return tab_android && tab_android->IsCurrentlyACustomTab();
}

// static
bool OfflinePageUtils::EqualsIgnoringFragment(const GURL& lhs,
                                              const GURL& rhs) {
  GURL::Replacements remove_params;
  remove_params.ClearRef();

  GURL lhs_stripped = lhs.ReplaceComponents(remove_params);
  GURL rhs_stripped = rhs.ReplaceComponents(remove_params);

  return lhs_stripped == rhs_stripped;
}

// static
GURL OfflinePageUtils::GetOriginalURLFromWebContents(
    content::WebContents* web_contents) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry || entry->GetRedirectChain().size() <= 1)
    return GURL();
  return entry->GetRedirectChain().front();
}

// static
void OfflinePageUtils::CheckDuplicateDownloads(
    content::BrowserContext* browser_context,
    const GURL& url,
    const DuplicateCheckCallback& callback) {
  // First check for finished downloads, that is, saved pages.
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model)
    return;

  auto continuation = [](content::BrowserContext* browser_context,
                         const GURL& url,
                         const DuplicateCheckCallback& callback,
                         const std::vector<OfflinePageItem>& pages) {
    base::Time latest_saved_time;
    for (const auto& offline_page_item : pages) {
      if (IsSupportedByDownload(browser_context,
                                offline_page_item.client_id.name_space) &&
          latest_saved_time < offline_page_item.creation_time) {
        latest_saved_time = offline_page_item.creation_time;
      }
    }
    if (latest_saved_time.is_null()) {
      // Then check for ongoing downloads, that is, requests.
      CheckDuplicateOngoingDownloads(browser_context, url, callback);
    } else {
      // Using CUSTOM_COUNTS instead of time-oriented histogram to record
      // samples in seconds rather than milliseconds.
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "OfflinePages.DownloadRequestTimeSinceDuplicateSaved",
          (base::Time::Now() - latest_saved_time).InSeconds(),
          base::TimeDelta::FromSeconds(1).InSeconds(),
          base::TimeDelta::FromDays(7).InSeconds(), 50);

      callback.Run(DuplicateCheckResult::DUPLICATE_PAGE_FOUND);
    }
  };

  offline_page_model->GetPagesByURL(
      url, OfflinePageModel::URLSearchMode::SEARCH_BY_ALL_URLS,
      base::Bind(continuation, browser_context, url, callback));
}

// static
void OfflinePageUtils::ScheduleDownload(content::WebContents* web_contents,
                                        const std::string& name_space,
                                        const GURL& url,
                                        DownloadUIActionFlags ui_action) {
  DCHECK(web_contents);

  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;
  tab_helper->ScheduleDownloadHelper(web_contents, name_space, url, ui_action);
}

// static
bool OfflinePageUtils::CanDownloadAsOfflinePage(
    const GURL& url,
    const std::string& contents_mime_type) {
  return url.SchemeIsHTTPOrHTTPS() &&
         (net::MatchesMimeType(contents_mime_type, "text/html") ||
          net::MatchesMimeType(contents_mime_type, "application/xhtml+xml"));
}

// static
bool OfflinePageUtils::GetCachedOfflinePageSizeBetween(
    content::BrowserContext* browser_context,
    const SizeInBytesCallback& callback,
    const base::Time& begin_time,
    const base::Time& end_time) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model || begin_time > end_time)
    return false;
  OfflinePageModelQueryBuilder builder;
  builder.RequireRemovedOnCacheReset(
      OfflinePageModelQuery::Requirement::INCLUDE_MATCHING);
  offline_page_model->GetPagesMatchingQuery(
      builder.Build(offline_page_model->GetPolicyController()),
      base::Bind(&DoCalculateSizeBetween, callback, begin_time, end_time));
  return true;
}

}  // namespace offline_pages
