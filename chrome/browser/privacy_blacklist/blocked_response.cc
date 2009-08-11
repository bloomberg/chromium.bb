// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blocked_response.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/common/jstemplate_builder.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace BlockedResponse {

std::string GetHTML(const Blacklist::Match* match) {
  DictionaryValue strings;
  strings.SetString(L"title", l10n_util::GetString(IDS_BLACKLIST_TITLE));
  strings.SetString(L"message", l10n_util::GetString(IDS_BLACKLIST_MESSAGE));
  strings.SetString(L"unblock", l10n_util::GetString(IDS_BLACKLIST_UNBLOCK));

  // If kBlockAll is specified, assign blame to such an entry.
  // Otherwise pick the first one.
  const Blacklist::Entry* entry = NULL;
  if (match->attributes() & Blacklist::kBlockAll) {
    for (std::vector<const Blacklist::Entry*>::const_iterator i =
         match->entries().begin(); i != match->entries().end(); ++i) {
      if ((*i)->attributes() == Blacklist::kBlockAll) {
        entry = *i;
        break;
      }
    }
  } else {
    entry = match->entries().front();
  }
  DCHECK(entry);

  strings.SetString(L"name", entry->provider()->name());
  strings.SetString(L"url", entry->provider()->url());

  const StringPiece html =
    ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_BLACKLIST_HTML);
  return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
}

std::string GetImage(const Blacklist::Match*) {
  return ResourceBundle::GetSharedInstance().
      GetDataResource(IDR_BLACKLIST_IMAGE);
}

}  // namespace BlockedResponse
