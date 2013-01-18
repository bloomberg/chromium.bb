// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_test_utils.h"

#include "base/command_line.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test_utils.h"

namespace {

std::string WrapScript(const std::string& script) {
  return "domAutomationController.send(" + script + ")";
}

}  // namespace

// InstantTestModelObserver --------------------------------------------------

InstantTestModelObserver::InstantTestModelObserver(
    const InstantModel* model,
    chrome::search::Mode::Type desired_mode_type)
    : model_(model),
      desired_mode_type_(desired_mode_type) {
  model_->AddObserver(this);
}

InstantTestModelObserver::~InstantTestModelObserver() {
  model_->RemoveObserver(this);
}

void InstantTestModelObserver::WaitUntilDesiredPreviewState() {
  run_loop_.Run();
}

void InstantTestModelObserver::PreviewStateChanged(const InstantModel& model) {
  if (model.mode().mode == desired_mode_type_)
    run_loop_.Quit();
}

// InstantTestBase -----------------------------------------------------------

void InstantTestBase::SetupInstant() {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kInstantURL, instant_url_.spec());
  SetupInstantUsingTemplateURL();
}

void InstantTestBase::SetupInstantUsingTemplateURL() {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);

  TemplateURLData data;
  data.SetURL("http://does/not/exist?q={searchTerms}");
  data.instant_url = instant_url_.spec();
  data.safe_for_autoreplace = true;

  TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
  service->Add(template_url);  // Takes ownership of |template_url|.
  service->SetDefaultSearchProvider(template_url);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kInstantEnabled, true);
}

void InstantTestBase::KillInstantRenderView() {
  base::KillProcess(
      instant()->GetPreviewContents()->GetRenderProcessHost()->GetHandle(),
      content::RESULT_CODE_KILLED,
      false);
}

void InstantTestBase::FocusOmnibox() {
  // If the omnibox already has focus, just notify Instant.
  if (omnibox()->model()->has_focus()) {
    instant()->OmniboxFocusChanged(OMNIBOX_FOCUS_VISIBLE,
                                   OMNIBOX_FOCUS_CHANGE_EXPLICIT, NULL);
  } else {
    browser()->window()->GetLocationBar()->FocusLocation(false);
  }
}

void InstantTestBase::FocusOmniboxAndWaitForInstantSupport() {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  FocusOmnibox();
  observer.Wait();
}

void InstantTestBase::SetOmniboxText(const std::string& text) {
  FocusOmnibox();
  omnibox()->SetUserText(UTF8ToUTF16(text));
}

void InstantTestBase::SetOmniboxTextAndWaitForInstantToShow(
    const std::string& text) {
  InstantTestModelObserver observer(
      instant()->model(), chrome::search::Mode::MODE_SEARCH_SUGGESTIONS);
  SetOmniboxText(text);
  observer.WaitUntilDesiredPreviewState();
}

bool InstantTestBase::GetBoolFromJS(content::WebContents* contents,
                                    const std::string& script,
                                    bool* result) {
  return content::ExecuteScriptAndExtractBool(
      contents, WrapScript(script), result);
}

bool InstantTestBase::GetIntFromJS(content::WebContents* contents,
                                   const std::string& script,
                                   int* result) {
  return content::ExecuteScriptAndExtractInt(
      contents, WrapScript(script), result);
}

bool InstantTestBase::GetStringFromJS(content::WebContents* contents,
                                      const std::string& script,
                                      std::string* result) {
  return content::ExecuteScriptAndExtractString(
      contents, WrapScript(script), result);
}

bool InstantTestBase::ExecuteScript(const std::string& script) {
  return content::ExecuteScript(instant()->GetPreviewContents(), script);
}

bool InstantTestBase::CheckVisibilityIs(content::WebContents* contents,
                                        bool expected) {
  bool actual = !expected;  // Purposely start with a mis-match.
  // We can only use ASSERT_*() in a method that returns void, hence this
  // convoluted check.
  return GetBoolFromJS(contents, "!document.webkitHidden", &actual) &&
      actual == expected;
}
