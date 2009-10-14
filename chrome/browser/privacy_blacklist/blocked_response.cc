// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blocked_response.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

unsigned long Hash(std::set<std::string>::iterator i) {
  return (unsigned long)(i.operator->());
}

std::string Dehash(unsigned long l) {
  return *(std::string*)l;
}

}  // namespace

namespace chrome {

const char kUnblockScheme[] = "chrome-unblock";

const char kBlockScheme[] = "chrome-block";

std::string BlockedResponse::GetHTML(const std::string& url,
                                     const Blacklist::Match* match) {
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
  strings.SetString(L"bypass", GetUnblockedURL(url));

  const base::StringPiece html =
    ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_BLACKLIST_HTML);
  return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
}

std::string BlockedResponse::GetImage(const Blacklist::Match*) {
  return ResourceBundle::GetSharedInstance().
      GetDataResource(IDR_BLACKLIST_IMAGE);
}

std::string BlockedResponse::GetHeaders(const std::string& url) {
  return
      "HTTP/1.1 200 OK\nContent-Type: text/html\nlocation: "
      + GetBlockedURL(url) + "\n" + "Cache-Control: no-store";
}

std::string BlockedResponse::GetBlockedURL(const std::string& url) {
  return std::string(kBlockScheme) + "://" + url;
}

std::string BlockedResponse::GetUnblockedURL(const std::string& url) {
  std::set<std::string>::iterator i = blocked_.insert(blocked_.end(), url);

  char buf[64];
  base::snprintf(buf, 64, "%s://%lX", kUnblockScheme, Hash(i));
  return  buf;
}

std::string BlockedResponse::GetOriginalURL(const std::string& url) {
  unsigned long l = 0;

  // Read the address of the url.
  if (sscanf(url.c_str() + sizeof(kUnblockScheme) + 2, "%lX", &l)) {
    std::size_t p = url.find('/', sizeof(kUnblockScheme) + 2);
    if (p != std::string::npos)
      return Dehash(l) + url.substr(p+1);
    return Dehash(l);
  }
  return chrome::kAboutBlankURL;
}

}  // namespace chrome
