// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/quota_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/json_value_serializer.h"
#include "grit/quota_internals_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

QuotaInternalsUI::QuotaInternalsUI(TabContents* contents)
    : ChromeWebUI(contents) {
  WebUIMessageHandler* handler = new quota_internals::QuotaInternalsHandler;
  AddMessageHandler(handler->Attach(this));
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      new quota_internals::QuotaInternalsHTMLSource);
}

namespace quota_internals {

const char QuotaInternalsHTMLSource::kStringsJSPath[] = "strings.js";

QuotaInternalsHTMLSource::QuotaInternalsHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUIQuotaInternalsHost) {
}

void QuotaInternalsHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_incognito,
                                                int request_id) {
  if (path == kStringsJSPath)
    SendLocalizedStringsAsJSON(request_id);
  else
    SendFromResourceBundle(request_id, IDR_QUOTA_INTERNALS_MAIN_HTML);
}

std::string QuotaInternalsHTMLSource::GetMimeType(
    const std::string& path) const {
  if (path == kStringsJSPath)
    return "application/javascript";
  else
    return "text/html";
}

}  // namespace quota_internals
