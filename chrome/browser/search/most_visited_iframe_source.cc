// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/most_visited_iframe_source.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "chrome/browser/search/local_files_ntp_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/local_ntp_resources.h"
#include "url/gurl.h"

namespace {

// Single-iframe version, used by the local NTP and the Google remote NTP.
const char kSingleHTMLPath[] = "/single.html";
const char kSingleCSSPath[] = "/single.css";
const char kSingleJSPath[] = "/single.js";

// Multi-iframe version, used by third party remote NTPs.
const char kTitleHTMLPath[] = "/title.html";
const char kTitleCSSPath[] = "/title.css";
const char kTitleJSPath[] = "/title.js";
const char kUtilJSPath[] = "/util.js";
const char kCommonCSSPath[] = "/common.css";

// Edit custom links dialog iframe and resources, used by the local NTP and the
// Google remote NTP.
const char kEditHTMLPath[] = "/edit.html";
const char kEditCSSPath[] = "/edit.css";
const char kEditJSPath[] = "/edit.js";
const char kAddSvgPath[] = "/add_link.svg";
const char kEditMenuSvgPath[] = "/edit_menu.svg";

}  // namespace

MostVisitedIframeSource::MostVisitedIframeSource() = default;

MostVisitedIframeSource::~MostVisitedIframeSource() = default;

std::string MostVisitedIframeSource::GetSource() const {
  return chrome::kChromeSearchMostVisitedHost;
}

void MostVisitedIframeSource::StartDataRequest(
    const std::string& path_and_query,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL url(chrome::kChromeSearchMostVisitedUrl + path_and_query);
  std::string path(url.path());

#if !defined(GOOGLE_CHROME_BUILD)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLocalNtpReload)) {
    std::string rel_path = "most_visited_" + path.substr(1);
    if (path == kSingleJSPath) {
      std::string origin;
      if (!GetOrigin(wc_getter, &origin)) {
        callback.Run(nullptr);
        return;
      }
      local_ntp::SendLocalFileResourceWithOrigin(rel_path, origin, callback);
    } else {
      local_ntp::SendLocalFileResource(rel_path, callback);
    }
    return;
  }
#endif

  if (path == kSingleHTMLPath) {
    SendResource(IDR_MOST_VISITED_SINGLE_HTML, callback);
  } else if (path == kSingleCSSPath) {
    SendResource(IDR_MOST_VISITED_SINGLE_CSS, callback);
  } else if (path == kSingleJSPath) {
    SendJSWithOrigin(IDR_MOST_VISITED_SINGLE_JS, wc_getter, callback);
  } else if (path == kTitleHTMLPath) {
    SendResource(IDR_MOST_VISITED_TITLE_HTML, callback);
  } else if (path == kTitleCSSPath) {
    SendResource(IDR_MOST_VISITED_TITLE_CSS, callback);
  } else if (path == kTitleJSPath) {
    SendResource(IDR_MOST_VISITED_TITLE_JS, callback);
  } else if (path == kUtilJSPath) {
    SendJSWithOrigin(IDR_MOST_VISITED_UTIL_JS, wc_getter, callback);
  } else if (path == kCommonCSSPath) {
    SendResource(IDR_MOST_VISITED_IFRAME_CSS, callback);
  } else if (path == kEditHTMLPath) {
    SendResource(IDR_CUSTOM_LINKS_EDIT_HTML, callback);
  } else if (path == kEditCSSPath) {
    SendResource(IDR_CUSTOM_LINKS_EDIT_CSS, callback);
  } else if (path == kEditJSPath) {
    SendJSWithOrigin(IDR_CUSTOM_LINKS_EDIT_JS, wc_getter, callback);
  } else if (path == kAddSvgPath) {
    SendResource(IDR_CUSTOM_LINKS_ADD_SVG, callback);
  } else if (path == kEditMenuSvgPath) {
    SendResource(IDR_CUSTOM_LINKS_EDIT_MENU_SVG, callback);
  } else {
    callback.Run(nullptr);
  }
}

bool MostVisitedIframeSource::ServesPath(const std::string& path) const {
  return path == kSingleHTMLPath || path == kSingleCSSPath ||
         path == kSingleJSPath || path == kTitleHTMLPath ||
         path == kTitleCSSPath || path == kTitleJSPath || path == kUtilJSPath ||
         path == kCommonCSSPath || path == kEditHTMLPath ||
         path == kEditCSSPath || path == kEditJSPath || path == kAddSvgPath ||
         path == kEditMenuSvgPath;
}
