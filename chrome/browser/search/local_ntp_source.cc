// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_ntp_source.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/ui_resources.h"
#include "net/url_request/url_request.h"

namespace {

const char kHtmlFilename[] = "local-ntp.html";
const char kJSFilename[] = "local-ntp.js";
const char kCssFilename[] = "local-ntp.css";
const char kCloseBarFilename[] = "images/close_bar.png";
const char kCloseBarHoverFilename[] = "images/close_bar_hover.png";
const char kCloseBarActiveFilename[] = "images/close_bar_active.png";

}  // namespace

LocalNtpSource::LocalNtpSource() {
}

LocalNtpSource::~LocalNtpSource() {
}

std::string LocalNtpSource::GetSource() {
  return chrome::kChromeSearchLocalNtpHost;
}

void LocalNtpSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  int identifier = -1;
  if (path == kHtmlFilename) {
    identifier = IDR_LOCAL_NTP_HTML;
  } else if (path == kJSFilename) {
    identifier = IDR_LOCAL_NTP_JS;
  } else if (path == kCssFilename) {
    identifier = IDR_LOCAL_NTP_CSS;
  } else if (path == kCloseBarFilename) {
    identifier = IDR_CLOSE_BAR;
  } else if (path == kCloseBarHoverFilename) {
    identifier = IDR_CLOSE_BAR_H;
  } else if (path == kCloseBarActiveFilename) {
    identifier = IDR_CLOSE_BAR_P;
  } else {
    callback.Run(NULL);
    return;
  }

  scoped_refptr<base::RefCountedStaticMemory> response(
      content::GetContentClient()->GetDataResourceBytes(identifier));
  callback.Run(response);
}

std::string LocalNtpSource::GetMimeType(
    const std::string& path) const {
  if (path == kHtmlFilename)
    return "text/html";
  if (path == kJSFilename)
    return "application/javascript";
  if (path == kCssFilename)
    return "text/css";
  if (path == kCloseBarFilename || path == kCloseBarHoverFilename ||
      path == kCloseBarActiveFilename) {
    return "image/png";
  }
  return "";
}

bool LocalNtpSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  DCHECK(request->url().host() == chrome::kChromeSearchLocalNtpHost);

  if (request->url().SchemeIs(chrome::kChromeSearchScheme)) {
    DCHECK(StartsWithASCII(request->url().path(), "/", true));
    std::string filename = request->url().path().substr(1);
    return filename == kHtmlFilename || filename == kJSFilename ||
           filename == kCssFilename || filename == kCloseBarFilename ||
           filename == kCloseBarHoverFilename ||
           filename == kCloseBarActiveFilename;
  }
  return false;
}
