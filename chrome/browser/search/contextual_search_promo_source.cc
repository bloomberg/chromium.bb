// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/contextual_search_promo_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

const char kPromoHTMLPath[] = "/promo.html";
const char kPromoCSSPath[] = "/promo.css";
const char kPromoJSPath[] = "/promo.js";

}  // namespace

ContextualSearchPromoSource::ContextualSearchPromoSource() {}

ContextualSearchPromoSource::~ContextualSearchPromoSource() {}

void ContextualSearchPromoSource::StartDataRequest(
    const std::string& path_and_query, int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url(std::string(chrome::kChromeUIContextualSearchPromoURL) + "/" +
           path_and_query);
  std::string path(url.path());
  if (path == kPromoHTMLPath) {
    SendResource(IDR_CONTEXTUAL_SEARCH_PROMO_HTML, callback);
  } else if (path == kPromoCSSPath) {
    SendResource(IDR_CONTEXTUAL_SEARCH_PROMO_CSS, callback);
  } else if (path == kPromoJSPath) {
    SendResource(IDR_CONTEXTUAL_SEARCH_PROMO_JS, callback);
  } else {
    callback.Run(NULL);
  }
}

std::string ContextualSearchPromoSource::GetSource() const {
  return chrome::kChromeUIContextualSearchPromoHost;
}

std::string ContextualSearchPromoSource::GetMimeType(
    const std::string& path_and_query) const {
  std::string path(GURL("chrome://host/" + path_and_query).path());
  if (EndsWith(path, ".js", false)) return "application/javascript";
  if (EndsWith(path, ".png", false)) return "image/png";
  if (EndsWith(path, ".css", false)) return "text/css";
  if (EndsWith(path, ".html", false)) return "text/html";
  return "";
}

bool ContextualSearchPromoSource::ShouldDenyXFrameOptions() const {
  return false;
}

void ContextualSearchPromoSource::SendResource(
    int resource_id, const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id));
  callback.Run(response.get());
}
