// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/identity.h"

#include "mojo/shell/query_util.h"

namespace mojo {
namespace shell {
namespace {

// It's valid to specify mojo: URLs in the filter either as mojo:foo or
// mojo://foo/ - but we store the filter in the latter form.
CapabilityFilter CanonicalizeFilter(const CapabilityFilter& filter) {
  CapabilityFilter canonicalized;
  for (CapabilityFilter::const_iterator it = filter.begin();
       it != filter.end();
       ++it) {
    if (it->first == "*")
      canonicalized[it->first] = it->second;
    else
      canonicalized[GURL(it->first).spec()] = it->second;
  }
  return canonicalized;
}

}  // namespace

Identity::Identity() {}

Identity::Identity(const GURL& url)
    : url_(GetBaseURLAndQuery(url, nullptr)),
      qualifier_(url_.spec()) {}

Identity::Identity(const GURL& url, const std::string& qualifier)
    : url_(GetBaseURLAndQuery(url, nullptr)),
      qualifier_(qualifier.empty() ? url_.spec() : qualifier) {}

Identity::Identity(const GURL& url,
                   const std::string& qualifier,
                   CapabilityFilter filter)
    : url_(GetBaseURLAndQuery(url, nullptr)),
      qualifier_(qualifier.empty() ? url_.spec() : qualifier),
      filter_(CanonicalizeFilter(filter)) {}

Identity::~Identity() {}

bool Identity::operator<(const Identity& other) const {
  // We specifically don't include filter in the equivalence check because we
  // don't quite know how this should work yet.
  // TODO(beng): figure out how it should work.
  if (url_ != other.url_)
    return url_ < other.url_;
  return qualifier_ < other.qualifier_;
}

}  // namespace shell
}  // namespace mojo
