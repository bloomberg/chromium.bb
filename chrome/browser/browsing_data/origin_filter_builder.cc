// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/origin_filter_builder.h"

#include <vector>

#include "base/bind.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

using Relation = ContentSettingsPattern::Relation;

namespace {

template<typename T> bool DontDeleteAnythingFilter(const T& data) {
  return false;
}

}  // namespace

OriginFilterBuilder::OriginFilterBuilder(Mode mode)
    : BrowsingDataFilterBuilder(mode) {
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

base::Callback<bool(const GURL&)>
    OriginFilterBuilder::BuildGeneralFilter() const {
  std::set<url::Origin>* origins = new std::set<url::Origin>(origin_list_);
  return base::Bind(&OriginFilterBuilder::MatchesURL,
                    base::Owned(origins), mode());
}

base::Callback<bool(const ContentSettingsPattern& pattern)>
    OriginFilterBuilder::BuildWebsiteSettingsPatternMatchesFilter() const {
  std::vector<ContentSettingsPattern>* patterns_from_origins =
      new std::vector<ContentSettingsPattern>();
  patterns_from_origins->reserve(origin_list_.size());

  for (const url::Origin& origin : origin_list_) {
    patterns_from_origins->push_back(
        ContentSettingsPattern::FromURLNoWildcard(GURL(origin.Serialize())));
    DCHECK(patterns_from_origins->back().IsValid());
  }

  return base::Bind(&OriginFilterBuilder::MatchesWebsiteSettingsPattern,
                    base::Owned(patterns_from_origins), mode());
}

base::Callback<bool(const net::CanonicalCookie& cookie)>
OriginFilterBuilder::BuildCookieFilter() const {
  NOTREACHED() <<
      "Origin-based deletion is not suitable for cookies. Please use "
      "different scoping, such as RegistrableDomainFilterBuilder.";
  return base::Bind(DontDeleteAnythingFilter<net::CanonicalCookie>);
}

base::Callback<bool(const std::string& channel_id_server_id)>
OriginFilterBuilder::BuildChannelIDFilter() const {
  NOTREACHED() <<
      "Origin-based deletion is not suitable for channel IDs. Please use "
      "different scoping, such as RegistrableDomainFilterBuilder.";
  return base::Bind(DontDeleteAnythingFilter<std::string>);
}

bool OriginFilterBuilder::IsEmpty() const {
  return origin_list_.empty();
}

// static
bool OriginFilterBuilder::MatchesWebsiteSettingsPattern(
    std::vector<ContentSettingsPattern>* origin_patterns,
    Mode mode,
    const ContentSettingsPattern& pattern) {
  for (const ContentSettingsPattern& origin : *origin_patterns) {
    DCHECK(origin.IsValid());
    Relation relation = pattern.Compare(origin);
    if (relation == Relation::IDENTITY)
      return mode == WHITELIST;
  }
  return mode != WHITELIST;
}

// static
bool OriginFilterBuilder::MatchesURL(
    std::set<url::Origin>* origins, Mode mode, const GURL& url) {
  return ((origins->find(url::Origin(url)) != origins->end()) ==
          (mode == WHITELIST));
}
