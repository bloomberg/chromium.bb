// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_test_utils.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace {

std::string WrapScript(const std::string& script) {
  return "domAutomationController.send(" + script + ")";
}

}  // namespace

// InstantTestBase -----------------------------------------------------------

InstantTestBase::InstantTestBase()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
      init_suggestions_url_(false) {
  https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
}

InstantTestBase::~InstantTestBase() {}

void InstantTestBase::SetupInstant(Browser* browser) {
  browser_ = browser;

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(service);

  TemplateURLData data;
  // Necessary to use exact URL for both the main URL and the alternate URL for
  // search term extraction to work in InstantExtended.
  data.SetShortName(base::ASCIIToUTF16("name"));
  data.SetURL(instant_url_.spec() + "q={searchTerms}&is_search");
  data.instant_url = instant_url_.spec();
  data.new_tab_url = ntp_url_.spec();
  if (init_suggestions_url_)
    data.suggestions_url = instant_url_.spec() + "#q={searchTerms}";
  data.alternate_urls.push_back(instant_url_.spec() + "#q={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = service->Add(base::MakeUnique<TemplateURL>(data));
  service->SetUserSelectedDefaultSearchProvider(template_url);
}

void InstantTestBase::Init(const GURL& instant_url,
                           const GURL& ntp_url,
                           bool init_suggestions_url) {
  instant_url_ = instant_url;
  ntp_url_ = ntp_url;
  init_suggestions_url_ = init_suggestions_url;
}

void InstantTestBase::FocusOmnibox() {
  // If the omnibox already has focus, just notify SearchTabHelper.
  if (omnibox()->model()->has_focus()) {
    content::WebContents* active_tab =
        browser_->tab_strip_model()->GetActiveWebContents();
    SearchTabHelper::FromWebContents(active_tab)->OmniboxFocusChanged(
        OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  } else {
    browser_->window()->GetLocationBar()->FocusLocation(false);
  }
}

void InstantTestBase::SetOmniboxText(const std::string& text) {
  FocusOmnibox();
  omnibox()->SetUserText(base::UTF8ToUTF16(text));
}

void InstantTestBase::PressEnterAndWaitForNavigation() {
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  browser_->window()->GetLocationBar()->AcceptInput();
  nav_observer.Wait();
}

void InstantTestBase::PressEnterAndWaitForFrameLoad() {
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  browser_->window()->GetLocationBar()->AcceptInput();
  nav_observer.Wait();
}

bool InstantTestBase::GetBoolFromJS(const content::ToRenderFrameHost& adapter,
                                    const std::string& script,
                                    bool* result) {
  return content::ExecuteScriptAndExtractBool(adapter, WrapScript(script),
                                              result);
}

bool InstantTestBase::GetIntFromJS(const content::ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   int* result) {
  return content::ExecuteScriptAndExtractInt(adapter, WrapScript(script),
                                             result);
}

bool InstantTestBase::GetDoubleFromJS(const content::ToRenderFrameHost& adapter,
                                      const std::string& script,
                                      double* result) {
  return content::ExecuteScriptAndExtractDouble(adapter, WrapScript(script),
                                                result);
}

bool InstantTestBase::GetStringFromJS(const content::ToRenderFrameHost& adapter,
                                      const std::string& script,
                                      std::string* result) {
  return content::ExecuteScriptAndExtractString(adapter, WrapScript(script),
                                                result);
}

std::string InstantTestBase::GetOmniboxText() {
  return base::UTF16ToUTF8(omnibox()->GetText());
}

bool InstantTestBase::LoadImage(content::RenderViewHost* rvh,
                                const std::string& image,
                                bool* loaded) {
  std::string js_chrome =
      "var img = document.createElement('img');"
      "img.onerror = function() { domAutomationController.send(false); };"
      "img.onload  = function() { domAutomationController.send(true); };"
      "img.src = '" + image + "';";
  return content::ExecuteScriptAndExtractBool(rvh, js_chrome, loaded);
}
