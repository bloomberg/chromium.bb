// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/contextual_search_promo_source_android.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "grit/browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/gurl.h"

namespace {

const char kPromoHTMLPath[] = "/promo.html";
const char kPromoCSSPath[] = "/promo.css";
const char kPromoJSPath[] = "/promo.js";
const char kRobotoWoffPath[] = "/roboto.woff";
const char kRobotoWoff2Path[] = "/roboto.woff2";

}  // namespace

ContextualSearchPromoSourceAndroid::ContextualSearchPromoSourceAndroid() {}

ContextualSearchPromoSourceAndroid::~ContextualSearchPromoSourceAndroid() {}

void ContextualSearchPromoSourceAndroid::StartDataRequest(
    const std::string& path_and_query, int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url(std::string(chrome::kChromeUIContextualSearchPromoURL) + "/" +
           path_and_query);
  std::string path(url.path());
  if (path == kPromoHTMLPath) {
    SendHtmlWithStrings(callback);
  } else if (path == kPromoCSSPath) {
    SendResource(IDR_CONTEXTUAL_SEARCH_PROMO_CSS, callback);
  } else if (path == kPromoJSPath) {
    SendResource(IDR_CONTEXTUAL_SEARCH_PROMO_JS, callback);
  } else if (path == kRobotoWoffPath) {
    SendResource(IDR_ROBOTO_WOFF, callback);
  } else if (path == kRobotoWoff2Path) {
    SendResource(IDR_ROBOTO_WOFF2, callback);
  } else {
    callback.Run(NULL);
  }
}

std::string ContextualSearchPromoSourceAndroid::GetSource() const {
  return chrome::kChromeUIContextualSearchPromoHost;
}

std::string ContextualSearchPromoSourceAndroid::GetMimeType(
    const std::string& path_and_query) const {
  std::string path(GURL("chrome://host/" + path_and_query).path());
  if (EndsWith(path, ".js", false)) return "application/javascript";
  if (EndsWith(path, ".png", false)) return "image/png";
  if (EndsWith(path, ".css", false)) return "text/css";
  if (EndsWith(path, ".html", false)) return "text/html";
  if (EndsWith(path, ".woff", false)) return "font/woff";
  if (EndsWith(path, ".woff2", false)) return "font/woff2";
  return "";
}

bool ContextualSearchPromoSourceAndroid::ShouldDenyXFrameOptions() const {
  return false;
}

bool
ContextualSearchPromoSourceAndroid::ShouldAddContentSecurityPolicy() const {
  return false;
}

void ContextualSearchPromoSourceAndroid::SendResource(
    int resource_id, const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id));
  callback.Run(response.get());
}

void ContextualSearchPromoSourceAndroid::SendHtmlWithStrings(
    const content::URLDataSource::GotDataCallback& callback) {
  base::DictionaryValue strings_data;
  strings_data.SetString(
      "description",
      l10n_util::GetStringUTF16(IDS_CONTEXTUAL_SEARCH_PROMO_DESCRIPTION));
  strings_data.SetString(
      "optIn", l10n_util::GetStringUTF16(IDS_CONTEXTUAL_SEARCH_PROMO_OPTIN));
  strings_data.SetString(
      "optOut", l10n_util::GetStringUTF16(IDS_CONTEXTUAL_SEARCH_PROMO_OPTOUT));
  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
         IDR_CONTEXTUAL_SEARCH_PROMO_HTML));
  webui::UseVersion2 version;
  std::string response(webui::GetI18nTemplateHtml(html, &strings_data));
  callback.Run(base::RefCountedString::TakeString(&response));
}
