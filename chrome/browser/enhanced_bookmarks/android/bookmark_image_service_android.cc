// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_parser.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/enhanced_bookmarks/android/bookmark_image_service_android.h"
#include "chrome/browser/enhanced_bookmarks/enhanced_bookmark_model_factory.h"
#include "chrome/grit/browser_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/base/load_flags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::Referrer;
using bookmarks::BookmarkNode;

namespace enhanced_bookmarks {

BookmarkImageServiceAndroid::BookmarkImageServiceAndroid(
    content::BrowserContext* browserContext)
    : BookmarkImageService(
          browserContext->GetPath(),
          EnhancedBookmarkModelFactory::GetForBrowserContext(browserContext),
          make_scoped_refptr(content::BrowserThread::GetBlockingPool())),
      browser_context_(browserContext) {
}

void BookmarkImageServiceAndroid::RetrieveSalientImage(
    const GURL& page_url,
    const GURL& image_url,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy,
    bool update_bookmark) {
  const BookmarkNode* bookmark =
      enhanced_bookmark_model_->bookmark_model()
          ->GetMostRecentlyAddedUserNodeForURL(page_url);
  if (!bookmark || !image_url.is_valid()) {
    ProcessNewImage(page_url, update_bookmark, gfx::Image(), image_url);
    return;
  }

  BitmapFetcherHandler* bitmap_fetcher_handler =
      new BitmapFetcherHandler(this, image_url);
  bitmap_fetcher_handler->Start(
      browser_context_, referrer, referrer_policy,
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES,
      update_bookmark, page_url);
}

void BookmarkImageServiceAndroid::RetrieveSalientImageFromContext(
    content::RenderFrameHost* render_frame_host,
    const GURL& page_url,
    bool update_bookmark) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (IsPageUrlInProgress(page_url))
    return;  // A request for this URL is already in progress.

  const BookmarkNode* bookmark = enhanced_bookmark_model_->bookmark_model()
          ->GetMostRecentlyAddedUserNodeForURL(page_url);
  if (!bookmark)
    return;

  // Stop the image extraction if there is already an image present.
  GURL url;
  int height, width;
  if (enhanced_bookmark_model_->GetOriginalImage(bookmark, &url, &width,
                                                 &height) ||
      enhanced_bookmark_model_->GetThumbnailImage(bookmark, &url, &width,
                                                  &height)) {
    return;
  }

  if (script_.empty()) {
    script_ =
        base::UTF8ToUTF16(ResourceBundle::GetSharedInstance()
                              .GetRawDataResource(IDR_GET_SALIENT_IMAGE_URL_JS)
                              .as_string());
  }

  render_frame_host->ExecuteJavaScript(
      script_,
      base::Bind(
          &BookmarkImageServiceAndroid::RetrieveSalientImageFromContextCallback,
          base::Unretained(this), page_url, update_bookmark));
}

void BookmarkImageServiceAndroid::FinishSuccessfulPageLoadForTab(
    content::WebContents* web_contents, bool update_bookmark) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();

  // If the navigation is a simple back or forward, do not extract images, those
  // were extracted already.
  if (!entry || (entry->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK))
    return;
  const GURL& entry_url = entry->GetURL();
  const GURL& entry_original_url = entry->GetOriginalRequestURL();
  std::vector<GURL> urls;
  urls.push_back(entry_url);
  if (entry_url != entry_original_url)
    urls.push_back(entry_original_url);
  for (GURL url : urls) {
    if (enhanced_bookmark_model_->bookmark_model()->IsBookmarked(url)) {
      RetrieveSalientImageFromContext(web_contents->GetMainFrame(), url,
                                      update_bookmark);
    }
  }
}

void BookmarkImageServiceAndroid::RetrieveSalientImageFromContextCallback(
    const GURL& page_url,
    bool update_bookmark,
    const base::Value* result) {
  if (!result)
    return;

  std::string json;
  if (!result->GetAsString(&json)) {
    LOG(WARNING)
        << "Salient image extracting script returned non-string result.";
    return;
  }

  scoped_ptr<base::Value> json_data;
  int error_code = 0;
  std::string errorMessage;
  json_data.reset(base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error_code, &errorMessage));
  if (error_code || !json_data) {
    LOG(WARNING) << "JSON parse error: " << errorMessage.c_str() << json;
    return;
  }

  base::DictionaryValue* dict;
  if (!json_data->GetAsDictionary(&dict)) {
    LOG(WARNING) << "JSON parse error, not a dict: " << json;
    return;
  }

  std::string referrerPolicy;
  std::string image_url;
  dict->GetString("referrerPolicy", &referrerPolicy);
  dict->GetString("imageUrl", &image_url);

  // The policy strings are guaranteed to be in lower-case.
  blink::WebReferrerPolicy policy = blink::WebReferrerPolicyDefault;
  if (referrerPolicy == "never")
    policy = blink::WebReferrerPolicyNever;
  if (referrerPolicy == "always")
    policy = blink::WebReferrerPolicyAlways;
  if (referrerPolicy == "origin")
    policy = blink::WebReferrerPolicyOrigin;

  in_progress_page_urls_.insert(page_url);

  Referrer referrer =
      Referrer::SanitizeForRequest(GURL(image_url), Referrer(page_url, policy));
  net::URLRequest::ReferrerPolicy referrer_policy =
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  if (!referrer.url.is_empty()) {
    switch (policy) {
      case blink::WebReferrerPolicyDefault:
        break;
      case blink::WebReferrerPolicyAlways:
      case blink::WebReferrerPolicyNever:
      case blink::WebReferrerPolicyOrigin:
        referrer_policy = net::URLRequest::NEVER_CLEAR_REFERRER;
        break;
      default:
        NOTREACHED();
    }
  }
  RetrieveSalientImage(page_url, GURL(image_url), referrer.url.spec(),
                       referrer_policy, update_bookmark);
}

void BookmarkImageServiceAndroid::BitmapFetcherHandler::Start(
    content::BrowserContext* browser_context,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy,
    int load_flags,
    bool update_bookmark,
    const GURL& page_url) {
  update_bookmark_ = update_bookmark;
  page_url_ = page_url;

  bitmap_fetcher_.Start(browser_context->GetRequestContext(), referrer,
                        referrer_policy, load_flags);
}

void BookmarkImageServiceAndroid::BitmapFetcherHandler::OnFetchComplete(
    const GURL url,
    const SkBitmap* bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  gfx::Image image;
  if (bitmap) {
    gfx::ImageSkia imageSkia = gfx::ImageSkia::CreateFrom1xBitmap(*bitmap);
    imageSkia.MakeThreadSafe();
    image = gfx::Image(imageSkia);
  }
  service_->ProcessNewImage(page_url_, update_bookmark_, image, url);

  delete this;
}

}  // namespace enhanced_bookmarks
