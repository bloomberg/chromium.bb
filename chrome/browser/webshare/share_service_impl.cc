// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include "base/strings/strcat.h"
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

// Returns to |out| the result of running the "replace placeholders" algorithm
// on |template_string|. The algorithm is specified at
// https://wicg.github.io/web-share-target/#dfn-replace-placeholders
bool ReplacePlaceholders(base::StringPiece template_string,
                         base::StringPiece title,
                         base::StringPiece text,
                         const GURL& share_url,
                         std::string* out) {
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
  for (size_t i = 0; i < template_string.size(); ++i) {
    if (last_saw_open) {
      if (template_string[i] == '}') {
        base::StringPiece placeholder = template_string.substr(
            start_index_to_copy + 1, i - 1 - start_index_to_copy);
        auto it = placeholder_to_data.find(placeholder);
        if (it != placeholder_to_data.end()) {
          // Replace the placeholder text with the parameter value.
          split_template.push_back(it->second);
        }

        last_saw_open = false;
        start_index_to_copy = i + 1;
      } else if (!IsIdentifier(template_string[i])) {
        // Error: Non-identifier character seen after open.
        return false;
      }
    } else {
      if (template_string[i] == '}') {
        // Error: Saw close, with no corresponding open.
        return false;
      } else if (template_string[i] == '{') {
        split_template.push_back(template_string.substr(
            start_index_to_copy, i - start_index_to_copy));

        last_saw_open = true;
        start_index_to_copy = i;
      }
    }
  }
  if (last_saw_open) {
    // Error: Saw open that was never closed.
    return false;
  }
  split_template.push_back(template_string.substr(
      start_index_to_copy, template_string.size() - start_index_to_copy));

  *out = base::StrCat(split_template);
  return true;
}

}  // namespace

ShareServiceImpl::ShareServiceImpl() : weak_factory_(this) {}
ShareServiceImpl::~ShareServiceImpl() = default;

// static
void ShareServiceImpl::Create(blink::mojom::ShareServiceRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ShareServiceImpl>(),
                          std::move(request));
}

// static
bool ShareServiceImpl::ReplaceUrlPlaceholders(const GURL& url_template,
                                              base::StringPiece title,
                                              base::StringPiece text,
                                              const GURL& share_url,
                                              GURL* url_template_filled) {
  std::string new_query;
  std::string new_ref;
  if (!ReplacePlaceholders(url_template.query_piece(), title, text, share_url,
                           &new_query) ||
      !ReplacePlaceholders(url_template.ref_piece(), title, text, share_url,
                           &new_ref)) {
    return false;
  }

  // Check whether |url_template| has a query in order to preserve the '?' in a
  // URL with an empty query. e.g. http://www.google.com/?
  GURL::Replacements url_replacements;
  if (url_template.has_query())
    url_replacements.SetQueryStr(new_query);
  if (url_template.has_ref())
    url_replacements.SetRefStr(new_ref);
  *url_template_filled = url_template.ReplaceComponents(url_replacements);
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
    std::string url_template;
    share_target_dict->GetString("url_template", &url_template);

    sufficiently_engaged_targets.emplace_back(std::move(manifest_url),
                                              std::move(name),
                                              GURL(std::move(url_template)));
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

  GURL url_template_filled;
  if (!ReplaceUrlPlaceholders(result->url_template(), title, text, share_url,
                              &url_template_filled)) {
    // TODO(mgiuca): This error should not be possible at share time, because
    // targets with invalid templates should not be chooseable. Fix
    // https://crbug.com/694380 and replace this with a DCHECK.
    std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  // User should not be able to cause an invalid target URL. The replaced pieces
  // are escaped. If somehow we slip through this DCHECK, it will just open
  // about:blank.
  DCHECK(url_template_filled.is_valid());
  OpenTargetURL(url_template_filled);

  std::move(callback).Run(blink::mojom::ShareError::OK);
}
