// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
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
const offline_pages::OfflinePageItem* GetOfflinePageForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url) {
  DCHECK(browser_context);

  if (!offline_pages::IsOfflinePagesEnabled())
    return nullptr;

  offline_pages::OfflinePageModel* offline_page_model =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(
          browser_context);
  if (!offline_page_model)
    return nullptr;

  return offline_page_model->GetPageByOnlineURL(online_url);
}

// Returns an offline page that is stored as the |offline_url|.
const offline_pages::OfflinePageItem* GetOfflinePageForOfflineURL(
    content::BrowserContext* browser_context,
    const GURL& offline_url) {
  DCHECK(browser_context);

  if (!offline_pages::IsOfflinePagesEnabled())
    return nullptr;

  // Note that we first check if the url likely points to an offline page
  // before calling GetPageByOfflineURL in order to avoid unnecessary lookup
  // cost.
  if (!OfflinePageUtils::MightBeOfflineURL(offline_url))
    return nullptr;

  offline_pages::OfflinePageModel* offline_page_model =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(
          browser_context);
  if (!offline_page_model)
    return nullptr;

  return offline_page_model->GetPageByOfflineURL(offline_url);
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
GURL OfflinePageUtils::GetOfflineURLForOnlineURL(
    content::BrowserContext* browser_context,
    const GURL& online_url) {
  const offline_pages::OfflinePageItem* offline_page =
      GetOfflinePageForOnlineURL(browser_context, online_url);
  if (!offline_page)
    return GURL();

  return offline_page->GetOfflineURL();
}

// static
GURL OfflinePageUtils::GetOnlineURLForOfflineURL(
    content::BrowserContext* browser_context,
    const GURL& offline_url) {
  const offline_pages::OfflinePageItem* offline_page =
      GetOfflinePageForOfflineURL(browser_context, offline_url);
  if (!offline_page)
    return GURL();

  return offline_page->url;
}

// static
int64_t OfflinePageUtils::GetBookmarkIdForOfflineURL(
    content::BrowserContext* browser_context,
    const GURL& offline_url) {
  const offline_pages::OfflinePageItem* offline_page =
      GetOfflinePageForOfflineURL(browser_context, offline_url);
  if (!offline_page)
    return -1;

  if (offline_page->client_id.name_space != offline_pages::kBookmarkNamespace) {
    return -1;
  }

  int64_t result;
  if (base::StringToInt64(offline_page->client_id.id, &result)) {
    return result;
  }
  return -1;
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
  const offline_pages::OfflinePageItem* offline_page =
      GetOfflinePageForOnlineURL(browser_context, online_url);
  return offline_page && !offline_page->file_path.empty();
}

}  // namespace offline_pages
