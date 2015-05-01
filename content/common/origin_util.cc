// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "content/public/common/content_client.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

namespace content {

namespace {

class SecureSchemeAndOriginSet {
 public:
  SecureSchemeAndOriginSet() { Reset(); }
  ~SecureSchemeAndOriginSet() {}

  void Reset() {
    GetContentClient()->AddSecureSchemesAndOrigins(&schemes_, &origins_);
  }

  const std::set<std::string>& schemes() const { return schemes_; }
  const std::set<GURL>& origins() const { return origins_; }

 private:
  std::set<std::string> schemes_;
  std::set<GURL> origins_;
  DISALLOW_COPY_AND_ASSIGN(SecureSchemeAndOriginSet);
};

base::LazyInstance<SecureSchemeAndOriginSet>::Leaky g_trustworthy_whitelist =
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

  if (ContainsKey(g_trustworthy_whitelist.Get().schemes(), url.scheme()))
    return true;

  if (ContainsKey(g_trustworthy_whitelist.Get().origins(), url.GetOrigin()))
    return true;

  return false;
}

void ResetSecureSchemesAndOriginsForTesting() {
  g_trustworthy_whitelist.Get().Reset();
}

}  // namespace content
