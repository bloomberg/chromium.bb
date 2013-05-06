// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_ntp_source.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

namespace {

// Signifies a locally constructed resource, i.e. not from grit/.
const int kLocalResource = -1;

const char kTranslatedStringsFilename[] = "translated-strings.js";

const struct Resource{
  const char* filename;
  int identifier;
  const char* mime_type;
} kResources[] = {
  { "local-ntp.html", IDR_LOCAL_NTP_HTML, "text/html" },
  { "local-ntp.js", IDR_LOCAL_NTP_JS, "application/javascript" },
  { kTranslatedStringsFilename, kLocalResource, "application/javascript" },
  { "local-ntp.css", IDR_LOCAL_NTP_CSS, "text/css" },
  { "images/close_2.png", IDR_CLOSE_2, "image/png" },
  { "images/close_2_hover.png", IDR_CLOSE_2_H, "image/png" },
  { "images/close_2_active.png", IDR_CLOSE_2_P, "image/png" },
  { "images/page_icon.png", IDR_LOCAL_OMNIBOX_POPUP_IMAGES_PAGE_ICON_PNG,
    "image/png" },
  { "images/2x/page_icon.png",
    IDR_LOCAL_OMNIBOX_POPUP_IMAGES_2X_PAGE_ICON_PNG, "image/png" },
  { "images/search_icon.png",
    IDR_LOCAL_OMNIBOX_POPUP_IMAGES_SEARCH_ICON_PNG, "image/png" },
  { "images/2x/search_icon.png",
    IDR_LOCAL_OMNIBOX_POPUP_IMAGES_2X_SEARCH_ICON_PNG, "image/png" },
  { "images/google_logo.png", IDR_LOCAL_NTP_IMAGES_LOGO_PNG, "image/png" },
  { "images/2x/google_logo.png",
    IDR_LOCAL_NTP_IMAGES_2X_LOGO_PNG, "image/png" },
  { "images/white_google_logo.png",
    IDR_LOCAL_NTP_IMAGES_WHITE_LOGO_PNG, "image/png" },
  { "images/2x/white_google_logo.png",
    IDR_LOCAL_NTP_IMAGES_2X_WHITE_LOGO_PNG, "image/png" },
};

// Strips any query parameters from the specified path.
std::string StripParameters(const std::string& path) {
  return path.substr(0, path.find("?"));
}

// Adds a localized string keyed by resource id to the dictionary.
void AddString(base::DictionaryValue* dictionary,
               const std::string& key,
               int resource_id) {
  dictionary->SetString(key, l10n_util::GetStringUTF16(resource_id));
}

// Returns a JS dictionary of translated strings for the local NTP.
std::string GetTranslatedStrings() {
  base::DictionaryValue translated_strings;
  AddString(&translated_strings, "thumbnailRemovedNotification",
            IDS_NEW_TAB_THUMBNAIL_REMOVED_NOTIFICATION);
  AddString(&translated_strings, "removeThumbnailTooltip",
            IDS_NEW_TAB_REMOVE_THUMBNAIL_TOOLTIP);
  AddString(&translated_strings, "undoThumbnailRemove",
            IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  AddString(&translated_strings, "restoreThumbnailsShort",
            IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK);
  AddString(&translated_strings, "attributionIntro",
            IDS_NEW_TAB_ATTRIBUTION_INTRO);
  AddString(&translated_strings, "title", IDS_NEW_TAB_TITLE);
  std::string translated_strings_js;
  webui::AppendJsonJS(&translated_strings, &translated_strings_js);
  return translated_strings_js;
}

}  // namespace

LocalNtpSource::LocalNtpSource() {
}

LocalNtpSource::~LocalNtpSource() {
}

std::string LocalNtpSource::GetSource() const {
  return chrome::kChromeSearchLocalNtpHost;
}

void LocalNtpSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_view_id,
    const content::URLDataSource::GotDataCallback& callback) {
  const std::string stripped_path = StripParameters(path);
  if (stripped_path == kTranslatedStringsFilename) {
    std::string translated_strings_js = GetTranslatedStrings();
    callback.Run(base::RefCountedString::TakeString(&translated_strings_js));
    return;
  }
  for (size_t i = 0; i < arraysize(kResources); ++i) {
    if (stripped_path == kResources[i].filename) {
      scoped_refptr<base::RefCountedStaticMemory> response(
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              kResources[i].identifier));
      callback.Run(response);
      return;
    }
  }
  callback.Run(NULL);
};

std::string LocalNtpSource::GetMimeType(
    const std::string& path) const {
  const std::string stripped_path = StripParameters(path);
  for (size_t i = 0; i < arraysize(kResources); ++i) {
    if (stripped_path == kResources[i].filename)
      return kResources[i].mime_type;
  }
  return std::string();
}

bool LocalNtpSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  DCHECK(request->url().host() == chrome::kChromeSearchLocalNtpHost);

  if (request->url().SchemeIs(chrome::kChromeSearchScheme)) {
    DCHECK(StartsWithASCII(request->url().path(), "/", true));
    std::string filename = request->url().path().substr(1);
    for (size_t i = 0; i < arraysize(kResources); ++i) {
      if (filename == kResources[i].filename)
        return true;
    }
  }
  return false;
}

std::string LocalNtpSource::GetContentSecurityPolicyFrameSrc() const {
  // Allow embedding of suggestion and most visited iframes.
  return base::StringPrintf("frame-src %s %s;",
                            chrome::kChromeSearchSuggestionUrl,
                            chrome::kChromeSearchMostVisitedUrl);
}
