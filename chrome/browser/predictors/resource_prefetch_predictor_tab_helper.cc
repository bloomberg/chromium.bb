// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tab_helper.h"

#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    predictors::ResourcePrefetchPredictorTabHelper);

using content::BrowserThread;

namespace predictors {

ResourcePrefetchPredictorTabHelper::ResourcePrefetchPredictorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

ResourcePrefetchPredictorTabHelper::~ResourcePrefetchPredictorTabHelper() {
}

void ResourcePrefetchPredictorTabHelper::DocumentOnLoadCompletedInMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ResourcePrefetchPredictor* predictor =
      ResourcePrefetchPredictorFactory::GetForProfile(
          web_contents()->GetBrowserContext());
  if (!predictor)
    return;

  NavigationID navigation_id(web_contents());
  predictor->RecordMainFrameLoadComplete(navigation_id);
}

void ResourcePrefetchPredictorTabHelper::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& mime_type,
    content::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ResourcePrefetchPredictor* predictor =
      ResourcePrefetchPredictorFactory::GetForProfile(
          web_contents()->GetBrowserContext());
  if (!predictor)
    return;

  ResourcePrefetchPredictor::URLRequestSummary summary;
  summary.navigation_id = NavigationID(web_contents());
  summary.resource_url = url;
  summary.mime_type = mime_type;
  summary.resource_type =
      ResourcePrefetchPredictor::GetResourceTypeFromMimeType(
          mime_type, resource_type);
  summary.was_cached = true;
  predictor->RecordURLResponse(summary);
}

}  // namespace predictors
