// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/android/tab_android.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/request_header/offline_page_header.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

void OnGetPagesByOnlineURLDone(
    int tab_id,
    const std::vector<std::string>& namespaces_to_show_in_original_tab,
    const base::Callback<void(const OfflinePageItem*)>& callback,
    const MultipleOfflinePageItemResult& pages) {
  const OfflinePageItem* selected_page = nullptr;
  std::string tab_id_str = base::IntToString(tab_id);

  for (const auto& offline_page : pages) {
    auto result = std::find(namespaces_to_show_in_original_tab.begin(),
                            namespaces_to_show_in_original_tab.end(),
                            offline_page.client_id.name_space);
    if (result != namespaces_to_show_in_original_tab.end() &&
        offline_page.client_id.id != tab_id_str) {
      continue;
    }

    if (!selected_page ||
        offline_page.creation_time > selected_page->creation_time) {
      selected_page = &offline_page;
    }
  }
  callback.Run(selected_page);
}

}  // namespace

// static
void OfflinePageUtils::SelectPageForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url,
    int tab_id,
    const base::Callback<void(const OfflinePageItem*)>& callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  offline_page_model->GetPagesByOnlineURL(
      online_url, base::Bind(&OnGetPagesByOnlineURLDone, tab_id,
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
bool OfflinePageUtils::GetTabId(content::WebContents* web_contents,
                                int* tab_id) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  if (!tab_android)
    return false;
  *tab_id = tab_android->GetAndroidId();
  return true;
}

// static
void OfflinePageUtils::CheckExistenceOfPagesWithURL(
    content::BrowserContext* browser_context,
    const std::string name_space,
    const GURL& offline_page_url,
    const base::Callback<void(bool)>& callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  DCHECK(offline_page_model);
  auto continuation = [](const std::string& name_space,
                         const base::Callback<void(bool)>& callback,
                         const std::vector<OfflinePageItem>& pages) {
    for (auto& page : pages) {
      if (page.client_id.name_space == name_space) {
        callback.Run(true);
        return;
      }
    }
    callback.Run(false);
  };

  offline_page_model->GetPagesByOnlineURL(
      offline_page_url, base::Bind(continuation, name_space, callback));
}

// static
void OfflinePageUtils::CheckExistenceOfRequestsWithURL(
    content::BrowserContext* browser_context,
    const std::string name_space,
    const GURL& offline_page_url,
    const base::Callback<void(bool)>& callback) {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context);

  auto request_coordinator_continuation = [](
      const std::string& name_space, const GURL& offline_page_url,
      const base::Callback<void(bool)>& callback,
      std::vector<std::unique_ptr<SavePageRequest>> requests) {
    for (auto& request : requests) {
      if (request->url() == offline_page_url &&
          request->client_id().name_space == name_space) {
        callback.Run(true);
        return;
      }
    }
    callback.Run(false);
  };

  request_coordinator->GetAllRequests(
      base::Bind(request_coordinator_continuation, name_space, offline_page_url,
                 callback));
}

// static
bool OfflinePageUtils::EqualsIgnoringFragment(const GURL& lhs,
                                              const GURL& rhs) {
  GURL::Replacements remove_params;
  remove_params.ClearRef();

  GURL lhs_stripped = lhs.ReplaceComponents(remove_params);
  GURL rhs_stripped = lhs.ReplaceComponents(remove_params);

  return lhs_stripped == rhs_stripped;
}

}  // namespace offline_pages
