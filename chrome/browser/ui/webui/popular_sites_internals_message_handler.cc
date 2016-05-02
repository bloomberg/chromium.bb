// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/popular_sites_internals_message_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/android/ntp/popular_sites.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

namespace {

std::string ReadFileToString(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result))
    result.clear();
  return result;
}

}  // namespace

PopularSitesInternalsMessageHandler::PopularSitesInternalsMessageHandler()
    : weak_ptr_factory_(this) {}

PopularSitesInternalsMessageHandler::~PopularSitesInternalsMessageHandler() {}

void PopularSitesInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("registerForEvents",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("download",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleDownload,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "viewJson",
      base::Bind(&PopularSitesInternalsMessageHandler::HandleViewJson,
                 base::Unretained(this)));
}

void PopularSitesInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());

  std::string country;
  std::string version;
  Profile* profile = Profile::FromWebUI(web_ui());
  popular_sites_.reset(new PopularSites(
      profile->GetPrefs(),
      TemplateURLServiceFactory::GetForProfile(profile),
      g_browser_process->variations_service(),
      profile->GetRequestContext(),
      country, version, false,
      base::Bind(&PopularSitesInternalsMessageHandler::OnPopularSitesAvailable,
                 base::Unretained(this), false)));
}

void PopularSitesInternalsMessageHandler::HandleDownload(
    const base::ListValue* args) {
  DCHECK_EQ(3u, args->GetSize());
  Profile* profile = Profile::FromWebUI(web_ui());
  auto callback =
      base::Bind(&PopularSitesInternalsMessageHandler::OnPopularSitesAvailable,
                 base::Unretained(this), true);

  std::string url;
  args->GetString(0, &url);
  if (!url.empty()) {
    popular_sites_.reset(new PopularSites(
        profile->GetPrefs(),
        TemplateURLServiceFactory::GetForProfile(profile),
        profile->GetRequestContext(),
        url_formatter::FixupURL(url, std::string()), callback));
    return;
  }
  std::string country;
  args->GetString(1, &country);
  std::string version;
  args->GetString(2, &version);
  popular_sites_.reset(new PopularSites(
      profile->GetPrefs(),
      TemplateURLServiceFactory::GetForProfile(profile),
      g_browser_process->variations_service(),
      profile->GetRequestContext(),
      country, version, true, callback));
}

void PopularSitesInternalsMessageHandler::HandleViewJson(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  const base::FilePath& path = popular_sites_->local_path();
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get(),
      FROM_HERE, base::Bind(&ReadFileToString, path),
      base::Bind(&PopularSitesInternalsMessageHandler::SendJson,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PopularSitesInternalsMessageHandler::SendDownloadResult(bool success) {
  base::StringValue result(success ? "Success" : "Fail");
  web_ui()->CallJavascriptFunction(
      "chrome.popular_sites_internals.receiveDownloadResult", result);
}

void PopularSitesInternalsMessageHandler::SendSites() {
  std::unique_ptr<base::ListValue> sites_list(new base::ListValue);
  for (const PopularSites::Site& site : popular_sites_->sites()) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    entry->SetString("title", site.title);
    entry->SetString("url", site.url.spec());
    sites_list->Append(std::move(entry));
  }

  base::DictionaryValue result;
  result.Set("sites", std::move(sites_list));
  result.SetString("country", popular_sites_->GetCountry());
  result.SetString("version", popular_sites_->GetVersion());
  web_ui()->CallJavascriptFunction(
      "chrome.popular_sites_internals.receiveSites", result);
}

void PopularSitesInternalsMessageHandler::SendJson(const std::string& json) {
  web_ui()->CallJavascriptFunction("chrome.popular_sites_internals.receiveJson",
                                   base::StringValue(json));
}

void PopularSitesInternalsMessageHandler::OnPopularSitesAvailable(
    bool explicit_request, bool success) {
  if (explicit_request)
    SendDownloadResult(success);
  SendSites();
}
