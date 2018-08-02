// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/idn_navigation_observer.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/url_formatter/idn_spoof_checker.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"

namespace {

void RecordEvent(IdnNavigationObserver::NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(IdnNavigationObserver::kHistogramName, event);
}

}  // namespace

// static
const char IdnNavigationObserver::kHistogramName[] =
    "NavigationSuggestion.Event";

IdnNavigationObserver::IdnNavigationObserver(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

IdnNavigationObserver::~IdnNavigationObserver() {}

void IdnNavigationObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  const GURL url = load_details.entry->GetVirtualURL();
  const base::StringPiece host = url.host_piece();
  std::string matched_domain;

  url_formatter::IDNConversionResult result =
      url_formatter::IDNToUnicodeWithDetails(host);
  if (!result.has_idn_component || result.matching_top_domain.empty())
    return;

  GURL::Replacements replace_host;
  replace_host.SetHostStr(result.matching_top_domain);
  const GURL suggested_url = url.ReplaceComponents(replace_host);

  RecordEvent(NavigationSuggestionEvent::kInfobarShown);

  AlternateNavInfoBarDelegate::CreateForIDNNavigation(
      web_contents(), base::UTF8ToUTF16(result.matching_top_domain),
      suggested_url, load_details.entry->GetVirtualURL(),
      base::BindOnce(RecordEvent, NavigationSuggestionEvent::kLinkClicked));
}

// static
void IdnNavigationObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), std::make_unique<IdnNavigationObserver>(web_contents));
  }
}
