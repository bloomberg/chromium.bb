// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_provider.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"

// static
const size_t AutocompleteProvider::kMaxMatches = 3;

AutocompleteProvider::AutocompleteProvider(
    AutocompleteProviderListener* listener,
    Profile* profile,
    const char* name)
    : profile_(profile),
      listener_(listener),
      done_(true),
      name_(name) {
}

void AutocompleteProvider::Stop() {
  done_ = true;
}

metrics::OmniboxEventProto_ProviderType AutocompleteProvider::
    AsOmniboxEventProviderType() const {
  if (name_ == "HistoryURL")
    return metrics::OmniboxEventProto::HISTORY_URL;
  if (name_ == "HistoryContents")
    return metrics::OmniboxEventProto::HISTORY_CONTENTS;
  if (name_ == "HistoryQuickProvider")
    return metrics::OmniboxEventProto::HISTORY_QUICK;
  if (name_ == "Search")
    return metrics::OmniboxEventProto::SEARCH;
  if (name_ == "Keyword")
    return metrics::OmniboxEventProto::KEYWORD;
  if (name_ == "Builtin")
    return metrics::OmniboxEventProto::BUILTIN;
  if (name_ == "ShortcutsProvider")
    return metrics::OmniboxEventProto::SHORTCUTS;
  if (name_ == "ExtensionApps")
    return metrics::OmniboxEventProto::EXTENSION_APPS;

  NOTREACHED();
  return metrics::OmniboxEventProto::UNKNOWN_PROVIDER;
}

void AutocompleteProvider::DeleteMatch(const AutocompleteMatch& match) {
  DLOG(WARNING) << "The AutocompleteProvider '" << name_
                << "' has not implemented DeleteMatch.";
}

void AutocompleteProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
}

string16 AutocompleteProvider::StringForURLDisplay(const GURL& url,
                                                   bool check_accept_lang,
                                                   bool trim_http) const {
  std::string languages = (check_accept_lang && profile_) ?
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) : std::string();
  return net::FormatUrl(url, languages,
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP),
      net::UnescapeRule::SPACES, NULL, NULL, NULL);
}

AutocompleteProvider::~AutocompleteProvider() {
  Stop();
}

// static
bool AutocompleteProvider::HasHTTPScheme(const string16& input) {
  std::string utf8_input(UTF16ToUTF8(input));
  url_parse::Component scheme;
  if (url_util::FindAndCompareScheme(utf8_input, chrome::kViewSourceScheme,
                                     &scheme))
    utf8_input.erase(0, scheme.end() + 1);
  return url_util::FindAndCompareScheme(utf8_input, chrome::kHttpScheme, NULL);
}

void AutocompleteProvider::UpdateStarredStateOfMatches() {
  if (matches_.empty())
    return;

  if (!profile_)
    return;

  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (!bookmark_model || !bookmark_model->IsLoaded())
    return;

  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i)
    i->starred = bookmark_model->IsBookmarked(i->destination_url);
}
