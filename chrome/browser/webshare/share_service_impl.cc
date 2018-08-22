// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webshare/webshare_target.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/escape.h"

ShareServiceImpl::ShareServiceImpl() : weak_factory_(this) {}
ShareServiceImpl::~ShareServiceImpl() = default;

// static
void ShareServiceImpl::Create(blink::mojom::ShareServiceRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ShareServiceImpl>(),
                          std::move(request));
}

void ShareServiceImpl::ShowPickerDialog(
    std::vector<WebShareTarget> targets,
    chrome::WebShareTargetPickerCallback callback) {
  // TODO(mgiuca): Get the browser window as |parent_window|.
  chrome::ShowWebShareTargetPickerDialog(
      nullptr /* parent_window */, std::move(targets), std::move(callback));
}

Browser* ShareServiceImpl::GetBrowser() {
  return BrowserList::GetInstance()->GetLastActive();
}

void ShareServiceImpl::OpenTargetURL(const GURL& target_url) {
  Browser* browser = GetBrowser();
  chrome::AddTabAt(browser, target_url,
                   browser->tab_strip_model()->active_index() + 1, true);
}

PrefService* ShareServiceImpl::GetPrefService() {
  return GetBrowser()->profile()->GetPrefs();
}

blink::mojom::EngagementLevel ShareServiceImpl::GetEngagementLevel(
    const GURL& url) {
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(GetBrowser()->profile());
  return site_engagement_service->GetEngagementLevel(url);
}

std::vector<WebShareTarget>
ShareServiceImpl::GetTargetsWithSufficientEngagement() {
  constexpr blink::mojom::EngagementLevel kMinimumEngagementLevel =
      blink::mojom::EngagementLevel::LOW;

  PrefService* pref_service = GetPrefService();

  const base::DictionaryValue* share_targets_dict =
      pref_service->GetDictionary(prefs::kWebShareVisitedTargets);

  std::vector<WebShareTarget> sufficiently_engaged_targets;
  for (const auto& it : *share_targets_dict) {
    GURL manifest_url(it.first);
    // This should not happen, but if the prefs file is corrupted, it might, so
    // don't (D)CHECK, just continue gracefully.
    if (!manifest_url.is_valid())
      continue;

    if (GetEngagementLevel(manifest_url) < kMinimumEngagementLevel)
      continue;

    const base::DictionaryValue* share_target_dict;
    bool result = it.second->GetAsDictionary(&share_target_dict);
    DCHECK(result);

    std::string name;
    share_target_dict->GetString("name", &name);
    std::string action;
    share_target_dict->GetString("action", &action);
    std::string text;
    share_target_dict->GetString("text", &text);
    std::string title;
    share_target_dict->GetString("title", &title);
    std::string url;
    share_target_dict->GetString("url", &url);

    sufficiently_engaged_targets.emplace_back(
        std::move(manifest_url), std::move(name), GURL(std::move(action)),
        std::move(text), std::move(title), std::move(url));
  }

  return sufficiently_engaged_targets;
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& share_url,
                             ShareCallback callback) {
  std::vector<WebShareTarget> sufficiently_engaged_targets =
      GetTargetsWithSufficientEngagement();

  ShowPickerDialog(std::move(sufficiently_engaged_targets),
                   base::BindOnce(&ShareServiceImpl::OnPickerClosed,
                                  weak_factory_.GetWeakPtr(), title, text,
                                  share_url, std::move(callback)));
}

void ShareServiceImpl::OnPickerClosed(const std::string& title,
                                      const std::string& text,
                                      const GURL& share_url,
                                      ShareCallback callback,
                                      const WebShareTarget* result) {
  if (result == nullptr) {
    std::move(callback).Run(blink::mojom::ShareError::CANCELED);
    return;
  }

  std::vector<std::pair<std::string, std::string>> entry_list;
  if (!result->title().empty() && !title.empty())
    entry_list.push_back({result->title(), title});
  if (!result->text().empty() && !text.empty())
    entry_list.push_back({result->text(), text});
  if (!result->url().empty() && share_url.is_valid())
    entry_list.push_back({result->url(), share_url.spec()});

  auto build_query_part =
      [](const std::pair<std::string, std::string>& name_value) {
        return base::StringPrintf(
            "%s=%s", net::EscapeQueryParamValue(name_value.first, true).c_str(),
            net::EscapeQueryParamValue(name_value.second, true).c_str());
      };

  std::vector<std::string> query_parts;
  std::transform(entry_list.begin(), entry_list.end(),
                 std::back_inserter(query_parts), build_query_part);

  std::string query = base::JoinString(query_parts, "&");
  url::Replacements<char> replacements;
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
  GURL url = result->action().ReplaceComponents(replacements);

  // User should not be able to cause an invalid target URL. The replaced pieces
  // are escaped. If somehow we slip through this DCHECK, it will just open
  // about:blank.
  DCHECK(url.is_valid());
  OpenTargetURL(url);

  std::move(callback).Run(blink::mojom::ShareError::OK);
}
