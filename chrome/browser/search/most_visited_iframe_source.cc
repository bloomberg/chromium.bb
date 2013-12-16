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
const char kLogHTMLPath[] = "/log.html";
const char kMostVisitedHistogramWithProvider[] = "NewTabPage.MostVisited.%s";

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
    int render_view_id,
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
    SendResource(IDR_MOST_VISITED_THUMBNAIL_JS, callback);
  } else if (path == kUtilJSPath) {
    SendResource(IDR_MOST_VISITED_UTIL_JS, callback);
  } else if (path == kCommonCSSPath) {
    SendResource(IDR_MOST_VISITED_IFRAME_CSS, callback);
  } else if (path == kLogHTMLPath) {
    // Log the clicked MostVisited element by position.
    std::string str_position;
    int position;
    if (net::GetValueForKeyInQuery(url, "pos", &str_position) &&
        base::StringToInt(str_position, &position)) {
      // Log the Most Visited click.
      UMA_HISTOGRAM_ENUMERATION(kMostVisitedHistogramName, position,
                                kNumMostVisited);
      // If a specific provider is specified, log the metric specific to the
      // provider.
      std::string provider;
      if (net::GetValueForKeyInQuery(url, "pr", &provider))
        LogMostVisitedProviderClick(position, provider);

      // Records the action. This will be available as a time-stamped stream
      // server-side and can be used to compute time-to-long-dwell.
      content::RecordAction(content::UserMetricsAction("MostVisited_Clicked"));
    }
    callback.Run(NULL);
  } else {
    callback.Run(NULL);
  }
}

bool MostVisitedIframeSource::ServesPath(const std::string& path) const {
  return path == kTitleHTMLPath || path == kTitleCSSPath ||
         path == kTitleJSPath || path == kThumbnailHTMLPath ||
         path == kThumbnailCSSPath || path == kThumbnailJSPath ||
         path == kUtilJSPath || path == kCommonCSSPath || path == kLogHTMLPath;
}

void MostVisitedIframeSource::LogMostVisitedProviderClick(
    int position,
    const std::string& provider) {
  std::string histogram_name =
      MostVisitedIframeSource::GetHistogramNameForProvider(provider);
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram_name, 1,
      MostVisitedIframeSource::kNumMostVisited,
      MostVisitedIframeSource::kNumMostVisited + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(position);
}

// static
std::string MostVisitedIframeSource::GetHistogramNameForProvider(
    const std::string& provider) {
  return base::StringPrintf(kMostVisitedHistogramWithProvider,
                            provider.c_str());
}
