// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
void ShareServiceImpl::Create(blink::mojom::ShareServiceRequest request) {
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
    const std::vector<std::pair<base::string16, GURL>>& targets,
    const base::Callback<void(base::Optional<std::string>)>& callback) {
// TODO(mgiuca): Get the browser window as |parent_window|.
  chrome::ShowWebShareTargetPickerDialog(nullptr /* parent_window */, targets,
                                         callback);
}

Browser* ShareServiceImpl::GetBrowser() {
  return BrowserList::GetInstance()->GetLastActive();
}

void ShareServiceImpl::OpenTargetURL(const GURL& target_url) {
  Browser* browser = GetBrowser();
  chrome::AddTabAt(browser, target_url,
                   browser->tab_strip_model()->active_index() + 1, true);
}

std::string ShareServiceImpl::GetTargetTemplate(
    const std::string& target_url,
    const base::DictionaryValue& share_targets) {
  const base::DictionaryValue* share_target_info_dict = nullptr;
  share_targets.GetDictionaryWithoutPathExpansion(target_url,
                                                  &share_target_info_dict);

  std::string url_template;
  share_target_info_dict->GetString("url_template", &url_template);
  return url_template;
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

// static
std::vector<std::pair<base::string16, GURL>>
ShareServiceImpl::GetTargetsWithSufficientEngagement(
    const base::DictionaryValue& share_targets) {
  constexpr blink::mojom::EngagementLevel kMinimumEngagementLevel =
      blink::mojom::EngagementLevel::LOW;

  std::vector<std::pair<base::string16, GURL>> sufficiently_engaged_targets;

  for (base::DictionaryValue::Iterator it(share_targets); !it.IsAtEnd();
       it.Advance()) {
    GURL manifest_url(it.key());
    if (GetEngagementLevel(manifest_url) >= kMinimumEngagementLevel) {
      const base::DictionaryValue* share_target_dict;
      bool result = it.value().GetAsDictionary(&share_target_dict);
      DCHECK(result);

      std::string name;
      share_target_dict->GetString("name", &name);

      sufficiently_engaged_targets.push_back(
          make_pair(base::UTF8ToUTF16(name), manifest_url));
    }
  }

  return sufficiently_engaged_targets;
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& share_url,
                             const ShareCallback& callback) {
  std::unique_ptr<base::DictionaryValue> share_targets;

  share_targets = GetPrefService()
                      ->GetDictionary(prefs::kWebShareVisitedTargets)
                      ->CreateDeepCopy();

  std::vector<std::pair<base::string16, GURL>> sufficiently_engaged_targets =
      GetTargetsWithSufficientEngagement(*share_targets);

  ShowPickerDialog(
      sufficiently_engaged_targets,
      base::Bind(&ShareServiceImpl::OnPickerClosed, weak_factory_.GetWeakPtr(),
                 base::Passed(&share_targets), title, text, share_url,
                 callback));
}

void ShareServiceImpl::OnPickerClosed(
    std::unique_ptr<base::DictionaryValue> share_targets,
    const std::string& title,
    const std::string& text,
    const GURL& share_url,
    const ShareCallback& callback,
    base::Optional<std::string> result) {
  if (!result.has_value()) {
    callback.Run(base::Optional<std::string>("Share was cancelled"));
    return;
  }

  std::string chosen_target = result.value();

  std::string url_template = GetTargetTemplate(chosen_target, *share_targets);
  std::string url_template_filled;
  if (!ReplacePlaceholders(url_template, title, text, share_url,
                           &url_template_filled)) {
    callback.Run(base::Optional<std::string>(
        "Error: unable to replace placeholders in url template"));
    return;
  }

  // The template is relative to the manifest URL (minus the filename).
  // Concatenate to make an absolute URL.
  base::StringPiece url_base(
      chosen_target.data(),
      chosen_target.size() - GURL(chosen_target).ExtractFileName().size());
  const GURL target(url_base.as_string() + url_template_filled);
  // User should not be able to cause an invalid target URL. Possibilities are:
  // - The base URL: can't be invalid since it's derived from the manifest URL.
  // - The template: can only be invalid if it contains a NUL character or
  //   invalid UTF-8 sequence (which it can't have).
  // - The replaced pieces: these are escaped.
  // If somehow we slip through this DCHECK, it will just open about:blank.
  DCHECK(target.is_valid());
  OpenTargetURL(target);

  callback.Run(base::nullopt);
}
