// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_factory.h"

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/history/top_sites_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

struct RawPrepopulatedPage {
  int url_id;        // The resource for the page URL.
  int title_id;      // The resource for the page title.
  int favicon_id;    // The raw data resource for the favicon.
  int thumbnail_id;  // The raw data resource for the thumbnail.
  SkColor color;     // The best color to highlight the page (should roughly
                     // match favicon).
};

#if !defined(OS_ANDROID)
// Android does not use prepopulated pages.
const RawPrepopulatedPage kRawPrepopulatedPages[] = {
    {
     IDS_CHROME_WELCOME_URL,
     IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE,
     IDR_PRODUCT_LOGO_16,
     IDR_NEWTAB_CHROME_WELCOME_PAGE_THUMBNAIL,
     SkColorSetRGB(0, 147, 60),
    },
    {
     IDS_WEBSTORE_URL,
     IDS_EXTENSION_WEB_STORE_TITLE,
     IDR_WEBSTORE_ICON_16,
     IDR_NEWTAB_WEBSTORE_THUMBNAIL,
     SkColorSetRGB(63, 132, 197),
    },
};
#endif

void InitializePrepopulatedPageList(
    history::PrepopulatedPageList* prepopulated_pages) {
#if !defined(OS_ANDROID)
  DCHECK(prepopulated_pages);
  prepopulated_pages->reserve(arraysize(kRawPrepopulatedPages));
  for (size_t i = 0; i < arraysize(kRawPrepopulatedPages); ++i) {
    const RawPrepopulatedPage& page = kRawPrepopulatedPages[i];
    prepopulated_pages->push_back(history::PrepopulatedPage(
        GURL(l10n_util::GetStringUTF8(page.url_id)),
        l10n_util::GetStringUTF16(page.title_id), page.favicon_id,
        page.thumbnail_id, page.color));
  }
#endif
}

}  // namespace

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForProfile(
    Profile* profile) {
  return static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserContext(profile, false).get());
}

// static
TopSitesFactory* TopSitesFactory::GetInstance() {
  return Singleton<TopSitesFactory>::get();
}

TopSitesFactory::TopSitesFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "TopSites",
          BrowserContextDependencyManager::GetInstance()) {
}

TopSitesFactory::~TopSitesFactory() {
}

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  history::PrepopulatedPageList prepopulated_pages;
  InitializePrepopulatedPageList(&prepopulated_pages);

  history::TopSitesImpl* top_sites = new history::TopSitesImpl(
      static_cast<Profile*>(context), prepopulated_pages);
  top_sites->Init(context->GetPath().Append(chrome::kTopSitesFilename),
                  content::BrowserThread::GetMessageLoopProxyForThread(
                      content::BrowserThread::DB));
  return make_scoped_refptr(top_sites);
}

bool TopSitesFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
