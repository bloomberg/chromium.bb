// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/identity.h"

#include "mojo/shell/query_util.h"

namespace mojo {
namespace shell {

Identity::Identity() {}

Identity::Identity(const GURL& in_url, const std::string& in_qualifier)
    : url(GetBaseURLAndQuery(in_url, nullptr)),
      qualifier(in_qualifier.empty() ? url.spec() : in_qualifier) {}

// explicit
Identity::Identity(const GURL& in_url)
    : url(GetBaseURLAndQuery(in_url, nullptr)), qualifier(url.spec()) {}

bool Identity::operator<(const Identity& other) const {
  if (url != other.url)
    return url < other.url;
  return qualifier < other.qualifier;
}

}  // namespace shell
}  // namespace mojo
