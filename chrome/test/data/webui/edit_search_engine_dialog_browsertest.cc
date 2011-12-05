// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/edit_search_engine_dialog_browsertest.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/edit_search_engine_dialog_webui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_html_dialog_observer.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"

namespace {

// Creates a Template URL for testing purposes.
TemplateURL* CreateTestTemplateURL(int seed) {
  TemplateURL* turl = new TemplateURL();
  turl->SetURL(base::StringPrintf("http://www.test%d.com/", seed), 0, 0);
  turl->set_keyword(ASCIIToUTF16(base::StringPrintf("keyword%d", seed)));
  turl->set_short_name(ASCIIToUTF16(base::StringPrintf("short%d", seed)));
  turl->set_safe_for_autoreplace(true);
  GURL favicon_url("http://favicon.url");
  turl->SetFaviconURL(favicon_url);
  turl->set_date_created(base::Time::FromTimeT(100));
  turl->set_last_modified(base::Time::FromTimeT(100));
  turl->SetPrepopulateId(999999);
  turl->set_sync_guid(base::StringPrintf("0000-0000-0000-%04d", seed));
  return turl;
}

}  // namespace

EditSearchEngineDialogUITest::EditSearchEngineDialogUITest() {}

EditSearchEngineDialogUITest::~EditSearchEngineDialogUITest() {}

void EditSearchEngineDialogUITest::ShowSearchEngineDialog() {
  // Force the flag so that we will use the WebUI version of the Dialog.
  ChromeWebUI::OverrideMoreWebUI(true);

  // The TestHtmlDialogObserver will catch our dialog when it gets created.
  TestHtmlDialogObserver dialog_observer(this);

  // Show the Edit Search Engine Dialog.
  EditSearchEngineDialogWebUI::ShowEditSearchEngineDialog(
      CreateTestTemplateURL(0),
      browser()->profile());

  // Now we can get the WebUI object from the observer, and make some details
  // about our test available to the JavaScript.
  WebUI* webui = dialog_observer.GetWebUI();
  webui->tab_contents()->render_view_host()->SetWebUIProperty(
      "expectedUrl", chrome::kChromeUIEditSearchEngineDialogURL);

  // Tell the test which WebUI instance we are dealing with and complete
  // initialization of this test.
  SetWebUIInstance(webui);
  WebUIBrowserTest::SetUpOnMainThread();
}
