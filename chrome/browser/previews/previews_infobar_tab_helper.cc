// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_infobar_tab_helper.h"

#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/common/features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PreviewsInfoBarTabHelper);

PreviewsInfoBarTabHelper::~PreviewsInfoBarTabHelper() {}

PreviewsInfoBarTabHelper::PreviewsInfoBarTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      displayed_preview_infobar_(false),
      is_showing_offline_preview_(false) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PreviewsInfoBarTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only show the infobar if this is a full main frame navigation.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsSamePage())
    return;
  is_showing_offline_preview_ = false;
  displayed_preview_infobar_ = false;

#if BUILDFLAG(ANDROID_JAVA_UI)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents());

  if (tab_helper && tab_helper->IsShowingOfflinePreview()) {
    if (navigation_handle->IsErrorPage()) {
      // TODO(ryansturm): Add UMA for errors.
      return;
    }
    is_showing_offline_preview_ = true;
    PreviewsInfoBarDelegate::Create(web_contents(),
                                    PreviewsInfoBarDelegate::OFFLINE);
    // Don't try to show other infobars if this is an offline preview.
    return;
  }
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers && data_reduction_proxy::IsLitePagePreview(*headers)) {
    PreviewsInfoBarDelegate::Create(web_contents(),
                                    PreviewsInfoBarDelegate::LITE_PAGE);
  }
}

