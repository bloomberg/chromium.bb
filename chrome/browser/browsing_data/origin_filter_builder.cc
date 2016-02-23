// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/origin_filter_builder.h"

#include <string>
#include <vector>

#include "base/bind.h"

namespace {

bool NoopFilter(const GURL& url) {
  return true;
}

}  // namespace

OriginFilterBuilder::OriginFilterBuilder(Mode mode)
    : mode_(mode) {
}

OriginFilterBuilder::~OriginFilterBuilder() {
}

void OriginFilterBuilder::AddOrigin(const url::Origin& origin) {
  // TODO(msramek): Optimize OriginFilterBuilder for larger filters if needed.
  DCHECK_LE(origin_list_.size(), 10U) << "OriginFilterBuilder is only suitable "
                                         "for creating small filters.";

  // By limiting the filter to non-unique origins, we can guarantee that
  // origin1 < origin2 && origin1 > origin2 <=> origin1.isSameOrigin(origin2).
  // This means that std::set::find() will use the same semantics for
  // origin comparison as Origin::IsSameOriginWith(). Furthermore, this
  // means that two filters are equal iff they are equal element-wise.
  DCHECK(!origin.unique()) << "Invalid origin passed into OriginFilter.";

  // TODO(msramek): All urls with file scheme currently map to the same
  // origin. This is currently not a problem, but if it becomes one,
  // consider recognizing the URL path.

  origin_list_.insert(origin);
}

void OriginFilterBuilder::SetMode(Mode mode) {
  mode_ = mode;
}

base::Callback<bool(const GURL&)>
    OriginFilterBuilder::BuildSameOriginFilter() const {
  std::set<url::Origin>* origins = new std::set<url::Origin>(origin_list_);
  return base::Bind(&OriginFilterBuilder::MatchesURL,
                    base::Owned(origins), mode_);
}

base::Callback<bool(const GURL&)>
    OriginFilterBuilder::BuildDomainFilter() const {
  std::set<url::Origin>* origins = new std::set<url::Origin>(origin_list_);
  return base::Bind(&OriginFilterBuilder::MatchesURLWithSubdomains,
                    base::Owned(origins), mode_);
}

// static
base::Callback<bool(const GURL&)> OriginFilterBuilder::BuildNoopFilter() {
  return base::Bind(&NoopFilter);
}

// static
bool OriginFilterBuilder::MatchesURL(
    std::set<url::Origin>* origins, Mode mode, const GURL& url) {
  return ((origins->find(url::Origin(url)) != origins->end()) ==
          (mode == WHITELIST));
}

// static
bool OriginFilterBuilder::MatchesURLWithSubdomains(
    std::set<url::Origin>* origins, Mode mode, const GURL& url) {
  if (origins->empty())
    return mode == BLACKLIST;

  // If there is no concept of subdomains, simply delegate to MatchesURL().
  if (url.HostIsIPAddress())
    return MatchesURL(origins, mode, url);

  // TODO(msramek): We do not expect filters to be particularly large.
  // If they are, replace std::set with a trie for faster searching.
  int port = url.EffectiveIntPort();
  base::StringPiece host_piece = url.host_piece();
  for (size_t i = 0; i < host_piece.length(); ++i) {
    if (i != 0 && host_piece[i - 1] != '.')
      continue;

    url::Origin origin_with_removed_subdomain =
        url::Origin::UnsafelyCreateOriginWithoutNormalization(
            url.scheme_piece(),
            host_piece.substr(i),
            port);

    // If we recognize the origin, return true for whitelist and false
    // for blacklist.
    if (origins->find(url::Origin(origin_with_removed_subdomain))
        != origins->end()) {
      return mode == WHITELIST;
    }
  }

  // We do not recognize the URL. Return false for whitelist mode and true
  // for blacklist mode.
  return mode == BLACKLIST;
}
