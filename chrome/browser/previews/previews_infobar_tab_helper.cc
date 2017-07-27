// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_tab_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/offline_pages/features/features.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_ui_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

namespace {

// Adds the preview navigation to the black list.
void AddPreviewNavigationCallback(content::BrowserContext* browser_context,
                                  const GURL& url,
                                  previews::PreviewsType type,
                                  bool opt_out) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (previews_service && previews_service->previews_ui_service()) {
    previews_service->previews_ui_service()->AddPreviewNavigation(url, type,
                                                                  opt_out);
  }
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PreviewsInfoBarTabHelper);

PreviewsInfoBarTabHelper::~PreviewsInfoBarTabHelper() {}

PreviewsInfoBarTabHelper::PreviewsInfoBarTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      displayed_preview_infobar_(false),
      displayed_preview_timestamp_(false) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PreviewsInfoBarTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only show the infobar if this is a full main frame navigation.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsSameDocument())
    return;
  // The infobar should only be told if the page was a reload if the previous
  // page displayed a timestamp.
  bool is_reload =
      displayed_preview_timestamp_
          ? navigation_handle->GetReloadType() != content::ReloadType::NONE
          : false;
  displayed_preview_infobar_ = false;
  displayed_preview_timestamp_ = false;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents());

  if (tab_helper && tab_helper->IsShowingOfflinePreview()) {
    if (navigation_handle->IsErrorPage()) {
      // TODO(ryansturm): Add UMA for errors.
      return;
    }
    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                web_contents()->GetBrowserContext());
    PreviewsInfoBarDelegate::Create(
        web_contents(), previews::PreviewsType::OFFLINE,
        base::Time() /* previews_freshness */, false /* is_reload */,
        data_reduction_proxy_settings &&
            data_reduction_proxy_settings->IsDataReductionProxyEnabled(),
        base::Bind(&AddPreviewNavigationCallback,
                   web_contents()->GetBrowserContext(),
                   navigation_handle->GetRedirectChain()[0],
                   previews::PreviewsType::OFFLINE));
    // Don't try to show other infobars if this is an offline preview.
    return;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers && data_reduction_proxy::IsLitePagePreview(*headers)) {
    base::Time previews_freshness;
    headers->GetDateValue(&previews_freshness);
    PreviewsInfoBarDelegate::Create(
        web_contents(), previews::PreviewsType::LITE_PAGE, previews_freshness,
        true /* is_data_saver_user */, is_reload,
        base::Bind(&AddPreviewNavigationCallback,
                   web_contents()->GetBrowserContext(),
                   navigation_handle->GetRedirectChain()[0],
                   previews::PreviewsType::LITE_PAGE));
  }
}
