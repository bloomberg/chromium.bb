// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/snippets_internals_message_handler.h"

#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace {

std::unique_ptr<base::DictionaryValue> PrepareSnippet(
    const ntp_snippets::NTPSnippet& snippet,
    int index,
    bool discarded) {
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->SetString("title", snippet.title());
  entry->SetString("siteTitle", snippet.best_source().publisher_name);
  entry->SetString("snippet", snippet.snippet());
  entry->SetString("published",
                   TimeFormatShortDateAndTime(snippet.publish_date()));
  entry->SetString("expires",
                   TimeFormatShortDateAndTime(snippet.expiry_date()));
  entry->SetString("url", snippet.url().spec());
  entry->SetString("ampUrl", snippet.best_source().amp_url.spec());
  entry->SetString("salientImageUrl", snippet.salient_image_url().spec());
  entry->SetDouble("score", snippet.score());

  if (discarded)
    entry->SetString("id", "discarded-snippet-" + base::IntToString(index));
  else
    entry->SetString("id", "snippet-" + base::IntToString(index));

  return entry;
}

} // namespace

SnippetsInternalsMessageHandler::SnippetsInternalsMessageHandler()
    : observer_(this),
      dom_loaded_(false),
      ntp_snippets_service_(nullptr) {}

SnippetsInternalsMessageHandler::~SnippetsInternalsMessageHandler() {}

void SnippetsInternalsMessageHandler::NTPSnippetsServiceShutdown() {}

void SnippetsInternalsMessageHandler::NTPSnippetsServiceLoaded() {
  if (!dom_loaded_) return;

  SendSnippets();
  SendDiscardedSnippets();

  web_ui()->CallJavascriptFunction(
      "chrome.SnippetsInternals.receiveJson",
      base::StringValue(ntp_snippets_service_->last_json()));
}

void SnippetsInternalsMessageHandler::RegisterMessages() {
  // additional initialization (web_ui() does not work from the constructor)
  ntp_snippets_service_ = NTPSnippetsServiceFactory::GetInstance()->
      GetForProfile(Profile::FromWebUI(web_ui()));
  observer_.Add(ntp_snippets_service_);

  web_ui()->RegisterMessageCallback(
      "loaded",
      base::Bind(&SnippetsInternalsMessageHandler::HandleLoaded,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clear", base::Bind(&SnippetsInternalsMessageHandler::HandleClear,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "dump", base::Bind(&SnippetsInternalsMessageHandler::HandleDump,
                         base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "download", base::Bind(&SnippetsInternalsMessageHandler::HandleDownload,
                             base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearDiscarded",
      base::Bind(&SnippetsInternalsMessageHandler::HandleClearDiscarded,
                 base::Unretained(this)));
}

void SnippetsInternalsMessageHandler::HandleLoaded(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  dom_loaded_ = true;

  SendInitialData();
}

void SnippetsInternalsMessageHandler::HandleClear(const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  ntp_snippets_service_->ClearSnippets();
}

void SnippetsInternalsMessageHandler::HandleDump(const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();

  std::string json;
  base::JSONWriter::Write(
      *pref_service->GetList(ntp_snippets::prefs::kSnippets), &json);

  SendJson(json);
}

void SnippetsInternalsMessageHandler::HandleClearDiscarded(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());

  ntp_snippets_service_->ClearDiscardedSnippets();
  SendDiscardedSnippets();
}

void SnippetsInternalsMessageHandler::HandleDownload(
    const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  SendString("hosts-status", std::string());

  std::string hosts_string;
  args->GetString(0, &hosts_string);

  std::vector<std::string> hosts_vector = base::SplitString(
      hosts_string, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::set<std::string> hosts(hosts_vector.begin(), hosts_vector.end());

  ntp_snippets_service_->FetchSnippetsFromHosts(hosts);
}

void SnippetsInternalsMessageHandler::SendInitialData() {
  SendHosts();

  SendBoolean("flag-snippets", base::FeatureList::IsEnabled(
                                   chrome::android::kNTPSnippetsFeature));

  bool restricted = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      ntp_snippets::switches::kDontRestrict);
  SendBoolean("switch-restrict-to-hosts", restricted);
  const std::string help(restricted ? "(specify at least one host)" :
      "(unrestricted if no host is given)");
  SendString("hosts-help", help);

  SendSnippets();
  SendDiscardedSnippets();
}

void SnippetsInternalsMessageHandler::SendSnippets() {
  std::unique_ptr<base::ListValue> snippets_list(new base::ListValue);

  int index = 0;
  for (const std::unique_ptr<ntp_snippets::NTPSnippet>& snippet :
       ntp_snippets_service_->snippets())
    snippets_list->Append(PrepareSnippet(*snippet, index++, false));

  base::DictionaryValue result;
  result.Set("list", std::move(snippets_list));
  web_ui()->CallJavascriptFunction("chrome.SnippetsInternals.receiveSnippets",
                                   result);

  const std::string& status = ntp_snippets_service_->last_status();
  if (!status.empty())
    SendString("hosts-status", "Finished: " + status);
}

void SnippetsInternalsMessageHandler::SendDiscardedSnippets() {
  std::unique_ptr<base::ListValue> snippets_list(new base::ListValue);

  int index = 0;
  for (const auto& snippet : ntp_snippets_service_->discarded_snippets())
    snippets_list->Append(PrepareSnippet(*snippet, index++, true));

  base::DictionaryValue result;
  result.Set("list", std::move(snippets_list));
  web_ui()->CallJavascriptFunction(
      "chrome.SnippetsInternals.receiveDiscardedSnippets", result);
}

void SnippetsInternalsMessageHandler::SendHosts() {
  std::unique_ptr<base::ListValue> hosts_list(new base::ListValue);

  std::set<std::string> hosts = ntp_snippets_service_->GetSuggestionsHosts();

  for (const std::string& host : hosts) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    entry->SetString("url", host);

    hosts_list->Append(std::move(entry));
  }

  base::DictionaryValue result;
  result.Set("list", std::move(hosts_list));
  web_ui()->CallJavascriptFunction("chrome.SnippetsInternals.receiveHosts",
                                   result);
}

void SnippetsInternalsMessageHandler::SendJson(const std::string& json) {
  web_ui()->CallJavascriptFunction(
      "chrome.SnippetsInternals.receiveJsonToDownload",
      base::StringValue(json));
}

void SnippetsInternalsMessageHandler::SendBoolean(const std::string& name,
                                                  bool value) {
  SendString(name, value ? "True" : "False");
}

void SnippetsInternalsMessageHandler::SendString(const std::string& name,
                                                 const std::string& value) {
  base::StringValue string_name(name);
  base::StringValue string_value(value);

  web_ui()->CallJavascriptFunction("chrome.SnippetsInternals.receiveProperty",
                                   string_name, string_value);
}
