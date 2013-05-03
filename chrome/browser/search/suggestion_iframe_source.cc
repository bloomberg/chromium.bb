// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestion_iframe_source.h"

#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"

namespace {

const char kLoaderHtmlPath[] = "/loader.html";
const char kLoaderJSPath[] = "/loader.js";
const char kResultHtmlPath[] = "/result.html";
const char kResultJSPath[] = "/result.js";

}  // namespace

SuggestionIframeSource::SuggestionIframeSource() {
}

SuggestionIframeSource::~SuggestionIframeSource() {
}

std::string SuggestionIframeSource::GetSource() const {
  return chrome::kChromeSearchSuggestionHost;
}

void SuggestionIframeSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_view_id,
    const content::URLDataSource::GotDataCallback& callback) {
  // path has no leading '/' and includes any query string. Since we don't
  // expect a query string for requests to this source, just add back the '/'.
  std::string full_path = "/" + path;
  if (full_path == kLoaderHtmlPath) {
    SendResource(IDR_OMNIBOX_RESULT_LOADER_HTML, callback);
  } else if (full_path == kLoaderJSPath) {
    SendJSWithOrigin(IDR_OMNIBOX_RESULT_LOADER_JS, render_process_id,
                     render_view_id, callback);
  } else if (full_path == kResultHtmlPath) {
    SendResource(IDR_OMNIBOX_RESULT_HTML, callback);
  } else if (full_path == kResultJSPath) {
    SendJSWithOrigin(IDR_OMNIBOX_RESULT_JS, render_process_id, render_view_id,
                     callback);
  } else {
    callback.Run(NULL);
  }
}

bool SuggestionIframeSource::ServesPath(const std::string& path) const {
  return path == kLoaderHtmlPath || path == kLoaderJSPath ||
         path == kResultHtmlPath || path == kResultJSPath;
}
