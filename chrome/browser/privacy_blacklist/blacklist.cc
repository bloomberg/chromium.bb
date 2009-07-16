// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"

#include <algorithm>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/privacy_blacklist/blacklist_store.h"
#include "chrome/common/url_constants.h"
#include "net/http/http_util.h"

#define STRINGIZE(s) #s

namespace {

const char* const cookie_headers[2] = { "cookie", "set-cookie" };

}  // namespace

// Value is not important, here just that the object has an address.
const void* const Blacklist::kRequestDataKey = 0;

unsigned int Blacklist::String2Attribute(const std::string& s) {
  if (s == STRINGIZE(kBlockAll))
    return kBlockAll;
  else if (s == STRINGIZE(kDontSendCookies))
    return kDontSendCookies;
  else if (s == STRINGIZE(kDontStoreCookies))
    return kDontStoreCookies;
  else if (s == STRINGIZE(kDontPersistCookies))
    return kDontPersistCookies;
  else if (s == STRINGIZE(kDontSendReferrer))
    return kDontSendReferrer;
  else if (s == STRINGIZE(kDontSendUserAgent))
    return kDontSendUserAgent;
  else if (s == STRINGIZE(kBlockByType))
    return kBlockByType;
  else if (s == STRINGIZE(kBlockUnsecure))
    return kBlockUnsecure;
  return 0;
}

bool Blacklist::Matches(const std::string& pattern, const std::string& url) {
  if (pattern.size() > url.size())
    return false;

  std::string::size_type p = 0;
  std::string::size_type u = 0;

  while (pattern[p] != '\0' && url[u] != '\0') {
    if (pattern[p] == '@') {
      while (pattern[++p] == '@');  // Consecutive @ are redundant.

      if (pattern[p] == '\0')
        return true;  // Nothing to match after the @.

      // Look for another wildcard to determine pattern-chunk.
      std::string::size_type tp = pattern.find_first_of("@", p);

      // If it must match until the end, compare the last characters.
      if (tp == std::string::npos) {
        std::string::size_type ur = url.size() - u;
        std::string::size_type pr = pattern.size() - p;
        return (pr <= ur) &&
            (url.compare(url.size() - pr, pr, pattern.c_str() + p) == 0);
      }

      // Else find the pattern chunk which is pattern[p:tp]
      std::string::size_type tu = url.find(pattern.c_str() + p, u, tp - p);
      if (tu == std::string::npos)
        return false;  // Pattern chunk not found.

      // Since tp is strictly greater than p, both u and p always increase.
      u = tu + tp - p;
      p = tp;
      continue;
    }

    // Match non-wildcard character.
    if (pattern[p++] != url[u++])
      return false;
  }
  return pattern[p] == '\0';
}

bool Blacklist::Entry::MatchType(const std::string& type) const {
  return std::find(types_.begin(), types_.end(), type) != types_.end();
}

bool Blacklist::Entry::IsBlocked(const GURL& url) const {
  return (attributes_ & kBlockAll) ||
    ((attributes_ & kBlockUnsecure) && !url.SchemeIsSecure());
}

Blacklist::Entry::Entry(const std::string& pattern, const Provider* provider)
    : pattern_(pattern), attributes_(0), provider_(provider) {}

void Blacklist::Entry::AddAttributes(unsigned int attributes) {
  attributes_ |= attributes;
}

void Blacklist::Entry::AddType(const std::string& type) {
  types_.push_back(type);
}

void Blacklist::Entry::Merge(const Entry& entry) {
  attributes_ |= entry.attributes_;

  std::copy(entry.types_.begin(), entry.types_.end(),
            std::back_inserter(types_));
}

void Blacklist::Entry::SwapTypes(std::vector<std::string>* types) {
  if (types && types->size()) {
    types->swap(types_);
  }
}

bool Blacklist::Match::MatchType(const std::string& type) const {
  for (std::vector<const Entry*>::const_iterator i = entries_.begin();
      i != entries_.end(); ++i) {
    if ((*i)->MatchType(type))
      return true;
  }
  return false;
}

bool Blacklist::Match::IsBlocked(const GURL& url) const {
  return (attributes_ & kBlockAll) ||
    ((attributes_ & kBlockUnsecure) && !url.SchemeIsSecure());
}

Blacklist::Match::Match() : attributes_(0) {}

void Blacklist::Match::AddEntry(const Entry* entry) {
  attributes_ |= entry->attributes();
  entries_.push_back(entry);
}

Blacklist::Blacklist(const FilePath& file) {
  // No blacklist, nothing to load.
  if (file.value().empty())
    return;

  BlacklistStoreInput input(file_util::OpenFile(file, "rb"));

  // Read the providers
  std::size_t n = input.ReadNumProviders();
  providers_.reserve(n);
  std::string name;
  std::string url;
  for (std::size_t i = 0; i < n; ++i) {
    input.ReadProvider(&name, &url);
    providers_.push_back(new Provider(name.c_str(), url.c_str()));
  }

  // Read the entries
  n = input.ReadNumEntries();
  std::string pattern;
  unsigned int attributes, provider;
  std::vector<std::string> types;
  for (unsigned int i = 0; i < n; ++i) {
    input.ReadEntry(&pattern, &attributes, &types, &provider);
    Entry* entry = new Entry(pattern, providers_[provider]);
    entry->AddAttributes(attributes);
    entry->SwapTypes(&types);
    blacklist_.push_back(entry);
  }
}

Blacklist::~Blacklist() {
  for (std::vector<Entry*>::iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i)
    delete *i;
  for (std::vector<Provider*>::iterator i = providers_.begin();
       i != providers_.end(); ++i)
    delete *i;
}

// Returns a pointer to the Blacklist-owned entry which matches the given
// URL. If no matching Entry is found, returns null.
Blacklist::Match* Blacklist::findMatch(const GURL& url) const {
  // Never match something which is not http, https or ftp.
  // TODO(idanan): Investigate if this would be an inclusion test instead of an
  // exclusion test and if there are other schemes to test for.
  if (!url.SchemeIs(chrome::kHttpScheme) &&
      !url.SchemeIs(chrome::kHttpsScheme) &&
      !url.SchemeIs(chrome::kFtpScheme))
    return 0;
  Match* match = NULL;
  for (std::vector<Entry*>::const_iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i) {
    if (Matches((*i)->pattern(), url.host()+url.path())) {
      if (!match)
        match = new Match;
      match->AddEntry(*i);
    }
  }
  return match;
}

std::string Blacklist::StripCookies(const std::string& header) {
  return net::HttpUtil::StripHeaders(header, cookie_headers, 2);
}

std::string Blacklist::StripCookieExpiry(const std::string& cookie) {
  std::string::size_type delim = cookie.find(';');
  std::string::size_type start = cookie.find("expires=", delim + 1);
  if (start != std::string::npos) {
    std::string::size_type i = start;
    // Make sure only whitespace precedes the expiry until a delimiter.
    while (cookie[--i] != ';')
      if (!IsAsciiWhitespace(cookie[i]))
        return cookie;

    std::string session_cookie(cookie, 0, i);
    std::string::size_type end = cookie.find(';', start + 1);
    if (end != std::string::npos)
      session_cookie.append(cookie.substr(end));
    return session_cookie;
  }
  return cookie;
}
