// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/local_ntp_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/resource/resource_bundle.h"

namespace local_ntp_test_utils {

content::WebContents* OpenNewTab(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  return browser->tab_strip_model()->GetActiveWebContents();
}

bool SwitchBrowserLanguageToFrench() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Make sure the default language is not French.
  std::string default_locale = g_browser_process->GetApplicationLocale();
  EXPECT_NE("fr", default_locale);

  // Switch browser language to French.
  g_browser_process->SetApplicationLocale("fr");
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, "fr");

  std::string loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("fr");

  return loaded_locale == "fr";
}

void SetUserSelectedDefaultSearchProvider(Profile* profile,
                                          const std::string& base_url,
                                          const std::string& ntp_url) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(base_url));
  data.SetKeyword(base::UTF8ToUTF16(base_url));
  data.SetURL(base_url + "url?bar={searchTerms}");
  data.new_tab_url = ntp_url;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}

}  // namespace local_ntp_test_utils
