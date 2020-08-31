// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/chrome_media_app_ui_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "url/gurl.h"

ChromeMediaAppUIDelegate::ChromeMediaAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

base::Optional<std::string> ChromeMediaAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kMediaAppFeedbackCategoryTag[] = "FromMediaApp";

  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in.
  chrome::ShowFeedbackPage(GURL(chromeos::kChromeUIMediaAppURL), profile,
                           chrome::kFeedbackSourceMediaApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kMediaAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);

  // TODO(crbug/1048368): Showing the feedback dialog can fail, communicate this
  // back to the client with an error string. For now assume dialog opened.
  return base::nullopt;
}
