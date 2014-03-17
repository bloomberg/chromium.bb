// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/most_visited_iframe_source.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "grit/browser_resources.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

const char kTitleHTMLPath[] = "/title.html";
const char kTitleCSSPath[] = "/title.css";
const char kTitleJSPath[] = "/title.js";
const char kThumbnailHTMLPath[] = "/thumbnail.html";
const char kThumbnailCSSPath[] = "/thumbnail.css";
const char kThumbnailJSPath[] = "/thumbnail.js";
const char kUtilJSPath[] = "/util.js";
const char kCommonCSSPath[] = "/common.css";

}  // namespace

MostVisitedIframeSource::MostVisitedIframeSource() {
}

MostVisitedIframeSource::~MostVisitedIframeSource() {
}

const int MostVisitedIframeSource::kNumMostVisited = 8;
const char MostVisitedIframeSource::kMostVisitedHistogramName[] =
    "NewTabPage.MostVisited";

std::string MostVisitedIframeSource::GetSource() const {
  return chrome::kChromeSearchMostVisitedHost;
}

void MostVisitedIframeSource::StartDataRequest(
    const std::string& path_and_query,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url(chrome::kChromeSearchMostVisitedUrl + path_and_query);
  std::string path(url.path());
  if (path == kTitleHTMLPath) {
    SendResource(IDR_MOST_VISITED_TITLE_HTML, callback);
  } else if (path == kTitleCSSPath) {
    SendResource(IDR_MOST_VISITED_TITLE_CSS, callback);
  } else if (path == kTitleJSPath) {
    SendResource(IDR_MOST_VISITED_TITLE_JS, callback);
  } else if (path == kThumbnailHTMLPath) {
    SendResource(IDR_MOST_VISITED_THUMBNAIL_HTML, callback);
  } else if (path == kThumbnailCSSPath) {
    SendResource(IDR_MOST_VISITED_THUMBNAIL_CSS, callback);
  } else if (path == kThumbnailJSPath) {
    SendJSWithOrigin(IDR_MOST_VISITED_THUMBNAIL_JS, render_process_id,
                     render_frame_id, callback);
  } else if (path == kUtilJSPath) {
    SendResource(IDR_MOST_VISITED_UTIL_JS, callback);
  } else if (path == kCommonCSSPath) {
    SendResource(IDR_MOST_VISITED_IFRAME_CSS, callback);
  } else {
    callback.Run(NULL);
  }
}

bool MostVisitedIframeSource::ServesPath(const std::string& path) const {
  return path == kTitleHTMLPath || path == kTitleCSSPath ||
         path == kTitleJSPath || path == kThumbnailHTMLPath ||
         path == kThumbnailCSSPath || path == kThumbnailJSPath ||
         path == kUtilJSPath || path == kCommonCSSPath;
}
