// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/url_resolver.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/shell/query_util.h"
#include "mojo/util/filename_util.h"
#include "url/url_util.h"

namespace mojo {
namespace shell {

URLResolver::URLResolver(const GURL& mojo_base_url)
    : mojo_base_url_(util::AddTrailingSlashIfNeeded(mojo_base_url)) {
  DCHECK(mojo_base_url_.is_valid());
  // Needed to treat first component of mojo URLs as host, not path.
  url::AddStandardScheme("mojo", url::SCHEME_WITHOUT_AUTHORITY);
  url::AddStandardScheme("exe", url::SCHEME_WITHOUT_AUTHORITY);
}

URLResolver::~URLResolver() {
}

GURL URLResolver::ResolveMojoURL(const GURL& mojo_url) const {
  if (mojo_url.SchemeIs("mojo")) {
    // It's still a mojo: URL, use the default mapping scheme.
    std::string query;
    GURL base_url = GetBaseURLAndQuery(mojo_url, &query);
    const std::string host = base_url.host();
    return mojo_base_url_.Resolve(host + "/" + host + ".mojo" + query);
  } else if (mojo_url.SchemeIs("exe")) {
#if defined OS_WIN
    std::string extension = ".exe";
#else
    std::string extension;
#endif
    std::string query;
    GURL base_url = GetBaseURLAndQuery(mojo_url, &query);
    return mojo_base_url_.Resolve(base_url.host() + extension);
  } else {
    // The mapping has produced some sort of non-mojo: URL - file:, http:, etc.
    return mojo_url;
  }
}

GURL URLResolver::ResolveMojoManifest(const GURL& mojo_url) const {
  // TODO(beng): think more about how this should be done for exe targets.
  if (mojo_url.SchemeIs("mojo")) {
    std::string host = GetBaseURLAndQuery(mojo_url, nullptr).host();
    return mojo_base_url_.Resolve(host +
                                  "/manifest.json");
  } else if (mojo_url.SchemeIs("exe")) {
    return mojo_base_url_.Resolve(GetBaseURLAndQuery(mojo_url, nullptr).host() +
                                  "_manifest.json");
  }
  return GURL();
}

}  // namespace shell
}  // namespace mojo
