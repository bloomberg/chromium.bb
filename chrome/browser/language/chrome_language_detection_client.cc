// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/chrome_language_detection_client.h"

#include "base/logging.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeLanguageDetectionClient);

ChromeLanguageDetectionClient::ChromeLanguageDetectionClient(
    content::WebContents* const web_contents)
    : content::WebContentsObserver(web_contents),
      language_histogram_(
          UrlLanguageHistogramFactory::GetInstance()->GetForBrowserContext(
              web_contents->GetBrowserContext())),
      binding_(this) {}

ChromeLanguageDetectionClient::~ChromeLanguageDetectionClient() = default;

// static
void ChromeLanguageDetectionClient::BindContentTranslateDriver(
    translate::mojom::ContentTranslateDriverRequest request,
    content::RenderFrameHost* const render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  // We try to bind to the driver, but if driver is not ready for now or totally
  // not available for this render frame host, the request will be just dropped.
  // This would cause the message pipe to be closed, which will raise a
  // connection error on the peer side.
  ChromeLanguageDetectionClient* const instance =
      ChromeLanguageDetectionClient::FromWebContents(web_contents);
  if (!instance)
    return;

  instance->binding_.Bind(std::move(request));
}

// translate::mojom::ContentTranslateDriver implementation.
void ChromeLanguageDetectionClient::RegisterPage(
    translate::mojom::PagePtr page,
    const translate::LanguageDetectionDetails& details,
    const bool page_needs_translation) {
  // If we have a language histogram (i.e. we're not in incognito), update it
  // with the detected language of every page visited.
  if (language_histogram_ && details.is_cld_reliable)
    language_histogram_->OnPageVisited(details.cld_language);

  ChromeTranslateClient* const translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  if (!translate_client)
    return;

  translate_client->translate_driver().RegisterPage(std::move(page), details,
                                                    page_needs_translation);
}
