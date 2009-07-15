// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"

#include <algorithm>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/privacy_blacklist/blacklist_store.h"
#include "net/http/http_util.h"

#define STRINGIZE(s) #s

namespace {

bool matches(const std::string& pattern, const std::string& url) {
  return url.find(pattern) != std::string::npos;
}

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
const Blacklist::Entry* Blacklist::findMatch(const GURL& url) const {
  for (std::vector<Entry*>::const_iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i)
    if (matches((*i)->pattern(), url.spec()))
      return *i;
  return 0;
}

std::string Blacklist::StripCookies(const std::string& header) {
  return net::HttpUtil::StripHeaders(header, cookie_headers, 2);
}

std::string Blacklist::StripCookieExpiry(const std::string& cookie) {
  std::string::size_type start = cookie.find("; expires=");
  if (start != std::string::npos) {
    std::string::size_type finish = cookie.find(";", start+1);
    std::string session_cookie(cookie, 0, start);
    if (finish != std::string::npos)
      session_cookie.append(cookie.substr(finish));
    return session_cookie;
  }
  return cookie;
}
