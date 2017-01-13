// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/escape.h"

namespace {

// Determines whether a character is allowed in a URL template placeholder.
bool IsIdentifier(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-' || c == '_';
}

// Joins a std::vector<base::StringPiece> into a single std::string.
// TODO(constantina): Implement a base::JoinString() that takes StringPieces.
// i.e. move this to base/strings/string_util.h, and thoroughly test.
std::string JoinString(const std::vector<base::StringPiece>& pieces) {
  size_t total_size = 0;
  for (const auto& piece : pieces) {
    total_size += piece.size();
  }
  std::string joined_pieces;
  joined_pieces.reserve(total_size);

  for (const auto& piece : pieces) {
    piece.AppendToString(&joined_pieces);
  }
  return joined_pieces;
}

}  // namespace

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

  *url_template_filled = JoinString(split_template);
  return true;
}

void ShareServiceImpl::OpenTargetURL(const GURL& target_url) {
// TODO(constantina): Prevent this code from being run/compiled in android.
#if defined(OS_LINUX) || defined(OS_WIN)
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  chrome::AddTabAt(browser, target_url,
                   browser->tab_strip_model()->active_index() + 1, true);
#endif
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& share_url,
                             const ShareCallback& callback) {
  // TODO(constantina): replace hard-coded URL with one from user-chosen site.
  constexpr char kUrlBase[] = "https://wicg.github.io/web-share-target/";
  constexpr char kUrlTemplate[] =
      "demos/sharetarget.html?title={title}&text={text}&url={url}";

  std::string url_template_filled;
  if (!ReplacePlaceholders(kUrlTemplate, title, text, share_url,
                           &url_template_filled)) {
    callback.Run(base::Optional<std::string>(
        "Error: unable to replace placeholders in url template"));
    return;
  }

  GURL target_url(kUrlBase + url_template_filled);
  if (!target_url.is_valid()) {
    callback.Run(base::Optional<std::string>(
        "Error: url of share target is not a valid url."));
    return;
  }
  OpenTargetURL(target_url);

  callback.Run(base::nullopt);
}
