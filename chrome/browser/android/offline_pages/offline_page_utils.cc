// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

// Returns an offline page originated from the |online_url|.
const OfflinePageItem* MaybeGetBestOfflinePageForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url) {
  DCHECK(browser_context);

  if (!IsOfflinePagesEnabled())
    return nullptr;

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model)
    return nullptr;

  return offline_page_model->MaybeGetBestPageForOnlineURL(online_url);
}

// Returns an offline page that is stored as the |offline_url|.
const OfflinePageItem* GetOfflinePageForOfflineURL(
    content::BrowserContext* browser_context,
    const GURL& offline_url) {
  DCHECK(browser_context);

  if (!IsOfflinePagesEnabled())
    return nullptr;

  // Note that we first check if the url likely points to an offline page
  // before calling GetPageByOfflineURL in order to avoid unnecessary lookup
  // cost.
  if (!OfflinePageUtils::MightBeOfflineURL(offline_url))
    return nullptr;

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model)
    return nullptr;

  return offline_page_model->MaybeGetPageByOfflineURL(offline_url);
}

void OnGetPageByOfflineURLDone(
    const base::Callback<void(const GURL&)>& callback,
    const base::Optional<OfflinePageItem>& item) {
  GURL result_url;
  if (item)
    result_url = item->url;
  callback.Run(result_url);
}

void OnGetBestPageForOnlineURLDone(
    const base::Callback<void(const GURL&)>& callback,
    const base::Optional<OfflinePageItem>& item) {
  GURL result_url;
  if (item)
    result_url = item->GetOfflineURL();
  callback.Run(result_url);
}

}  // namespace

// static
bool OfflinePageUtils::MightBeOfflineURL(const GURL& url) {
  // It has to be a file URL ending with .mhtml extension.
  return url.is_valid() && url.SchemeIsFile() &&
         base::EndsWith(url.spec(),
                        OfflinePageMHTMLArchiver::GetFileNameExtension(),
                        base::CompareCase::INSENSITIVE_ASCII);
}

// static
GURL OfflinePageUtils::MaybeGetOfflineURLForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url) {
  const OfflinePageItem* offline_page =
      MaybeGetBestOfflinePageForOnlineURL(browser_context, online_url);
  if (!offline_page)
    return GURL();

  return offline_page->GetOfflineURL();
}

// static
void OfflinePageUtils::GetOfflineURLForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url,
    const base::Callback<void(const GURL&)>& callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&OnGetPageByOfflineURLDone, callback, base::nullopt));
    return;
  }

  offline_page_model->GetBestPageForOnlineURL(
      online_url, base::Bind(&OnGetBestPageForOnlineURLDone, callback));
}

// static
GURL OfflinePageUtils::MaybeGetOnlineURLForOfflineURL(
    content::BrowserContext* browser_context,
    const GURL& offline_url) {
  const OfflinePageItem* offline_page =
      GetOfflinePageForOfflineURL(browser_context, offline_url);
  if (!offline_page)
    return GURL();

  return offline_page->url;
}

// static
void OfflinePageUtils::GetOnlineURLForOfflineURL(
    content::BrowserContext* browser_context,
    const GURL& offline_url,
    const base::Callback<void(const GURL&)>& callback) {
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&OnGetPageByOfflineURLDone, callback, base::nullopt));
    return;
  }

  offline_page_model->GetPageByOfflineURL(
      offline_url, base::Bind(&OnGetPageByOfflineURLDone, callback));
}

// static
bool OfflinePageUtils::IsOfflinePage(content::BrowserContext* browser_context,
                                     const GURL& offline_url) {
  return GetOfflinePageForOfflineURL(browser_context, offline_url) != nullptr;
}

// static
bool OfflinePageUtils::HasOfflinePageForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url) {
  const OfflinePageItem* offline_page =
      MaybeGetBestOfflinePageForOnlineURL(browser_context, online_url);
  return offline_page && !offline_page->file_path.empty();
}

// static
void OfflinePageUtils::MarkPageAccessed(
    content::BrowserContext* browser_context, const GURL& offline_url) {
  DCHECK(browser_context);

  const OfflinePageItem* offline_page =
      GetOfflinePageForOfflineURL(browser_context, offline_url);
  if (!offline_page)
    return;

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  DCHECK(offline_page_model);
  offline_page_model->MarkPageAccessed(offline_page->offline_id);
}

}  // namespace offline_pages
