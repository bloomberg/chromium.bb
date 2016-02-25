// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/identity.h"

#include "mojo/shell/public/interfaces/shell.mojom.h"

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
    : Identity(url, url.spec(), mojom::Connector::kUserRoot) {}

Identity::Identity(const GURL& url, const std::string& qualifier,
                   uint32_t user_id)
    : url_(url),
      qualifier_(qualifier.empty() ? url_.spec() : qualifier),
      user_id_(user_id) {}

Identity::~Identity() {}

bool Identity::operator<(const Identity& other) const {
  // We specifically don't include filter in the equivalence check because we
  // don't quite know how this should work yet.
  // TODO(beng): figure out how it should work.
  if (url_ != other.url_)
    return url_ < other.url_;
  if (qualifier_ != other.qualifier_)
    return qualifier_ < other.qualifier_;
  return user_id_ < other.user_id_;
}

bool Identity::operator==(const Identity& other) const {
  // We specifically don't include filter in the equivalence check because we
  // don't quite know how this should work yet.
  // TODO(beng): figure out how it should work.
  return other.url_ == url_ && other.qualifier_ == qualifier_ &&
         other.user_id_ == user_id_;
}

void Identity::SetFilter(const CapabilityFilter& filter) {
  filter_ = CanonicalizeFilter(filter);
}

Identity CreateShellIdentity() {
  Identity id =
      Identity(GURL("mojo://shell/"), "", mojom::Connector::kUserRoot);
  id.SetFilter(GetPermissiveCapabilityFilter());
  return id;
}

}  // namespace shell
}  // namespace mojo
