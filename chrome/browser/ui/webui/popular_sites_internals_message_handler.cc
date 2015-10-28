// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/popular_sites_internals_message_handler.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/android/popular_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/web_ui.h"

PopularSitesInternalsMessageHandler::PopularSitesInternalsMessageHandler() {
}

PopularSitesInternalsMessageHandler::~PopularSitesInternalsMessageHandler() {}

void PopularSitesInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("registerForEvents",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("download",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleDownload,
                 base::Unretained(this)));
}

void PopularSitesInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());

  std::string country;
  std::string version;
  popular_sites_.reset(new PopularSites(
      Profile::FromWebUI(web_ui()), country, version, false,
      base::Bind(&PopularSitesInternalsMessageHandler::OnPopularSitesAvailable,
                 base::Unretained(this), false)));
}

void PopularSitesInternalsMessageHandler::HandleDownload(
    const base::ListValue* args) {
  DCHECK_EQ(3u, args->GetSize());
  auto callback =
      base::Bind(&PopularSitesInternalsMessageHandler::OnPopularSitesAvailable,
                 base::Unretained(this), true);

  std::string url;
  args->GetString(0, &url);
  if (!url.empty()) {
    popular_sites_.reset(new PopularSites(
        Profile::FromWebUI(web_ui()),
        url_formatter::FixupURL(url, std::string()), callback));
    return;
  }
  std::string country;
  args->GetString(1, &country);
  std::string version;
  args->GetString(2, &version);
  popular_sites_.reset(new PopularSites(Profile::FromWebUI(web_ui()), country,
                                        version, true, callback));
}

void PopularSitesInternalsMessageHandler::SendDownloadResult(bool success) {
  base::StringValue result(success ? "Success" : "Fail");
  web_ui()->CallJavascriptFunction(
      "chrome.popular_sites_internals.receiveDownloadResult", result);
}

void PopularSitesInternalsMessageHandler::SendSites() {
  scoped_ptr<base::ListValue> sites_list(new base::ListValue);
  for (const PopularSites::Site& site : popular_sites_->sites()) {
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    entry->SetString("title", site.title);
    entry->SetString("url", site.url.spec());
    sites_list->Append(entry.Pass());
  }

  base::DictionaryValue result;
  result.Set("sites", sites_list.Pass());
  web_ui()->CallJavascriptFunction(
      "chrome.popular_sites_internals.receiveSites", result);
}

void PopularSitesInternalsMessageHandler::OnPopularSitesAvailable(
    bool explicit_request, bool success) {
  if (explicit_request)
    SendDownloadResult(success);
  SendSites();
}
