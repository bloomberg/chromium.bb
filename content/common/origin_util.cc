// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

class SchemeAndOriginWhitelist {
 public:
  SchemeAndOriginWhitelist() { Reset(); }
  ~SchemeAndOriginWhitelist() {}

  void Reset() {
    secure_schemes_.clear();
    secure_origins_.clear();
    service_worker_schemes_.clear();
    GetContentClient()->AddSecureSchemesAndOrigins(&secure_schemes_,
                                                   &secure_origins_);
    GetContentClient()->AddServiceWorkerSchemes(&service_worker_schemes_);
  }

  const std::set<std::string>& secure_schemes() const {
    return secure_schemes_;
  }
  const std::set<GURL>& secure_origins() const { return secure_origins_; }
  const std::set<std::string>& service_worker_schemes() const {
    return service_worker_schemes_;
  }

 private:
  std::set<std::string> secure_schemes_;
  std::set<GURL> secure_origins_;
  std::set<std::string> service_worker_schemes_;
  DISALLOW_COPY_AND_ASSIGN(SchemeAndOriginWhitelist);
};

base::LazyInstance<SchemeAndOriginWhitelist>::Leaky g_trustworthy_whitelist =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool IsOriginSecure(const GURL& url) {
  if (url.SchemeIsCryptographic() || url.SchemeIsFile())
    return true;

  if (url.SchemeIsFileSystem() && url.inner_url() &&
      IsOriginSecure(*url.inner_url())) {
    return true;
  }

  std::string hostname = url.HostNoBrackets();
  if (net::IsLocalhost(hostname))
    return true;

  if (base::ContainsKey(g_trustworthy_whitelist.Get().secure_schemes(),
                        url.scheme()))
    return true;

  if (base::ContainsKey(g_trustworthy_whitelist.Get().secure_origins(),
                        url.GetOrigin())) {
    return true;
  }

  return false;
}

bool OriginCanAccessServiceWorkers(const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS() && IsOriginSecure(url))
    return true;

  if (base::ContainsKey(g_trustworthy_whitelist.Get().service_worker_schemes(),
                        url.scheme())) {
    return true;
  }

  return false;
}

void ResetSchemesAndOriginsWhitelistForTesting() {
  g_trustworthy_whitelist.Get().Reset();
}

bool HasSuborigin(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.scheme() != kHttpSuboriginScheme &&
      url.scheme() != kHttpsSuboriginScheme) {
    return false;
  }

  base::StringPiece host_piece = url.host_piece();
  size_t first_period = host_piece.find('.');

  // If the first period is the first position in the hostname, or there is no
  // period at all, there is no suborigin serialized in the hostname.
  if (first_period == 0 || first_period == base::StringPiece::npos)
    return false;

  // If there's nothing after the first dot, then there is no host for the
  // physical origin, which is not a valid suborigin serialization.
  if (first_period == (host_piece.size() - 1))
    return false;

  return true;
}

std::string SuboriginFromUrl(const GURL& url) {
  if (!HasSuborigin(url))
    return "";

  std::string host = url.host();
  size_t suborigin_end = host.find(".");
  return (suborigin_end == std::string::npos) ? ""
                                              : host.substr(0, suborigin_end);
}

GURL StripSuboriginFromUrl(const GURL& url) {
  if (!HasSuborigin(url))
    return url;

  GURL::Replacements replacements;
  if (url.scheme() == kHttpSuboriginScheme) {
    replacements.SetSchemeStr(url::kHttpScheme);
  } else {
    DCHECK(url.scheme() == kHttpsSuboriginScheme);
    replacements.SetSchemeStr(url::kHttpsScheme);
  }

  std::string host = url.host();
  size_t suborigin_end = host.find(".");
  std::string new_host(
      (suborigin_end == std::string::npos)
          ? ""
          : host.substr(suborigin_end + 1,
                        url.host().length() - suborigin_end - 1));
  replacements.SetHostStr(new_host);

  return url.ReplaceComponents(replacements);
}

}  // namespace content
