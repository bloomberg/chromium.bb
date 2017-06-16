// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include "base/strings/string_util.h"
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

namespace {

// Determines whether a character is allowed in a URL template placeholder.
bool IsIdentifier(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-' || c == '_';
}

}  // namespace

ShareServiceImpl::ShareServiceImpl() : weak_factory_(this) {}
ShareServiceImpl::~ShareServiceImpl() = default;

// static
void ShareServiceImpl::Create(
    const service_manager::BindSourceInfo& source_info,
    blink::mojom::ShareServiceRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<ShareServiceImpl>(),
                          std::move(request));
}

// static
bool ShareServiceImpl::ReplacePlaceholders(base::StringPiece url_template,
                                           base::StringPiece title,
                                           base::StringPiece text,
                                           const GURL& share_url,
                                           std::string* url_template_filled) {
  constexpr char kTitlePlaceholder[] = "title";
  constexpr char kTextPlaceholder[] = "text";
  constexpr char kUrlPlaceholder[] = "url";

  std::map<base::StringPiece, std::string> placeholder_to_data;
  placeholder_to_data[kTitlePlaceholder] =
      net::EscapeQueryParamValue(title, false);
  placeholder_to_data[kTextPlaceholder] =
      net::EscapeQueryParamValue(text, false);
  placeholder_to_data[kUrlPlaceholder] =
      net::EscapeQueryParamValue(share_url.spec(), false);

  std::vector<base::StringPiece> split_template;
  bool last_saw_open = false;
  size_t start_index_to_copy = 0;
  for (size_t i = 0; i < url_template.size(); ++i) {
    if (last_saw_open) {
      if (url_template[i] == '}') {
        base::StringPiece placeholder = url_template.substr(
            start_index_to_copy + 1, i - 1 - start_index_to_copy);
        auto it = placeholder_to_data.find(placeholder);
        if (it != placeholder_to_data.end()) {
          // Replace the placeholder text with the parameter value.
          split_template.push_back(it->second);
        }

        last_saw_open = false;
        start_index_to_copy = i + 1;
      } else if (!IsIdentifier(url_template[i])) {
        // Error: Non-identifier character seen after open.
        return false;
      }
    } else {
      if (url_template[i] == '}') {
        // Error: Saw close, with no corresponding open.
        return false;
      } else if (url_template[i] == '{') {
        split_template.push_back(
            url_template.substr(start_index_to_copy, i - start_index_to_copy));

        last_saw_open = true;
        start_index_to_copy = i;
      }
    }
  }
  if (last_saw_open) {
    // Error: Saw open that was never closed.
    return false;
  }
  split_template.push_back(url_template.substr(
      start_index_to_copy, url_template.size() - start_index_to_copy));

  *url_template_filled = base::JoinString(split_template, base::StringPiece());
  return true;
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

  std::unique_ptr<base::DictionaryValue> share_targets_dict =
      pref_service->GetDictionary(prefs::kWebShareVisitedTargets)
          ->CreateDeepCopy();

  std::vector<WebShareTarget> sufficiently_engaged_targets;
  for (const auto& it : *share_targets_dict) {
    GURL manifest_url(it.first);
    DCHECK(manifest_url.is_valid());

    if (GetEngagementLevel(manifest_url) < kMinimumEngagementLevel)
      continue;

    const base::DictionaryValue* share_target_dict;
    bool result = it.second->GetAsDictionary(&share_target_dict);
    DCHECK(result);

    std::string name;
    share_target_dict->GetString("name", &name);
    std::string url_template;
    share_target_dict->GetString("url_template", &url_template);

    sufficiently_engaged_targets.emplace_back(
        std::move(manifest_url), std::move(name), std::move(url_template));
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

  const GURL& chosen_target = result->manifest_url();

  std::string url_template_filled;
  if (!ReplacePlaceholders(result->url_template(), title, text, share_url,
                           &url_template_filled)) {
    // TODO(mgiuca): This error should not be possible at share time, because
    // targets with invalid templates should not be chooseable. Fix
    // https://crbug.com/694380 and replace this with a DCHECK.
    std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  // The template is relative to the manifest URL (minus the filename).
  // Concatenate to make an absolute URL.
  const std::string& chosen_target_spec = chosen_target.spec();
  base::StringPiece url_base(
      chosen_target_spec.data(),
      chosen_target_spec.size() - chosen_target.ExtractFileName().size());
  const GURL target(url_base.as_string() + url_template_filled);
  // User should not be able to cause an invalid target URL. Possibilities are:
  // - The base URL: can't be invalid since it's derived from the manifest URL.
  // - The template: can only be invalid if it contains a NUL character or
  //   invalid UTF-8 sequence (which it can't have).
  // - The replaced pieces: these are escaped.
  // If somehow we slip through this DCHECK, it will just open about:blank.
  DCHECK(target.is_valid());
  OpenTargetURL(target);

  std::move(callback).Run(blink::mojom::ShareError::OK);
}
