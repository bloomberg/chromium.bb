// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"

#include <set>

#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/password_manager_internals_resources.h"
#include "net/base/escape.h"

using content::BrowserContext;
using content::WebContents;

namespace {

content::WebUIDataSource* CreatePasswordManagerInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPasswordManagerInternalsHost);

  source->AddResourcePath(
      "password_manager_internals.js",
      IDR_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_JS);
  source->SetDefaultResource(
      IDR_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_HTML);
  return source;
}

void InsertWebContentsIfProfileMatches(
    WebContents* web_contents,
    const Profile* profile_to_match,
    std::set<WebContents*>* set_of_web_contents) {
  if (static_cast<const BrowserContext*>(profile_to_match) ==
      web_contents->GetBrowserContext()) {
    set_of_web_contents->insert(web_contents);
  }
}

}  // namespace

PasswordManagerInternalsUI::PasswordManagerInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://password-manager-internals/ source.
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePasswordManagerInternalsHTMLSource());
  NotifyAllPasswordManagerClients(PAGE_OPENED);
}

PasswordManagerInternalsUI::~PasswordManagerInternalsUI() {
  NotifyAllPasswordManagerClients(PAGE_CLOSED);
}

void PasswordManagerInternalsUI::LogSavePasswordProgress(
    const std::string& text) {
  base::StringValue text_string_value(net::EscapeForHTML(text));
  web_ui()->CallJavascriptFunction("addSavePasswordProgressLog",
                                   text_string_value);
}

void PasswordManagerInternalsUI::NotifyAllPasswordManagerClients(
    ClientNotificationType notification_type) {
  // First, find all the WebContents objects of the current profile.
  Profile* current_profile = Profile::FromWebUI(web_ui());
  std::set<WebContents*> profile_web_contents;
#if defined(OS_ANDROID)
  for (TabModelList::const_iterator iter = TabModelList::begin();
       iter != TabModelList::end();
       ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      InsertWebContentsIfProfileMatches(
          model->GetWebContentsAt(i), current_profile, &profile_web_contents);
    }
  }
#else
  for (TabContentsIterator iter; !iter.done(); iter.Next()) {
    InsertWebContentsIfProfileMatches(
        *iter, current_profile, &profile_web_contents);
  }
#endif

  // Now get the corresponding PasswordManagerClients, and attach/detach |this|.
  for (std::set<WebContents*>::iterator it = profile_web_contents.begin();
       it != profile_web_contents.end();
       ++it) {
    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(*it);
    switch (notification_type) {
      case PAGE_OPENED:
        client->SetLogger(this);
        break;
      case PAGE_CLOSED:
        client->SetLogger(NULL);
        break;
    }
  }
}
